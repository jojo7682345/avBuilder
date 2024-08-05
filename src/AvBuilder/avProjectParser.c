#include "avBuilder.h"
#define AV_DYNAMIC_ARRAY_EXPOSE_MEMORY_LAYOUT
#include <AvUtils/dataStructures/avDynamicArray.h>
#include <AvUtils/string/avChar.h>
#include <AvUtils/avMath.h>
#include <AvUtils/filesystem/avFile.h>
#include <AvUtils/avMemory.h>
#include <AvUtils/logging/avAssert.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "avProjectLang.h"

enum IteratorStatus {
    ITERATOR_STATUS_OK = 0,
    ITERATOR_STATUS_ERROR = 1,
};

typedef struct TokenIterator {
    Token* tokens;
    uint64 tokenCount;
    uint64 current;
    AvAllocator* allocator;
    enum IteratorStatus status;
} TokenIterator;

static Token* previous(TokenIterator* iterator){
    return iterator->tokens + (iterator->current-1);
}

static bool32 isAtEnd(TokenIterator* iterator){
    return (iterator->current >= iterator->tokenCount);
}

static Token* peek(TokenIterator* iterator){
    return iterator->tokens + iterator->current;
}

static Token* advance(TokenIterator* iterator){
    if(!isAtEnd(iterator)){
        iterator->current++;
    }
    return previous(iterator);
}

static void recede(TokenIterator* iterator){
    if(iterator->current!=0){
        (iterator->current)--;
    }
}

static bool32 check(TokenIterator* iterator, TokenType type){
    if(isAtEnd(iterator)) { return false; }
    return peek(iterator)->type == type;
}

#define match(iterator,...) match_(iterator,__VA_ARGS__,TOKEN_TYPE_NONE)
static bool32 match_(TokenIterator* iterator, ...){
    va_list args;
    va_start(args, iterator);
    TokenType type;
    while((type = va_arg(args, TokenType))!=TOKEN_TYPE_NONE){
        if(check(iterator, type)){
            advance(iterator);
            va_end(args);
            return true;
        }
    }
    va_end(args);
    return false;
}

#define TOKEN(t, token, symbol) if(type==TOKEN_TYPE_##t##_##token) return AV_CSTR("TOKEN_TYPE_"#t"_"#token); 
static AvString tokenTypeToString(TokenType type){
    LIST_OF_TOKENS
    if(type==TOKEN_TYPE_TEXT) return AV_CSTR("TOKEN_TYPE_TEXT");
    if(type==TOKEN_TYPE_NUMBER) return AV_CSTR("TOKEN_TYPE_NUMBER");
    if(type==TOKEN_TYPE_STRING) return AV_CSTR("TOKEN_TYPE_STRING");
    return AV_CSTR("UNKNOWN");
}
#undef TOKEN


static void logParserError(TokenIterator* iterator, TokenType type, AvString str){
    Token* token = peek(iterator);
    iterator->status |= ITERATOR_STATUS_ERROR;
    avStringPrintf(
        AV_CSTR("Unexpected token at line %i\n found %s but expected %s.\n%s\n"), 
        token->line,
        tokenTypeToString(token->type), // TODO: convert token types to string
        tokenTypeToString(type), // TODO: convert token types to string
        str
    );
}

static Token* consume(TokenIterator* iterator, TokenType type, const char* message){
    if(check(iterator,type)) { return advance(iterator); }
    logParserError(iterator, type, AV_CSTR(message));
    avAssert(false, "error");
    return nullptr;
}

static struct Expression* parseExpression(TokenIterator* iterator);

static struct Primary* parsePrimary(TokenIterator* iterator){
    struct Primary* primary = nullptr;
    primary = avAllocatorAllocate(sizeof(struct Primary), iterator->allocator);
    if(match(iterator, TOKEN_TYPE_STRING)){
        primary->type = PRIMARY_TYPE_LITERAL;
        memcpy(&(primary->literal),&(previous(iterator)->str),sizeof(AvString));
        return primary;
    }
    if(match(iterator, TOKEN_TYPE_TEXT)){
        primary->type = PRIMARY_TYPE_IDENTIFIER;
        memcpy(&(primary->identifier),&(previous(iterator)->str), sizeof(AvString));
        return primary;
    }
    if(match(iterator, TOKEN_TYPE_NUMBER)){
        primary->type = PRIMARY_TYPE_NUMBER;
        memcpy(&(primary->number), &(previous(iterator)->str),sizeof(AvString));
        return primary;
    }
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_open)){
        struct Expression* expression = parseExpression(iterator);
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_close, "Expect ')' after expression.");
        primary->type = PRIMARY_TYPE_GROUPING;
        primary->grouping = expression;
        return primary;
    }
    logParserError(iterator, TOKEN_TYPE_STRING,AV_CSTR("Literal expected"));
    return nullptr;
}

static struct Argument* parseArgument(TokenIterator* iterator){
    struct Argument* arguments = avAllocatorAllocate(sizeof(struct Argument), iterator->allocator);
    struct Argument* args = arguments;
    while(true){
        if(!check(iterator, TOKEN_TYPE_PUNCTUATOR_comma) && !check(iterator,TOKEN_TYPE_PUNCTUATOR_parenthese_close)){
            args->expression = parseExpression(iterator);
        }
        if(match(iterator, TOKEN_TYPE_PUNCTUATOR_comma)){
            struct Argument* next = avAllocatorAllocate(sizeof(struct Argument), iterator->allocator);
            args->next = next;
            args = next;
            continue;
        }
        if(check(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_close)){
            break;
        }
        break;
    }
    return arguments;
}

static struct Call* parseCall(TokenIterator* iterator){
    struct Call* call = avAllocatorAllocate(sizeof(struct Call), iterator->allocator);
    call->function = parsePrimary(iterator);
    
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_open)){
        call->argument = parseArgument(iterator);
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_close, "Expected ')' after argument.");
    }
    return call;
}
static struct Summation* parseSummation(TokenIterator* iterator);

static struct Array* parseArray(TokenIterator* iterator){
    if(check(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close)){
        return nullptr;
    }
    struct Array* array = avAllocatorAllocate(sizeof(struct Array), iterator->allocator);
    struct Array* arr = array;
    while(true){
        arr->expression = parseSummation(iterator);
        if(match(iterator, TOKEN_TYPE_PUNCTUATOR_comma)){
            if(check(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close)){
                break;
            }
            struct Array* next = avAllocatorAllocate(sizeof(struct Array), iterator->allocator);
            arr->next = next;
            arr = next;
            continue;
        }
        if(check(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close)){
            break;
        }
        break;
    }
    return array;
}

static struct ArrayList* parseArrayList(TokenIterator* iterator){
    struct ArrayList* arrayList = avAllocatorAllocate(sizeof(struct ArrayList), iterator->allocator);
    struct ArrayList* array = arrayList;
    while(match(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_open)){
        array->array = parseArray(iterator);
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close, "Expected ']' after expression.");
        if(check(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_open)){
            array->next = avAllocatorAllocate(sizeof(struct ArrayList), iterator->allocator);
            array = array->next;
        }
    }
    return arrayList;
}

static struct Filter* parseFilter(TokenIterator* iterator){
    struct Filter* filter = avAllocatorAllocate(sizeof(struct Filter), iterator->allocator);
    filter->call = parseCall(iterator);
    if(check(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_open)){
        filter->filter = parseArrayList(iterator);
    }
    return filter;
}

static struct Unary* parseUnary(TokenIterator* iterator){
    struct Unary* unary = avAllocatorAllocate(sizeof(struct Unary), iterator->allocator);
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_minus)){
        Token* operator = previous(iterator);
        (void)operator; // NOTE: when multiple operators added, find the operator using this
        unary->operator = UNARY_OPERATOR_MINUS;
        unary->type = UNARY_TYPE_UNARY;
        unary->unary = parseUnary(iterator);
        return unary;
    }
    
    unary->filter = parseFilter(iterator);
    return unary;
}

static struct Enumeration* parseEnumeration(TokenIterator* iterator){
    struct Enumeration* enumeration = avAllocatorAllocate(sizeof(struct Enumeration), iterator->allocator);
    if(match(iterator, TOKEN_TYPE_KEYWORD_files)){
        consume(iterator, TOKEN_TYPE_KEYWORD_in, "Expected keyword 'in' after keyword 'files'");
        enumeration->type = ENUMERATION_TYPE_ENUMERATION;
    }
    enumeration->unary = parseUnary(iterator);
    if(match(iterator, TOKEN_TYPE_KEYWORD_recursive) && enumeration->type == ENUMERATION_TYPE_ENUMERATION){
        enumeration->recursive = true;
    }
    return enumeration;
}

static struct Multiplication* parseMultiplication(TokenIterator* iterator){
    struct Multiplication* multiplication = avAllocatorAllocate(sizeof(struct Summation), iterator->allocator);
    multiplication->left = parseEnumeration(iterator);
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_star, TOKEN_TYPE_PUNCTUATOR_divide)){
        switch(previous(iterator)->type){
            case TOKEN_TYPE_PUNCTUATOR_star:
                multiplication->operator = MULTIPLICATION_OPERATOR_MULTIPLY;
            break;
            case TOKEN_TYPE_PUNCTUATOR_divide:
                multiplication->operator = MULTIPLICATION_OPERATOR_DIVIDE;
            break;
            default:
                avAssert(false, "shoud not reach here");
                break;
        }
        multiplication->right = parseMultiplication(iterator);
    }

    return multiplication;
}

static struct Summation* parseSummation(TokenIterator* iterator){
    struct Summation* summation = avAllocatorAllocate(sizeof(struct Summation), iterator->allocator);
    summation->left = parseMultiplication(iterator);
    
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_plus, TOKEN_TYPE_PUNCTUATOR_minus)){
        switch(previous(iterator)->type){
            case TOKEN_TYPE_PUNCTUATOR_plus:
                summation->operator = SUMMATION_OPERATOR_ADD;
            break;
            case TOKEN_TYPE_PUNCTUATOR_minus:
                summation->operator = SUMMATION_OPERATOR_SUBTRACT;
            break;
            default:
                avAssert(false, "shoud not reach here");
                break;
        }
        summation->right = parseSummation(iterator);
    }

    return summation;
}


static struct Expression* parseExpression(TokenIterator* iterator){
    struct Expression* expression = avAllocatorAllocate(sizeof(struct Expression), iterator->allocator);
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_open)){
        expression->array = parseArray(iterator);
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close, "expected ']'");
    }else{
        expression->array = avAllocatorAllocate(sizeof(struct Array), iterator->allocator);
        expression->array->expression = parseSummation(iterator);
    }
    return expression;
}

static struct FunctionCallStatement* parseFunctionCallStatement(TokenIterator* iterator){
    struct FunctionCallStatement* functionCall = avAllocatorAllocate(sizeof(struct FunctionCallStatement), iterator->allocator);
    functionCall->call = parseCall(iterator);
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_semicolon, "expected ';' after statement");
    return functionCall;
}

static struct Variable* parseVariable(TokenIterator* iterator){
    struct Variable* var = avAllocatorAllocate(sizeof(struct Variable), iterator->allocator);

    Token* variableName = consume(iterator, TOKEN_TYPE_TEXT, "expected variable name");
    memcpy(&(var->name),&(variableName->str), sizeof(AvString));

    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_open)){
        var->modifier = VARIABLE_ACCESS_MODIFIER_ARRAY;
        var->index = parseExpression(iterator);
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close, "expected ']' after array index");
    }

    return var;
}

static struct VariableAssignment* parseVariableAssignment(TokenIterator* iterator){
    struct VariableAssignment* var = avAllocatorAllocate(sizeof(struct VariableAssignment),iterator->allocator);
    var->variable = parseVariable(iterator);
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_equals, "expected '='");
    var->expression = parseExpression(iterator);
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_semicolon, "expected ';' after statement");
    return var;
}

static struct Parameter* parseParameter(TokenIterator* iterator){
    struct Parameter* param = avAllocatorAllocate(sizeof(struct Parameter), iterator->allocator);
    memcpy(&(param->name), &(consume(iterator, TOKEN_TYPE_TEXT, "expect parameter name")->str), sizeof(AvString));
    return param;
}

static struct ParameterList* parseParameterList(TokenIterator* iterator){
    if(check(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_close)){
        return nullptr;
    }
    struct ParameterList* list = avAllocatorAllocate(sizeof(struct ParameterList), iterator->allocator);
    struct ParameterList* l = list;
    do{
        l->parameter = parseParameter(iterator);
        l->next = avAllocatorAllocate(sizeof(struct ParameterList), iterator->allocator);
        l = l->next;
    }while(match(iterator, TOKEN_TYPE_PUNCTUATOR_comma));
    return list;
}

static struct CommandStatement* parseCommandStatement(TokenIterator* iterator){
    struct CommandStatement* stat = avAllocatorAllocate(sizeof(struct CommandStatement), iterator->allocator);

    if(match(iterator, TOKEN_TYPE_TEXT)){
        if(check(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_open)){
            stat->type = COMMAND_STATEMENT_FUNCTION_CALL;
            recede(iterator);
            stat->functionCall = parseFunctionCallStatement(iterator);
            return stat;
        }
        if(check(iterator, TOKEN_TYPE_PUNCTUATOR_equals) || check(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_open)){
            stat->type = COMMAND_STATEMENT_VARIABLE_ASSIGNMENT;
            recede(iterator);
            stat->variableAssignment = parseVariableAssignment(iterator);
            return stat;
        }
    }
    if(match(iterator, TOKEN_TYPE_KEYWORD_command)){
        struct VariableAssignment* var = avAllocatorAllocate(sizeof(struct VariableAssignment),iterator->allocator);
        var->variable = avAllocatorAllocate(sizeof(struct Variable), iterator->allocator);
        memcpy(&var->variable->name, &keywords[(TOKEN_TYPE_KEYWORD_command>>5)-1], sizeof(AvString));
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_equals, "expected '='");
        var->expression = parseExpression(iterator);
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_semicolon, "expected ';' after statement");
        stat->variableAssignment = var;
        stat->type = COMMAND_STATEMENT_VARIABLE_ASSIGNMENT;
        return stat;
    }
    logParserError(iterator, TOKEN_TYPE_TEXT, AV_CSTR("expected valid statement"));
    return nullptr;
}

static struct CommandStatementList* parseCommandStatementList(TokenIterator* iterator){
    if(check(iterator, TOKEN_TYPE_PUNCTUATOR_brace_close)){
        return nullptr;
    }
    struct CommandStatementList* list = avAllocatorAllocate(sizeof(struct CommandStatementList), iterator->allocator);
    struct CommandStatementList* l = list;
    do{
        l->commandStatement = parseCommandStatement(iterator);
        if(!l->commandStatement){
            break;
        }
        l->next = avAllocatorAllocate(sizeof(struct CommandStatementList), iterator->allocator);
        l = l->next;
    }while(!check(iterator, TOKEN_TYPE_PUNCTUATOR_brace_close));
    return list;
}

static struct VariableDefinitionStatement* parseVariableDefinition(TokenIterator* iterator){
    struct VariableDefinitionStatement* stat = avAllocatorAllocate(sizeof(struct VariableDefinitionStatement), iterator->allocator);
    Token* identifier = consume(iterator, TOKEN_TYPE_TEXT, "expected identifier");
    memcpy(&(stat->identifier), &identifier->str, sizeof(AvString));
    stat->size = nullptr;
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_open)){
        stat->size = parseExpression(iterator);
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close, "expected ']' after array definition");
    }
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_semicolon, "expected ';' after expression");
    return stat;
}

static struct PerformOperation* parsePerformOperation(TokenIterator* iterator) {
    struct PerformOperation* operation = avAllocatorAllocate(sizeof(struct PerformOperation), iterator->allocator);
    if(match(iterator, TOKEN_TYPE_KEYWORD_command)){
        operation->type = PERFORM_OPERATION_TYPE_COMMAND;
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_brace_open, "expected body");
        operation->commandStatementList = parseCommandStatementList(iterator);
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_brace_close, "expected end of body");
        return operation;
    }
    if(match(iterator, TOKEN_TYPE_TEXT)){
        if(check(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_open)){
            recede(iterator);
            operation->type = PERFORM_OPERATION_TYPE_FUNCTION_CALL;
            operation->functionCall = parseFunctionCallStatement(iterator);
            return operation;
        }
        recede(iterator);
        operation->type = PERFORM_OPERATION_TYPE_VARIABLE_ASSIGNMENT;
        operation->variableAssignment = parseVariableAssignment(iterator);
        return operation;
    }
    if(match(iterator, TOKEN_TYPE_KEYWORD_var)){
        operation->varStatement = parseVariableDefinition(iterator);
        operation->type = PERFORM_OPERATION_TYPE_VARIABLE_DEFINITION;
        return operation;
    }
    logParserError(iterator, TOKEN_TYPE_TEXT, AV_CSTR("expected valid statement"));
    return nullptr;
}

static struct PerformOperationList* parsePerformOperationList(TokenIterator* iterator){
    if(check(iterator, TOKEN_TYPE_PUNCTUATOR_brace_close)){
        return nullptr;
    }
    struct PerformOperationList* list = avAllocatorAllocate(sizeof(struct ParameterList), iterator->allocator);
    struct PerformOperationList* l = list;
    do{
        l->performOperation = parsePerformOperation(iterator);
        if(l->performOperation==nullptr){
            break;
        }
        l->next = avAllocatorAllocate(sizeof(struct PerformOperationList), iterator->allocator);
        l = l->next;

    }while(!check(iterator, TOKEN_TYPE_PUNCTUATOR_brace_close));
    return list;
}

static struct PerformStatement* parsePerformStatement(TokenIterator* iterator){
    struct PerformStatement* stat = avAllocatorAllocate(sizeof(struct PerformStatement), iterator->allocator);
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_brace_open)){
        stat->performOperationList = parsePerformOperationList(iterator);
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_brace_close, "exected '}'");
    }else{
        stat->performOperationList = avAllocatorAllocate(sizeof(struct PerformOperationList), iterator->allocator);
        stat->performOperationList->performOperation = parsePerformOperation(iterator);
    }
    return stat;
}

static struct ForeachStatement* parseForeachStatement(TokenIterator* iterator){
    struct ForeachStatement* stat = avAllocatorAllocate(sizeof(struct ForeachStatement), iterator->allocator);
    memcpy(&(stat->variable), &(consume(iterator, TOKEN_TYPE_TEXT, "expected variable name")->str), sizeof(AvString));
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_open)){
        memcpy(&(stat->index), &(consume(iterator, TOKEN_TYPE_TEXT, "expected variable name")->str), sizeof(AvString));
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close, "expected ']'");
    }
    consume(iterator, TOKEN_TYPE_KEYWORD_from, "expected keyword 'from'");
    stat->collection = parseExpression(iterator);
    consume(iterator, TOKEN_TYPE_KEYWORD_perform, "expected keyword 'perform'");
    stat->performStatement = parsePerformStatement(iterator);
    return stat;
}

static struct ReturnStatement* parseReturnStatement(TokenIterator* iterator){
    struct ReturnStatement* ret = avAllocatorAllocate(sizeof(struct ReturnStatement), iterator->allocator);
    ret->value = parseExpression(iterator);
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_semicolon, "expected ';' after expression");
    return ret;
}

static struct FunctionStatement* parseFunctionStatement(TokenIterator* iterator){
    struct FunctionStatement* statement = avAllocatorAllocate(sizeof(struct FunctionStatement), iterator->allocator);    
    if(match(iterator, TOKEN_TYPE_KEYWORD_perform)){
        statement->performStatement = parsePerformStatement(iterator);
        statement->type = FUNCTION_STATEMENT_TYPE_PERFORM;
        return statement;
    }
    if(match(iterator, TOKEN_TYPE_KEYWORD_foreach)){
        statement->foreachStatement = parseForeachStatement(iterator);
        statement->type = FUNCTION_STATEMENT_TYPE_FOREACH;
        return statement;
    }
    if(match(iterator, TOKEN_TYPE_KEYWORD_return)){
        statement->returnStatement = parseReturnStatement(iterator);
        statement->type = FUNCTION_STATEMENT_TYPE_RETURN;
        return statement;
    }
    if(match(iterator, TOKEN_TYPE_KEYWORD_var)){
        statement->varStatement = parseVariableDefinition(iterator);
        statement->type = FUNCTION_STATEMENT_TYPE_VAR_DEFINITION;
        return statement;
    }
    return nullptr;
}

static struct FunctionStatementList* parseFunctionStatementList(TokenIterator* iterator){
    if(check(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_close)){
        return nullptr;
    }
    struct FunctionStatementList* list = avAllocatorAllocate(sizeof(struct FunctionStatementList), iterator->allocator);
    struct FunctionStatementList* l = list;
    do{
        l->functionStatement = parseFunctionStatement(iterator);
        if(l->functionStatement==nullptr){
            break;
        }
        l->next = avAllocatorAllocate(sizeof(struct FunctionStatementList), iterator->allocator);
        l = l->next;
    }while(!check(iterator, TOKEN_TYPE_PUNCTUATOR_brace_close));
    return list;
}

static struct FunctionDefinition* parseFunctionDefinition(TokenIterator* iterator){
    struct FunctionDefinition* def = avAllocatorAllocate(sizeof(struct FunctionDefinition), iterator->allocator);
    Token* functionName = consume(iterator, TOKEN_TYPE_TEXT, "this should be checked before entering function");
    memcpy(&(def->name), &(functionName->str), sizeof(AvString));
    
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_open, "this should also be checked before entering");
    def->parameterList = parseParameterList(iterator);
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_close, "expected ')'");

    consume(iterator, TOKEN_TYPE_PUNCTUATOR_brace_open, "expected function body");
    def->functionStatementList = parseFunctionStatementList(iterator);
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_brace_close, "expected '}'");

    return def;
}

static struct DefinitionMapping* parseDefinitionMapping(TokenIterator* iterator){
    struct DefinitionMapping* mapping = avAllocatorAllocate(sizeof(struct DefinitionMapping), iterator->allocator);
    Token* symbol = consume(iterator, TOKEN_TYPE_TEXT, "expected symbol name");
    memcpy(&(mapping->symbol), &(symbol->str),sizeof(AvString));
    if(match(iterator, TOKEN_TYPE_KEYWORD_as)){
        Token* alias = consume(iterator, TOKEN_TYPE_TEXT, "expected alias");
        memcpy(&(mapping->symbolAlias),&(alias->str),sizeof(AvString));
    }
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_semicolon, "expected ';' after statement");
    return mapping;
}

static struct DefinitionMappingList* parseDefinitionMappingList(TokenIterator* iterator){
    struct DefinitionMappingList* definitionMapping = avAllocatorAllocate(sizeof(struct DefinitionMappingList), iterator->allocator);
    struct DefinitionMappingList* list = definitionMapping;
    while(!check(iterator, TOKEN_TYPE_PUNCTUATOR_brace_close)){
        list->definitionMapping = parseDefinitionMapping(iterator);
        list->next = avAllocatorAllocate(sizeof(struct DefinitionMappingList), iterator->allocator);
        list = list->next;
    }
    return definitionMapping;
}

static struct ImportStatement* parseImportStatement(TokenIterator* iterator){
    consume(iterator, TOKEN_TYPE_KEYWORD_import, "this should never trigger");
    Token* fileName = consume(iterator, TOKEN_TYPE_STRING, "file name expected");
    struct ImportStatement* import = avAllocatorAllocate(sizeof(struct ImportStatement), iterator->allocator);
    memcpy(&(import->file), &(fileName->str), sizeof(AvString));

    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_brace_open)){
        import->definitionMappingList = parseDefinitionMappingList(iterator);
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_brace_close, "expected '}' after statement");
    }else{
        import->definitionMappingList = avAllocatorAllocate(sizeof(struct DefinitionMappingList), iterator->allocator);
        import->definitionMappingList->definitionMapping = parseDefinitionMapping(iterator);
    }

    return import;
}

static struct ProjectStatement* parseProjectStatement(TokenIterator* iterator){
    struct ProjectStatement* stat = avAllocatorAllocate(sizeof(struct ProjectStatement), iterator->allocator);
    
    if(check(iterator, TOKEN_TYPE_KEYWORD_import)){
        stat->type = PROJECT_STATEMENT_TYPE_INCLUDE;
        stat->importStatement = parseImportStatement(iterator);
        return stat;
    }

    if(match(iterator, TOKEN_TYPE_TEXT)){
        if(check(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_open)){
            stat->type = PROJECT_STATEMENT_TYPE_FUNCTION_DEFINITION;
            recede(iterator);
            stat->functionDefinition = parseFunctionDefinition(iterator);
            return stat;
        }
        if(check(iterator, TOKEN_TYPE_PUNCTUATOR_equals)){
            stat->type = PROJECT_STATEMENT_TYPE_VARIABLE_ASSIGNMENT;
            recede(iterator);
            stat->variableAssignment = parseVariableAssignment(iterator);
            return stat;
        }
    }
    logParserError(iterator, TOKEN_TYPE_TEXT, AV_CSTR("expected valid statement"));
    return nullptr;
}

static struct ProjectStatementList* parseProjectStatementList(TokenIterator* iterator){
    struct ProjectStatementList* list = avAllocatorAllocate(sizeof(struct ProjectStatementList), iterator->allocator);
    struct ProjectStatementList* l = list;
    while(!isAtEnd(iterator)){
        l->statement = parseProjectStatement(iterator);
        if(l->statement==nullptr){
            break;
        }
        l->next = avAllocatorAllocate(sizeof(struct ProjectStatementList), iterator->allocator);
        l = l->next;
    }
    return list;
}

bool32 parseProject(AV_DS(AvDynamicArray, Token) tokenList, void** statements, Project* project){
    uint64 tokenCount = avDynamicArrayGetSize(tokenList);
    Token* tokens = avCallocate(tokenCount, sizeof(Token), "allocating tokens");
    avDynamicArrayReadRange(tokens, tokenCount, 0, sizeof(Token), 0, tokenList);
    TokenIterator iterator = {
        .allocator=  &project->allocator,
        .current = 0,
        .tokenCount = tokenCount,
        .tokens = tokens,
        .status = 0,
    };
    *statements = parseProjectStatementList(&iterator);
    avFree(tokens);
    return iterator.status==ITERATOR_STATUS_OK;    
}




