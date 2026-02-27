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
    AvString projectFile;
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
        AV_CSTR("Unexpected token at %S:%i\n\tfound %S but expected %S.\n%S\n"), 
        iterator->projectFile,
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

static struct Expression_S parseExpression(TokenIterator* iterator);
static struct Expression_S parseArray(TokenIterator* iterator);

static struct Expression_S parsePrimary(TokenIterator* iterator){
    struct Expression_S expr = {0};
    
    if(match(iterator, TOKEN_TYPE_STRING)){
        expr.type = EXPRESSION_TYPE_LITERAL;
        avMemcpy(&expr.literal, &(previous(iterator)->str), sizeof(AvString));
        return expr;
    }
    if(match(iterator, TOKEN_TYPE_TEXT)){
        expr.type = EXPRESSION_TYPE_IDENTIFIER;
        memcpy(&(expr.identifier.identifier),&(previous(iterator)->str), sizeof(AvString));
        return expr;
    }
    if(match(iterator, TOKEN_TYPE_NUMBER)){
        expr.type = EXPRESSION_TYPE_NUMBER;
        memcpy(&(expr.number.value), &(previous(iterator)->str),sizeof(AvString));
        return expr;
    }
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_open)){
        struct Expression_S expression = parseExpression(iterator);
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_close, "Expect ')' after expression.");
        avMemcpy(&expr, &expression, sizeof(struct Expression_S));
        return expr;
    }
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_open)){
        return parseArray(iterator);
    }
    logParserError(iterator, TOKEN_TYPE_STRING,AV_CSTR("Literal expected"));
    return (struct Expression_S){.type=EXPRESSION_TYPE_NONE};
}

static struct Expression_S parseCall(TokenIterator* iterator){
    struct Expression_S expr = {.type = EXPRESSION_TYPE_CALL};
    struct Expression_S func = parsePrimary(iterator);
    
    
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_open)){
        if(func.type != EXPRESSION_TYPE_IDENTIFIER){
            logParserError(iterator, TOKEN_TYPE_TEXT, AV_CSTR("Expected function name"));
            return func;
        }
        avStringCopyToAllocator(func.identifier.identifier, &expr.call.function, iterator->allocator);
        
        AvDynamicArray arguments;
        avDynamicArrayCreate(0, sizeof(struct Expression_S), &arguments);


        while(!check(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_close)){
            struct Expression_S arg = parseExpression(iterator);
            avDynamicArrayAdd(&arg, arguments);

            if(!match(iterator, TOKEN_TYPE_PUNCTUATOR_comma)){
                break;
            }
        }
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_close, "Expected ')' after argument.");

        expr.call.argumentCount = avDynamicArrayGetSize(arguments);
        if(expr.call.argumentCount){
            expr.call.arguments = avAllocatorAllocate(sizeof(struct Expression_S)*expr.call.argumentCount, iterator->allocator);
            avDynamicArrayReadRange(expr.call.arguments, expr.call.argumentCount, 0, sizeof(struct Expression_S), 0, arguments);
        }
        avDynamicArrayDestroy(arguments);
        return expr;
    }
    return func;
}

static struct Expression_S parseArray(TokenIterator* iterator){
    struct Expression_S expr = {.type = EXPRESSION_TYPE_ARRAY};

    AvDynamicArray elements;
    avDynamicArrayCreate(0, sizeof(struct Expression_S), &elements);

    while(!check(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close)){
        struct Expression_S element = parseExpression(iterator);
        avDynamicArrayAdd(&element, elements);

        if(!match(iterator, TOKEN_TYPE_PUNCTUATOR_comma)){
            break;
        }
    }
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close, "Expected ']' after array");

    expr.array.length = avDynamicArrayGetSize(elements);
    if(expr.array.length){
        expr.array.elements = avAllocatorAllocate(sizeof(struct Expression_S)*expr.array.length, iterator->allocator);
        avDynamicArrayReadRange(expr.array.elements, expr.array.length, 0, sizeof(struct Expression_S), 0, elements);
    }
    avDynamicArrayDestroy(elements);
    return expr;
}

static struct Expression_S parseIndex(TokenIterator* iterator){
    
    struct Expression_S expr = {0};
    struct Expression_S left = parseCall(iterator);
    while(match(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_open)){
        struct Expression_S index = parseExpression(iterator);
        expr.type = EXPRESSION_TYPE_INDEX;
        expr.index.index = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        avMemcpy(expr.index.index, &index, sizeof(struct Expression_S));
        expr.index.expression = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        avMemcpy(expr.index.expression, &left, sizeof(struct Expression_S));

        avMemcpy(&left, &expr, sizeof(struct Expression_S));
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close, "expected ']' after index");
    }
    return left;
}

static struct Expression_S parseUnary(TokenIterator* iterator){
    
    struct Expression_S expr = {0};
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_minus, TOKEN_TYPE_PUNCTUATOR_not, TOKEN_TYPE_PUNCTUATOR_plus)){
        Token* operator = previous(iterator);
        switch(operator->type){
            case TOKEN_TYPE_PUNCTUATOR_minus:
                expr.unary.operator = UNARY_OPERATOR_MINUS;
                break;
            case TOKEN_TYPE_PUNCTUATOR_not:
                expr.unary.operator = UNARY_OPERATOR_NOT;
                break;
            case TOKEN_TYPE_PUNCTUATOR_plus:
                expr.unary.operator = UNARY_OPERATOR_PLUS;
            default:
                break;
        }
        expr.type = EXPRESSION_TYPE_UNARY;
        struct Expression_S un = parseUnary(iterator);
        expr.unary.expression = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        avMemcpy(expr.unary.expression, &un, sizeof(struct Expression_S));
        return expr;
    }
    return parseIndex(iterator);
}

static struct Expression_S parseEnumeration(TokenIterator* iterator){
    struct Expression_S expr = {.type=EXPRESSION_TYPE_ENUMERATION};

    if(match(iterator, TOKEN_TYPE_KEYWORD_files, TOKEN_TYPE_KEYWORD_directories)){
        TokenType enumerationType = previous(iterator)->type;
        consume(iterator, TOKEN_TYPE_KEYWORD_in, "Expected keyword 'in' after keyword 'files'");
        switch(enumerationType){
            case TOKEN_TYPE_KEYWORD_files:
                expr.enumeration.dirs = 0;
                break;
            case TOKEN_TYPE_KEYWORD_directories:
                expr.enumeration.dirs = 1;
                break;
            default:
                break;
        }

        struct Expression_S dir = parseUnary(iterator);
        expr.enumeration.directory = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        avMemcpy(expr.enumeration.directory, &dir, sizeof(struct Expression_S));
        if(match(iterator, TOKEN_TYPE_KEYWORD_recursive)){
            expr.enumeration.recursive = true;
        }
        return expr;
    }
    return parseUnary(iterator);
}

static struct Expression_S parseMultiplication(TokenIterator* iterator){

    struct Expression_S expr = {.type = EXPRESSION_TYPE_MULTIPLICATION };
    
    struct Expression_S left = parseEnumeration(iterator);
    while(match(iterator, TOKEN_TYPE_PUNCTUATOR_star, TOKEN_TYPE_PUNCTUATOR_divide)){
        switch(previous(iterator)->type){
            case TOKEN_TYPE_PUNCTUATOR_star:
                expr.multiplication.operator = MULTIPLICATION_OPERATOR_MULTIPLY;
            break;
            case TOKEN_TYPE_PUNCTUATOR_divide:
                expr.multiplication.operator = MULTIPLICATION_OPERATOR_DIVIDE;
            break;
            default:
                avAssert(false, "shoud not reach here");
                break;
        }
        struct Expression_S right = parseMultiplication(iterator);
        expr.multiplication.left = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        expr.multiplication.right = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        avMemcpy(expr.multiplication.left, &left, sizeof(struct Expression_S));
        avMemcpy(expr.multiplication.right, &right, sizeof(struct Expression_S));
        avMemcpy(&left, &expr, sizeof(struct Expression_S));
    }
    return left;
}

static struct Expression_S parseSummation(TokenIterator* iterator){
    struct Expression_S expr = {.type = EXPRESSION_TYPE_SUMMATION };
    
    struct Expression_S left = parseMultiplication(iterator);
    while(match(iterator, TOKEN_TYPE_PUNCTUATOR_plus, TOKEN_TYPE_PUNCTUATOR_minus)){
        switch(previous(iterator)->type){
            case TOKEN_TYPE_PUNCTUATOR_plus:
                expr.summation.operator = SUMMATION_OPERATOR_ADD;
            break;
            case TOKEN_TYPE_PUNCTUATOR_minus:
                expr.summation.operator = SUMMATION_OPERATOR_SUBTRACT;
            break;
            default:
                avAssert(false, "shoud not reach here");
                break;
        }
        struct Expression_S right = parseSummation(iterator);
        expr.summation.left = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        expr.summation.right = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        avMemcpy(expr.summation.left, &left, sizeof(struct Expression_S));
        avMemcpy(expr.summation.right, &right, sizeof(struct Expression_S));
        avMemcpy(&left, &expr, sizeof(struct Expression_S));
    }
    return left;
}

static struct Expression_S parseComparison(TokenIterator* iterator){
    struct Expression_S expr = {.type=EXPRESSION_TYPE_COMPARISON};
    struct Expression_S left = parseSummation(iterator);

    if(match(iterator, 
        TOKEN_TYPE_PUNCTUATOR_comparison, 
        TOKEN_TYPE_PUNCTUATOR_not_equals,
        TOKEN_TYPE_PUNCTUATOR_greater_than,
        TOKEN_TYPE_PUNCTUATOR_less_than,
        TOKEN_TYPE_PUNCTUATOR_greater_than_or_equal,
        TOKEN_TYPE_PUNCTUATOR_less_than_or_equal
        
    )){
        switch(previous(iterator)->type){
            case TOKEN_TYPE_PUNCTUATOR_comparison :
                expr.comparison.operator = COMPARISON_OPERATOR_EQUALS;
                break;
            case TOKEN_TYPE_PUNCTUATOR_not_equals :
                expr.comparison.operator = COMPARISON_OPERATOR_NOT_EQUALS;
                break;
            case TOKEN_TYPE_PUNCTUATOR_greater_than :
                expr.comparison.operator = COMPARISON_OPERATOR_GREATER_THAN;
                break;
            case TOKEN_TYPE_PUNCTUATOR_less_than :
                expr.comparison.operator = COMPARISON_OPERATOR_LESS_THAN;
                break;
            case TOKEN_TYPE_PUNCTUATOR_greater_than_or_equal :
                expr.comparison.operator = COMPARISON_OPERATOR_GREATER_THAN_OR_EQUAL;
                break;
            case TOKEN_TYPE_PUNCTUATOR_less_than_or_equal :
                expr.comparison.operator = COMPARISON_OPERATOR_LESS_THAN_OR_EQUAL;
                break;
            default:
                avAssert(false, "shoud not reach here");
                break;
        }
        struct Expression_S right = parseSummation(iterator);
        expr.comparison.left = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        expr.comparison.right = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        avMemcpy(expr.comparison.left, &left, sizeof(struct Expression_S));
        avMemcpy(expr.comparison.right, &right, sizeof(struct Expression_S));

        return expr;
    }
    return left;
}


static struct Expression_S parseCombination(TokenIterator* iterator){
    struct Expression_S expr = {.type=EXPRESSION_TYPE_COMPARISON};
    struct Expression_S left = parseComparison(iterator);
    
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_and, TOKEN_TYPE_PUNCTUATOR_or)){
        switch(previous(iterator)->type){
            case TOKEN_TYPE_PUNCTUATOR_and:
                expr.combination.operator = COMBINATION_OPERATOR_AND;
                break;
            case TOKEN_TYPE_PUNCTUATOR_or:
                expr.combination.operator = COMBINATION_OPERATOR_OR;
                break;
            default:
                avAssert(false, "logic error");
                break;
        }
        struct Expression_S right = parseComparison(iterator);
        expr.combination.left = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        expr.combination.right = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        avMemcpy(expr.combination.left, &left, sizeof(struct Expression_S));
        avMemcpy(expr.combination.right, &right, sizeof(struct Expression_S));
        return expr;
    }
    return left;
}

static struct Expression_S parseCommandExpression(TokenIterator* iterator){
    // command[retcode] "echo hello" evaluates to the return code
    // command "echo hello" evaluates to the command output
    // command "echo hello" > outputs to file 
    // command "ehco hello" >> appends to file
    // command "echo hello" | evaluates to the command output and outputs to the pipe

    struct Expression_S expr = {.type=EXPRESSION_TYPE_COMMAND};

    if(!match(iterator, TOKEN_TYPE_KEYWORD_command)){
        return parseCombination(iterator);
    }

    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_open)){
        struct Expression_S retCode = parseIndex(iterator);
        expr.command.retCode = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        avMemcpy(expr.command.retCode, &retCode, sizeof(struct Expression_S));
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close, "expected ']'");
    }

    struct Expression_S command = parseSummation(iterator);
    expr.command.command = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
    avMemcpy(expr.command.command, &command, sizeof(struct Expression_S));

    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_greater_than, TOKEN_TYPE_PUNCTUATOR_pipe)){
        Token* prev = previous(iterator);
        switch(prev->type){
            case TOKEN_TYPE_PUNCTUATOR_greater_than:
                expr.command.outputType = COMMAND_OUTPUT_TYPE_WRITE;
                if(match(iterator, TOKEN_TYPE_PUNCTUATOR_greater_than)){
                    expr.command.outputType = COMMAND_OUTPUT_TYPE_APPEND;
                }
                break;
            case TOKEN_TYPE_PUNCTUATOR_pipe:
                expr.command.outputType = COMMAND_OUTPUT_TYPE_PIPE;
                break;
            default:
                break;
        }
        struct Expression_S pipeOutput = parseCommandExpression(iterator);
        expr.command.pipeOutput = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        avMemcpy(expr.command.pipeOutput, &pipeOutput, sizeof(struct Expression_S));
    }
    return expr;
}

static struct Expression_S parseAssignmentExpression(TokenIterator* iterator){
    struct Expression_S expr = parseCommandExpression(iterator);
    
    if(match(iterator, 
        TOKEN_TYPE_PUNCTUATOR_equals, 
        TOKEN_TYPE_PUNCTUATOR_increment_assign, 
        TOKEN_TYPE_PUNCTUATOR_decrement_assign, 
        TOKEN_TYPE_PUNCTUATOR_multiply_assign, 
        TOKEN_TYPE_PUNCTUATOR_divide_assign
    )){
        struct Expression_S assign = {.type = EXPRESSION_TYPE_ASSIGNMENT };
        Token* operator = previous(iterator);
        switch(operator->type){
            case TOKEN_TYPE_PUNCTUATOR_equals:
                assign.assignment.operator = ASSIGNMENT_OPERATOR_ASSIGN;
                break;
            case TOKEN_TYPE_PUNCTUATOR_increment_assign:
                assign.assignment.operator = ASSIGNMENT_OPERATOR_INCREMENT_ASSIGN;
                break;
            case TOKEN_TYPE_PUNCTUATOR_decrement_assign:
                assign.assignment.operator = ASSIGNMENT_OPERATOR_DECREMENT_ASSIGN;
                break;
            case TOKEN_TYPE_PUNCTUATOR_multiply_assign:
                assign.assignment.operator = ASSIGNMENT_OPERATOR_MULTIPLY_ASSIGN;
                break;
            case TOKEN_TYPE_PUNCTUATOR_divide_assign:
                assign.assignment.operator = ASSIGNMENT_OPERATOR_DIVIDE_ASSIGN;
                break;
            default:
                break;
        }
        struct Expression_S* index = 0;
        switch(expr.type){
            
            case EXPRESSION_TYPE_INDEX:
                if(expr.index.expression->type!=EXPRESSION_TYPE_IDENTIFIER){
                    logParserError(iterator, TOKEN_TYPE_TEXT, AV_CSTRA("Expected modifiable value"));
                    break;
                }
                index = expr.index.index;
                avMemcpy(&expr, expr.index.expression, sizeof(struct Expression_S));
            case EXPRESSION_TYPE_IDENTIFIER:
                avStringUnsafeCopy(&assign.assignment.variable, expr.identifier.identifier);
                break;
            default:
                logParserError(iterator, TOKEN_TYPE_TEXT, AV_CSTRA("Expected modifiable value"));
                break;
        }
        

        struct Expression_S value = parseExpression(iterator);
        assign.assignment.value = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        avMemcpy(assign.assignment.value, &value, sizeof(struct Expression_S));
        if(index){
            assign.assignment.index = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
            avMemcpy(assign.assignment.index, index, sizeof(struct Expression_S));
        }
        return assign;
    }

    return expr;
}

static struct Expression_S parseExpression(TokenIterator* iterator){
    return parseAssignmentExpression(iterator);
}

static struct Statement_S parseStatement(TokenIterator* iterator);

static struct Statement_S parseExpressionStatement(TokenIterator* iterator){
    struct Expression_S expr = parseExpression(iterator);
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_semicolon, "expected ';' after statement");
    struct Statement_S stmt = {.type = STATEMENT_TYPE_EXPRESSION, .expression = expr};
    return stmt;
}

static struct Statement_S parseVariableDefinition(TokenIterator* iterator){

    struct Statement_S stmt = {.type = STATEMENT_TYPE_VARIABLE_DEFINITION};

    Token* variableName = consume(iterator, TOKEN_TYPE_TEXT, "expected variable name");
    memcpy(&(stmt.variableDefinition.identifier),&(variableName->str), sizeof(AvString));

    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_open)){
        stmt.variableDefinition.size = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        if(check(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close)){
            struct Expression_S size = {.type=EXPRESSION_TYPE_NONE};
            avMemcpy(stmt.variableDefinition.size, &size, sizeof(struct Expression_S));
        }else{
            struct Expression_S size = parseExpression(iterator);
            consume(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close, "expected ']' after array index");
            avMemcpy(stmt.variableDefinition.size, &size, sizeof(struct Expression_S));
        }
    }
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_equals)){
        struct Expression_S value = parseExpression(iterator);
        avMemcpy(&stmt.variableDefinition.initialValue, &value, sizeof(struct Expression_S)); 
    }

    consume(iterator, TOKEN_TYPE_PUNCTUATOR_semicolon, "expected ';' after statement");

    return stmt;
}

static struct Statement_S parseForeachStatement(TokenIterator* iterator){
    struct Statement_S stmt = {.type=STATEMENT_TYPE_FOREACH};

    avStringUnsafeCopy(&stmt.foreachStatement.variable, consume(iterator, TOKEN_TYPE_TEXT, "expected variable name")->str);
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_open)){
        avStringUnsafeCopy(&stmt.foreachStatement.index, consume(iterator, TOKEN_TYPE_TEXT, "expected variable name")->str);
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close, "expected ']'");
    }

    if(check(iterator, TOKEN_TYPE_KEYWORD_from)){
        consume(iterator, TOKEN_TYPE_KEYWORD_from, "expected keyword 'in' or 'from'");
    }else{
        consume(iterator, TOKEN_TYPE_KEYWORD_in, "expected keyword 'in' or 'from'");
    }
    
    struct Expression_S expr = parseExpression(iterator);
    struct Statement_S statement = parseStatement(iterator);
    stmt.foreachStatement.collection = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
    avMemcpy(stmt.foreachStatement.collection, &expr, sizeof(struct Expression_S));
    stmt.foreachStatement.statement = avAllocatorAllocate(sizeof(struct Statement_S), iterator->allocator);
    avMemcpy(stmt.foreachStatement.statement, &statement, sizeof(struct Statement_S));

    return stmt;
}

static struct Statement_S parseReturnStatement(TokenIterator* iterator){
    struct Statement_S stmt = {.type=STATEMENT_TYPE_RETURN};
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_semicolon)){
        return stmt;
    }
    struct Expression_S expr = parseExpression(iterator);
    stmt.returnStatement.value = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
    avMemcpy(stmt.returnStatement.value, &expr, sizeof(struct Expression_S));
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_semicolon, "expected ';' after expression");
    return stmt;
}


static struct Statement_S parseStatement(TokenIterator* iterator);
static struct Statement_S parseIfStatement(TokenIterator* iterator){
    struct Statement_S stmt = {.type= STATEMENT_TYPE_IF};
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_open, "expected '('");
    struct Expression_S expr = parseExpression(iterator);
    stmt.ifStatement.check = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
    avMemcpy(stmt.ifStatement.check, &expr, sizeof(struct Expression_S));
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_close, "expected ')'");
    struct Statement_S body = parseStatement(iterator);
    stmt.ifStatement.branch = avAllocatorAllocate(sizeof(struct Statement_S), iterator->allocator);
    avMemcpy(stmt.ifStatement.branch, &body, sizeof(struct Statement_S));

    if(match(iterator, TOKEN_TYPE_KEYWORD_else)){
        if(match(iterator, TOKEN_TYPE_KEYWORD_if)){
            struct Statement_S alt = parseIfStatement(iterator);
            stmt.ifStatement.alternativeBranch = avAllocatorAllocate(sizeof(struct Statement_S), iterator->allocator);
            avMemcpy(stmt.ifStatement.alternativeBranch, &alt, sizeof(struct Statement_S));
        }else{
            struct Statement_S alt = parseStatement(iterator);
            stmt.ifStatement.alternativeBranch = avAllocatorAllocate(sizeof(struct Statement_S), iterator->allocator);
            avMemcpy(stmt.ifStatement.alternativeBranch, &alt, sizeof(struct Statement_S));
        }
    }
    return stmt;
}

static struct Statement_S parseFunctionDefinition(TokenIterator* iterator){
    struct Statement_S stmt = {.type = STATEMENT_TYPE_FUNCTION_DEFINITION};
    
    Token* functionName = consume(iterator, TOKEN_TYPE_TEXT, "this should be checked before entering function");
    avStringUnsafeCopy(&stmt.functionDefinition.functionName, functionName->str);
    
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_open, "this should also be checked before entering");
    
    AvDynamicArray params;
    avDynamicArrayCreate(0, sizeof(struct FunctionParameter_S), &params);
    while(!check(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_close)){
        struct FunctionParameter_S param = {};
        avStringUnsafeCopy(&param.name, consume(iterator, TOKEN_TYPE_TEXT, "expected parameter name")->str);
        if(match(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_open)){
            if(!check(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close)){
                struct Expression_S expr = parseExpression(iterator);
                avMemcpy(&param.size, &expr, sizeof(struct Expression_S));
            }else{
                param.unknownSize = 1;
            }
            consume(iterator, TOKEN_TYPE_PUNCTUATOR_bracket_close, "expected ']'");
        }
        avDynamicArrayAdd(&param, params);
        if(!match(iterator, TOKEN_TYPE_PUNCTUATOR_comma)){
            break;
        }
    }
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_parenthese_close, "expected ')'");
    stmt.functionDefinition.parameterCount = avDynamicArrayGetSize(params);
    if(stmt.functionDefinition.parameterCount){
        stmt.functionDefinition.parameters = avAllocatorAllocate(sizeof(struct FunctionParameter_S)*stmt.functionDefinition.parameterCount, iterator->allocator);
        avDynamicArrayReadRange(stmt.functionDefinition.parameters, stmt.functionDefinition.parameterCount, 0, sizeof(struct FunctionParameter_S), 0, params);
    }

    struct Statement_S functionBody = parseStatement(iterator);
    stmt.functionDefinition.body = avAllocatorAllocate(sizeof(struct Statement_S), iterator->allocator);
    avMemcpy(stmt.functionDefinition.body, &functionBody, sizeof(struct Statement_S));
    return stmt;
}

static struct Statement_S parseInheritStatement(TokenIterator* iterator){
    struct Statement_S stmt = {.type=STATEMENT_TYPE_INHERIT};

    consume(iterator, TOKEN_TYPE_KEYWORD_inherit, "this should never trigger");
    Token* variable = consume(iterator, TOKEN_TYPE_TEXT, "expected variable name");

    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_equals)){
        struct Expression_S expr = parseExpression(iterator);
        stmt.inheritStatement.defaultValue = avAllocatorAllocate(sizeof(struct Expression_S), iterator->allocator);
        avMemcpy(stmt.inheritStatement.defaultValue, &expr, sizeof(struct Expression_S));
    }
    avStringUnsafeCopy(&stmt.inheritStatement.variable, variable->str);
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_semicolon, "expected ';' after statement");
    return stmt;
}


static struct ImportMapping_S parseDefinitionMapping(TokenIterator* iterator){

    struct ImportMapping_S mapping = {0};

    if(match(iterator, TOKEN_TYPE_KEYWORD_provide)){
        Token* libraryFile = consume(iterator, TOKEN_TYPE_STRING, "expected library");
        avStringUnsafeCopy(&mapping.symbol, libraryFile->str);
        consume(iterator, TOKEN_TYPE_KEYWORD_as, "expected 'as'");
        mapping.type = DEFINITION_MAPPING_PROVIDE;
        if(match(iterator, TOKEN_TYPE_KEYWORD_global)){
            mapping.type |= DEFINITION_MAPPING_GLOBAL;
        }
        Token* alias = consume(iterator, TOKEN_TYPE_STRING, "expected library alias");
        avStringUnsafeCopy(&mapping.alias, alias->str);
    }else{
        Token* symbol = consume(iterator, TOKEN_TYPE_TEXT, "expected symbol name");
        avStringUnsafeCopy(&mapping.symbol, symbol->str);
        if(match(iterator, TOKEN_TYPE_KEYWORD_as)){
            Token* alias = consume(iterator, TOKEN_TYPE_TEXT, "expected alias");
            avStringUnsafeCopy(&mapping.alias, alias->str);
        }
        mapping.type = DEFINITION_MAPPING_DEFAULT;
    }
    consume(iterator, TOKEN_TYPE_PUNCTUATOR_semicolon, "expected ';' after statement");
    return mapping;
}

static struct Statement_S parseImportStatement(TokenIterator* iterator){
    struct Statement_S stmt = {.type=STATEMENT_TYPE_IMPORT};
    consume(iterator, TOKEN_TYPE_KEYWORD_import, "this should never trigger");
    bool32 global = false;
    if(match(iterator, TOKEN_TYPE_KEYWORD_global)){
        global = true;
    }
    Token* fileName = nullptr;
    if(match(iterator, TOKEN_TYPE_STRING)){
        fileName = previous(iterator);
    }else{
        logParserError(iterator, TOKEN_TYPE_STRING, AV_CSTR("file name expected"));
        return (struct Statement_S){0};
    }

    memcpy(&(stmt.importStatement.importFile), &(fileName->str), sizeof(AvString));
    stmt.importStatement.local = !global;
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_brace_open)){
        AvDynamicArray mappings;
        avDynamicArrayCreate(0, sizeof(struct ImportMapping_S), &mappings);

        while(!check(iterator, TOKEN_TYPE_PUNCTUATOR_brace_close)){
            struct ImportMapping_S mapping = parseDefinitionMapping(iterator);
            avDynamicArrayAdd(&mapping, mappings);
        }
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_brace_close, "expected '}' after statement");
        stmt.importStatement.mappingCount = avDynamicArrayGetSize(mappings);

        if(stmt.importStatement.mappingCount){
            stmt.importStatement.mappings = avAllocatorAllocate(sizeof(struct ImportMapping_S)*stmt.importStatement.mappingCount, iterator->allocator);
            avDynamicArrayReadRange(stmt.importStatement.mappings, stmt.importStatement.mappingCount, 0, sizeof(struct ImportMapping_S), 0, mappings);
        }
        avDynamicArrayDestroy(mappings);
        return stmt;
    }else{
        if(match(iterator, TOKEN_TYPE_PUNCTUATOR_semicolon)){
            return stmt;
        }

        struct ImportMapping_S mapping = parseDefinitionMapping(iterator);
        stmt.importStatement.mappings = avAllocatorAllocate(sizeof(struct ImportMapping_S), iterator->allocator);
        avMemcpy(stmt.importStatement.mappings, &mapping, sizeof(struct ImportMapping_S));
        stmt.importStatement.mappingCount = 1;
        return stmt;
    }
    return stmt;
}

static struct Statement_S parseBlockStatement(TokenIterator* iterator){
    struct Statement_S stmt = {.type = STATEMENT_TYPE_BLOCK};

    AvDynamicArray statements;
    avDynamicArrayCreate(0, sizeof(struct Statement_S), &statements);
    while(!check(iterator, TOKEN_TYPE_PUNCTUATOR_brace_close) && !isAtEnd(iterator)){
        struct Statement_S statement = parseStatement(iterator);
        while(match(iterator, TOKEN_TYPE_PUNCTUATOR_semicolon));
        if(statement.type == STATEMENT_TYPE_NONE){
            break;
        }
        avDynamicArrayAdd(&statement, statements);
    }
    stmt.block.statementCount = avDynamicArrayGetSize(statements);
    if(stmt.block.statementCount){
        if(stmt.block.statementCount==1){
            avDynamicArrayRead(&stmt, 0, statements);
            if(stmt.type==STATEMENT_TYPE_VARIABLE_DEFINITION){
                // emit just regular expression statement as a variable definition in singular statement block is pointless
                // and this allows for optimisations by not emiting a block with scope for singular statement blocks.
                stmt.type = STATEMENT_TYPE_EXPRESSION;
                struct Expression_S tmp = stmt.variableDefinition.initialValue;
                avMemcpy(&stmt.expression, &tmp, sizeof(struct Expression_S));
            }
            avDynamicArrayDestroy(statements);
            return stmt;
        }
        stmt.block.statements = avAllocatorAllocate(sizeof(struct Statement_S)*stmt.block.statementCount, iterator->allocator);
        avDynamicArrayReadRange(stmt.block.statements, stmt.block.statementCount, 0, sizeof(struct Statement_S), 0, statements);
    }else{
        avDynamicArrayDestroy(statements);
        return (struct Statement_S){.type=STATEMENT_TYPE_NONE};
    }
    avDynamicArrayDestroy(statements);
    return stmt;
}

static struct Statement_S parseStatement(TokenIterator* iterator){
    struct Statement_S stmt = {0};
    uint32 line = iterator->tokens[iterator->current].line;
    
    if(check(iterator, TOKEN_TYPE_KEYWORD_import)){
        struct Statement_S statement = parseImportStatement(iterator);
        statement.line = line;
        return statement;
    }

    if(check(iterator, TOKEN_TYPE_KEYWORD_inherit)){
        struct Statement_S statement = parseInheritStatement(iterator);
        statement.line = line;
        return statement;
    }

    if(match(iterator, TOKEN_TYPE_TEXT)){
        recede(iterator);
        struct Statement_S statement = parseExpressionStatement(iterator);
        statement.line = line;
        return statement;
    }
    if(match(iterator, TOKEN_TYPE_KEYWORD_foreach)){
        struct Statement_S statement = parseForeachStatement(iterator);
        statement.line = line;
        return statement;
    }
    if(match(iterator, TOKEN_TYPE_KEYWORD_return)){
        struct Statement_S statement = parseReturnStatement(iterator);
        statement.line = line;
        return statement;
    }
    if(match(iterator, TOKEN_TYPE_KEYWORD_continue)){
        struct Statement_S statement = {.type = STATEMENT_TYPE_CONTINUE,.line=line};
        return statement;
    }
    if(match(iterator, TOKEN_TYPE_KEYWORD_break)){
        struct Statement_S statement = {.type = STATEMENT_TYPE_BREAK,.line=line};
        return statement;
    }
    if(match(iterator, TOKEN_TYPE_KEYWORD_if)){
        struct Statement_S statement = parseIfStatement(iterator);
        statement.line = line;
        return statement;
    }
    if(match(iterator, TOKEN_TYPE_KEYWORD_var)){
        struct Statement_S statement = parseVariableDefinition(iterator);
        statement.line = line;
        return statement;
    }
    if(match(iterator, TOKEN_TYPE_KEYWORD_func)){
        struct Statement_S statement = parseFunctionDefinition(iterator);
        statement.line = line;
        return statement;
    }
    if(check(iterator, TOKEN_TYPE_KEYWORD_command)){
        struct Statement_S statement = parseExpressionStatement(iterator);
        statement.line = line;
        return statement;
    }
    if(match(iterator, TOKEN_TYPE_PUNCTUATOR_brace_open)){
        struct Statement_S block = parseBlockStatement(iterator);
        consume(iterator, TOKEN_TYPE_PUNCTUATOR_brace_close, "expected '}' after statements");
        if(block.type==STATEMENT_TYPE_BLOCK){
            block.line = line;
        }
        return block;
    }

    logParserError(iterator, TOKEN_TYPE_TEXT, AV_CSTR("expected valid statement"));
    return stmt;
}



bool32 parseProject(AV_DS(AvDynamicArray, Token) tokenList, Project* project){
    uint64 tokenCount = avDynamicArrayGetSize(tokenList);
    Token* tokens = avCallocate(tokenCount, sizeof(Token), "allocating tokens");
    avDynamicArrayReadRange(tokens, tokenCount, 0, sizeof(Token), 0, tokenList);
    TokenIterator iterator = {
        .allocator=  project->allocator,
        .current = 0,
        .tokenCount = tokenCount,
        .tokens = tokens,
        .status = 0,
        .projectFile = project->projectFileName,
    };
    struct Statement_S projectBlock = parseBlockStatement(&iterator);
    if(projectBlock.type == STATEMENT_TYPE_NONE){
        avFree(tokens);
        return false;
    }
    if(projectBlock.type != STATEMENT_TYPE_BLOCK){
        project->statementCount = 1;
        project->statements = avAllocatorAllocate(sizeof(struct Statement_S), project->allocator);
        avMemcpy(project->statements, &projectBlock, sizeof(struct Statement_S));
    }else{
        project->statements = projectBlock.block.statements;
        project->statementCount = projectBlock.block.statementCount;
    }
    avFree(tokens);
    return iterator.status==ITERATOR_STATUS_OK;    
}





