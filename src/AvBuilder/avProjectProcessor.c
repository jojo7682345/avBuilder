#include "avBuilder.h"
#include <AvUtils/avMemory.h>
#include <AvUtils/logging/avAssert.h>
#include <string.h>
#include <stdarg.h>
#include <stdarg.h>
#include <stdio.h>

#include <AvUtils/avFileSystem.h>

#define AV_DYNAMIC_ARRAY_EXPOSE_MEMORY_LAYOUT
#include <AvUtils/dataStructures/avDynamicArray.h>

#include "avProjectLang.h"
#include "builtIn/avBuilderBuiltIn.h"

void semanticError(uint32 line, Project* project, const char* message, ...){
    va_list args;
	va_start(args, message);

	avStringPrintf(AV_CSTR("Semantic Error in project %S at line %u:\n\t"), project->name, line);
	avStringPrintfVA(AV_CSTR(message), args);
    avStringPrintln(AV_EMPTY_STRING);

    va_end(args);
}

void enterScope(enum ScopeType type, struct Statement_S* statement, Project* ctx){
    if(type!=SCOPE_TYPE_TOPLEVEL){
        avAssert(statement!=NULL, "statement must be passed");
        avAssert(statement->attachedScope==NULL, "statement must not already have a scope");
    }
	Scope* scope = avAllocatorAllocate(sizeof(Scope), ctx->allocator);
	avAllocatorCreate(0, AV_ALLOCATOR_TYPE_DYNAMIC, &scope->allocator);
	ctx->allocator = &scope->allocator;
	scope->parent = ctx->currentScope;
    scope->type = type;
	ctx->currentScope = scope;
    if(statement) statement->attachedScope = scope;
	avDynamicArrayCreate(0, sizeof(Symbol), &scope->symbols);
}

void exitScope(Project* ctx){
	avAssert(ctx->currentScope!=NULL, "scope inbalance");
	Scope* scope = ctx->currentScope;
	ctx->currentScope = scope->parent;
	ctx->allocator = &ctx->currentScope->allocator;
    // semantic scope information is cleaned up during project destruction
	
    // avAllocatorDestroy(&scope->allocator);
	// avDynamicArrayDestroy(scope->symbols);
}

Symbol* resolveSymbol(AvString identifier, Project* ctx){
    Scope* scope = ctx->currentScope;
    while(scope){
        uint32 symbolCount = avDynamicArrayGetSize(scope->symbols);
        for(uint32 i = 0; i < symbolCount; i++){
            Symbol* symbol = avDynamicArrayGetPtr(i, scope->symbols);
            if(avStringEquals(identifier, symbol->identifier)){
                return symbol;
            }
        }

        scope = scope->parent;
    }
    return NULL;
}

int32 declareSymbol(Symbol symbol, Project* ctx){
    Symbol* resolved = resolveSymbol(symbol.identifier, ctx);
    if(resolved){
        return -1;
    }
    symbol.scope = ctx->currentScope;
    return avDynamicArrayAdd(&symbol, ctx->currentScope->symbols);
}

bool32 isInLoop(Project* ctx){
    Scope* scope = ctx->currentScope;
    while(scope){
        if(scope->type == SCOPE_TYPE_FOREACH){
            return true;
        }

        scope = scope->parent;
    }
    return false;
}

bool32 isInFunction(Project* ctx){
    Scope* scope = ctx->currentScope;
    while(scope){
        if(scope->type == SCOPE_TYPE_FUNCTION){
            return true;
        }

        scope = scope->parent;
    }
    return false;
}

struct ExpressionFlags {
    bool8 constant;
};

bool32 analyseExpression(struct Expression_S* expression, uint32 line, struct ExpressionFlags* flags, Project* ctx);

bool32 analyseIdentifier(struct Expression_S* expr, uint32 line, struct ExpressionFlags* flags, Project* ctx){
    Symbol* sym = resolveSymbol(expr->identifier.identifier, ctx);
    if(sym==0){
        semanticError(line, ctx, "Unidentified identifier %S", expr->identifier.identifier);
        if(flags) flags->constant = false;
        return false;
    }
    if(flags) flags->constant = sym->constValue;
    //expr->resolvedSymbol = sym;
    return true;
}

bool32 analyseAssignment(struct Expression_S* expr, uint32 line,  struct ExpressionFlags* flags, Project* ctx){
    struct Expression_S* left = expr->assignment.variable;
    bool8 ret = true;

    AvString variableIdentifier = {0};
    if(left->type == EXPRESSION_TYPE_IDENTIFIER){
        avStringUnsafeCopy(&variableIdentifier, left->identifier.identifier);
    }else if(left->type == EXPRESSION_TYPE_INDEX){
        struct IndexExpression_S index = left->index;
        if(index.expression->type != EXPRESSION_TYPE_IDENTIFIER){
            semanticError(line, ctx, "Left side of assignment must be modifiable value");
            ret = false;
        }else{
            avStringUnsafeCopy(&variableIdentifier, index.expression->identifier.identifier);
        }
        if(!analyseExpression(index.index, line, 0, ctx)){
            ret = false;
        }
    }

    Symbol* sym = resolveSymbol(variableIdentifier, ctx);
    if(sym->type!=SYMBOL_VARIABLE){
        semanticError(line, ctx, "Unidentified identifier %S", expr->identifier.identifier);
        ret = false;
    }
    if(sym->constant || sym->builtin){
        semanticError(line, ctx, "%S is a constant and cannot be assigned", variableIdentifier);
        ret = false;
    }

    struct ExpressionFlags flgs = {0};
    if(!analyseExpression(expr->assignment.value, line, &flgs, ctx)){
        ret = false;
    }
    if(flags) flags->constant = flgs.constant;
    return ret;


}

bool32 analyseUnary(struct Expression_S* expr, uint32 line,  struct ExpressionFlags* flags, Project* ctx){
    return analyseExpression(expr->unary.expression, line, flags, ctx);
}

bool32 analyseSummation(struct Expression_S* expr, uint32 line, struct ExpressionFlags* flags, Project* ctx){
    struct ExpressionFlags leftFlags = {0};
    bool32 retLeft = analyseExpression(expr->summation.left, line, &leftFlags, ctx);
    struct ExpressionFlags rightFLags = {0};
    bool32 retRight = analyseExpression(expr->summation.right, line, &rightFLags, ctx);

    if(flags){
        flags->constant = leftFlags.constant && rightFLags.constant;
    }

    return retLeft && retRight;
}

bool32 analyseMultiplication(struct Expression_S* expr, uint32 line, struct ExpressionFlags* flags, Project* ctx){
    struct ExpressionFlags leftFlags = {0};
    bool32 retLeft = analyseExpression(expr->multiplication.left, line, &leftFlags, ctx);
    struct ExpressionFlags rightFLags = {0};
    bool32 retRight = analyseExpression(expr->multiplication.right, line, &rightFLags, ctx);

    if(flags){
        flags->constant = leftFlags.constant && rightFLags.constant;
    }

    return retLeft && retRight;
}

bool32 analyseComparison(struct Expression_S* expr, uint32 line, struct ExpressionFlags* flags, Project* ctx){
    struct ExpressionFlags leftFlags = {0};
    bool32 retLeft = analyseExpression(expr->comparison.left, line, &leftFlags, ctx);
    struct ExpressionFlags rightFLags = {0};
    bool32 retRight = analyseExpression(expr->comparison.right, line, &rightFLags, ctx);

    if(flags){
        flags->constant = leftFlags.constant && rightFLags.constant;
    }

    return retLeft && retRight;
}

bool32 analyseCombination(struct Expression_S* expr, uint32 line, struct ExpressionFlags* flags, Project* ctx){
    struct ExpressionFlags leftFlags = {0};
    bool32 retLeft = analyseExpression(expr->combination.left, line, &leftFlags, ctx);
    struct ExpressionFlags rightFLags = {0};
    bool32 retRight = analyseExpression(expr->combination.right, line, &rightFLags, ctx);

    if(flags){
        flags->constant = leftFlags.constant && rightFLags.constant;
    }

    return retLeft && retRight;
}

bool32 analyseArray(struct Expression_S* expr, uint32 line, struct ExpressionFlags* flags, Project* ctx){
    struct ArrayExpression_S array = expr->array;
    bool32 ret = true;
    bool32 constant = true;
    for(uint32 i = 0; i < array.length; i++){
        struct ExpressionFlags flgs = {0};
        bool32 rtrn = analyseExpression(array.elements+i, line, &flgs, ctx);
        ret = ret && rtrn;
        constant = constant && flgs.constant;
    }
    if(flags){
        flags->constant = constant;
    }
    return ret;
}

bool32 analyseCall(struct Expression_S* expr, uint32 line, struct ExpressionFlags* flags, Project* ctx){
    struct CallExpression_S call = expr->call;
    
    bool32 ret = true;
    bool32 constant = true;
    for(uint32 i = 0; i < call.argumentCount; i++){
        struct ExpressionFlags flgs = {0};
        if(!analyseExpression(call.arguments+i, line, &flgs, ctx)){
            ret = false;
        }
        if(flgs.constant == false){
            constant = false;
        }
    }

    Symbol* sym = resolveSymbol(call.function, ctx);
    if(sym->type != SYMBOL_FUNCTION){
        semanticError(line, ctx, "Function %S not found", call.function);
        return false;
    }
    struct FunctionDefinition_S func;
    if(sym->builtin){
        struct FunctionParameter_S* params = avAllocate(sizeof(struct FunctionParameter_S)*sym->function.builtin->argumentCount, "");
        for(uint32 i = 0; i < sym->function.builtin->argumentCount; i++){
            struct FunctionParameter_S param = {
                .name = AV_CSTR(sym->function.builtin->argTypes[i].name),
                .size = {0},
                .unknownSize = false,
            };
            avMemcpy(params + i, &param, sizeof(struct FunctionParameter_S));
        }

        struct FunctionDefinition_S fn = {
            .functionName = sym->function.builtin->identifier,
            .parameterCount = sym->function.builtin->argumentCount,
            .parameters = params,
        };    
        avMemcpy(&func, &fn, sizeof(struct FunctionDefinition_S));
    }else{
        avMemcpy(&func, &sym->function.definition->functionDefinition, sizeof(struct FunctionDefinition_S));
    }

    if(flags) flags->constant = sym->constValue && constant;
    if(func.parameterCount > call.argumentCount){
        semanticError(line, ctx, "Function %S not supplied with enough arguments", func.functionName);
        return false;
    }
    if((func.parameterCount==0 || !func.parameters[func.parameterCount-1].unknownSize) && func.parameterCount != call.argumentCount){
        semanticError(line, ctx, "Function %S not supplied with too many arguments", func.functionName);
        return false;
    }
    if(sym->builtin){
        avFree(func.parameters);
    }
    return ret;
}

bool32 analyseIndex(struct Expression_S* expr, uint32 line, struct ExpressionFlags* flags, Project* ctx){
    struct IndexExpression_S index = expr->index;
    struct ExpressionFlags leftFlags = {0};
    bool32 retLeft = analyseExpression(index.expression, line, &leftFlags, ctx);
    struct ExpressionFlags rightFLags = {0};
    bool32 retRight = analyseExpression(index.index, line, &rightFLags, ctx);
    if(flags){
        flags->constant = leftFlags.constant && rightFLags.constant;
    }

    return retLeft && retRight;
}

bool32 analyseEnumeration(struct Expression_S* expr, uint32 line, struct ExpressionFlags* flags, Project* ctx){
    return analyseExpression(expr->enumeration.directory, line, flags, ctx);
}

bool32 analyseCommand(struct Expression_S* expr, uint32 line, struct ExpressionFlags* flags, Project* ctx){
    if(flags) flags->constant = false;
    bool32 ret = true;
    struct CommandExpression_S command = expr->command;
    
    if(command.pipeOutput && command.pipeOutput->type != EXPRESSION_TYPE_NONE){
        if(command.pipeOutput->type==EXPRESSION_TYPE_IDENTIFIER){
            Symbol* sym = resolveSymbol(command.pipeOutput->identifier.identifier, ctx);
            if(sym->type != SYMBOL_VARIABLE){
                ret = false;
                semanticError(line, ctx, "Unable to find variable %S", command.pipeOutput->identifier.identifier);
            }
        }else if(command.pipeOutput->type==EXPRESSION_TYPE_COMMAND){
            if(!analyseCommand(command.pipeOutput, line, 0, ctx)){
                ret = false;
            }
        }else{
            if(command.pipeOutput->type!=EXPRESSION_TYPE_LITERAL) {
                ret = false;
            }
        }
    }
    if(!analyseExpression(command.command, line, 0, ctx)){
        ret = false;
    }
    return ret;
}

bool32 analyseExpression(struct Expression_S* expression, uint32 line, struct ExpressionFlags* flags, Project* ctx){
    switch(expression->type){
        case EXPRESSION_TYPE_IDENTIFIER:
            return analyseIdentifier(expression, line, flags, ctx);
        case EXPRESSION_TYPE_ASSIGNMENT:
            return analyseAssignment(expression, line, flags, ctx);
        case EXPRESSION_TYPE_UNARY:
            return analyseUnary(expression, line, flags, ctx);
        case EXPRESSION_TYPE_SUMMATION:
            return analyseSummation(expression, line, flags, ctx);
        case EXPRESSION_TYPE_MULTIPLICATION:
            return analyseMultiplication(expression, line, flags, ctx);
        case EXPRESSION_TYPE_COMBINATION:
            return analyseCombination(expression, line, flags, ctx);
        case EXPRESSION_TYPE_COMPARISON:
            return analyseComparison(expression, line, flags, ctx);
        case EXPRESSION_TYPE_ARRAY:
            return analyseArray(expression, line, flags, ctx);
        case EXPRESSION_TYPE_CALL:
            return analyseCall(expression, line, flags, ctx);
        case EXPRESSION_TYPE_NUMBER:
        case EXPRESSION_TYPE_LITERAL:
            if(flags) flags->constant = true;
            return true;
        case EXPRESSION_TYPE_INDEX:
            return analyseIndex(expression, line, flags, ctx);
        case EXPRESSION_TYPE_ENUMERATION:
            return analyseEnumeration(expression, line, flags, ctx);
        case EXPRESSION_TYPE_COMMAND:
            return analyseCommand(expression, line, flags, ctx);
        default:
            if(flags) flags->constant = false;
            semanticError(line, ctx, "invalid expression type %u", expression->type);
            return false;
    }

    return true;
}

bool32 analyseStatement(struct Statement_S* statement, Project* ctx);

bool32 analyseFunction(struct Statement_S* statement, Project* ctx){
    struct FunctionDefinition_S* func = &statement->functionDefinition;
    bool32 ret = true;
    Symbol symbol = {.type = SYMBOL_FUNCTION, .function={.definition = statement }, .identifier=func->functionName};
    if(declareSymbol(symbol, ctx)==-1){
        semanticError(statement->line, ctx, "function %S already declared", func->functionName);
        ret = false;
    }
    if(ctx->currentScope->type!=SCOPE_TYPE_TOPLEVEL){
        semanticError(statement->line, ctx, "I am a buzkill and don't allow nested functions (%S)", func->functionName);
        ret = false;
    }
    // struct FunctionDefinition_S* currentFunc = ctx->currentFunction;
    // ctx->currentFunction = func;
    enterScope(SCOPE_TYPE_FUNCTION, statement, ctx);

    for(uint32 i = 0; i < func->parameterCount; i++){
        struct FunctionParameter_S param = func->parameters[i];
        Symbol paramSym = {.type = SYMBOL_VARIABLE, .identifier = param.name,};
        struct ExpressionFlags flags = {0};
        if(param.size.type != EXPRESSION_TYPE_NONE && !analyseExpression(&param.size, statement->line, &flags, ctx)){
            ret = false;
        }
        if(param.unknownSize && !flags.constant){
            semanticError(statement->line, ctx, "parameter %S's size is not constant or variadic", param.name);
            ret = false;
        }
        if(param.unknownSize && i != func->parameterCount - 1){
            semanticError(statement->line, ctx, "variadic parameter %S's is not last parameter", param.name);
            ret = false;
        }
        if(declareSymbol(paramSym, ctx)==-1){
            semanticError(statement->line, ctx, "parameter %S already defined", param.name);
            ret = false;
        }
    }

    ret = analyseStatement(func->body, ctx);
    exitScope(ctx);
    //ctx->currentFunction = currentFunc;
    return ret;
}

bool32 analyseBlock(struct Statement_S* statement, Project* ctx){
    struct BlockStatement_S block = statement->block;
    if(!block.statementCount){
        return true;
    }
    bool32 ret = true;
    enterScope(SCOPE_TYPE_BLOCK, statement, ctx);
    for(uint32 i = 0; i < block.statementCount; i++){
        if(!analyseStatement(block.statements + i, ctx)){
            ret = false;
        }
    }
    exitScope(ctx);
    return ret;
}

bool32 analyseForeach(struct Statement_S* statement, Project* ctx){
    struct ForeachStatement_S foreach = statement->foreachStatement;

    bool32 ret = true;
    enterScope(SCOPE_TYPE_FOREACH, statement, ctx);

    if(declareSymbol((Symbol){.type=SYMBOL_VARIABLE,.identifier=foreach.variable}, ctx)==-1){
        semanticError(statement->line, ctx, "variable %S already defined", foreach.variable);
        ret = false;
    }
    if(declareSymbol((Symbol){.type=SYMBOL_VARIABLE,.identifier=foreach.index}, ctx)==-1){
        semanticError(statement->line, ctx, "variable %S already defined", foreach.index);
        ret = false;
    }

    if(!analyseExpression(foreach.collection, statement->line, 0, ctx)){
        ret = false;
    }

    if(!analyseStatement(foreach.statement, ctx)){
        ret = false;
    }

    exitScope(ctx);
    return ret;
}


bool32 analyseIf(struct Statement_S* statement, Project* ctx){
    struct IfStatement_S stmt = statement->ifStatement;
    bool32 ret = true;

    if(!analyseExpression(stmt.check, statement->line, 0, ctx)){
        ret = false;
    }

    if(!analyseStatement(stmt.branch, ctx)){
        ret = false;
    }
    if(stmt.alternativeBranch && !analyseStatement(stmt.alternativeBranch, ctx)){
        ret = false;
    }
    return ret;
}



bool32 analyseImport(struct Statement_S* statement, Project* ctx){
    avStringDebugContextStart;
    struct ImportStatement_S import = statement->importStatement;
    if(ctx->currentScope->type != SCOPE_TYPE_TOPLEVEL){
        semanticError(statement->line, ctx, "Import statement not at top level");
        return false;
    }
    bool32 res = true;
    
    AvString importFile = AV_EMPTY;
    avStringClone(&importFile, import.importFile);

    avDynamicArrayForEachElement(struct Alias, ctx->libraryAliases, {
        if(avStringEquals(import.importFile, element.identifier)){
            avStringClone(&importFile, element.alias);
        }
    });

    if(!import.local){
        AvString homeDir = AV_EMPTY;
		extern void getInConfigFolder(AvStringRef dest, AvString subDir);
		getInConfigFolder(&homeDir, templatePath);
		avStringJoin(&importFile, homeDir, import.importFile);
		avStringPathNormalize(&importFile);
        avStringFree(&homeDir);
    }else{
        AvString tmp = {0};
        avStringPathResolveRelative(&tmp, ctx->projectFileName, import.importFile);
        avStringMove(&importFile, &tmp);
    }

    AvString projectFileContent = AV_EMPTY;
    AvString projectFileName = AV_EMPTY;
    if(!loadProjectFile(importFile, &projectFileContent, &projectFileName)){
        avStringPrintf(AV_CSTR("Failed to load project file %S\n"), importFile);
        avStringFree(&projectFileContent);
        avStringFree(&projectFileName); 
        res = false;
        goto loadingFailed;
    }

    AV_DS(AvDynamicArray, Token) tokens = AV_EMPTY;
    avDynamicArrayCreate(0, sizeof(Token), &tokens);
    if(!tokenizeProject(projectFileContent, projectFileContent, tokens)){
        avStringPrintf(AV_CSTR("Failed to tokenize project file %S\n"), importFile);
        
        avStringFree(&projectFileContent);
        avStringFree(&projectFileName); 
        res = false;
        goto tokenizingFailed;
    }
   
    Project* importProject = avAllocatorAllocate(sizeof(Project), ctx->allocator);
    projectCreate(importProject, projectFileName, importFile, projectFileContent, false);
    if(!parseProject(tokens, importProject)){
        avStringPrintf(AV_CSTR("Failed to parse project file %S\n"), importFile);
        res = false;
        goto parsingFailed;
    }

    AvDynamicArray aliases;
    avDynamicArrayClone(ctx->libraryAliases, &aliases);
    avDynamicArrayAppend(importProject->libraryAliases, &aliases);

    for(uint32 i = 0; i < import.mappingCount; i++){
        struct ImportMapping_S mapping = import.mappings[i];
        enum DefinitionMappingType type = mapping.type;
        if((type & DEFINITION_MAPPING_PROVIDE) == 0){
            continue;
        }
        AvString libraryMapping = AV_EMPTY;
        if(type & DEFINITION_MAPPING_GLOBAL){
            AvString homeDir = AV_EMPTY;
		    extern void getInConfigFolder(AvStringRef dest, AvString subDir);
		    getInConfigFolder(&homeDir, templatePath);
		    avStringJoin(&libraryMapping, homeDir, mapping.alias);
		    avStringFree(&homeDir);
            avStringMoveToAllocator(&libraryMapping, ctx->allocator);
        }else{
            avStringCopyToAllocator(mapping.alias, &libraryMapping, ctx->allocator);
        }

        struct Alias alias = {
            .identifier = mapping.symbol,
            .alias = libraryMapping,
        };
        avDynamicArrayAdd(&alias, importProject->libraryAliases);
    }
    importProject->parent = ctx;
    avDynamicArrayAdd(&importProject, ctx->importedProjects);
    if(!processProject(importProject)){
        avStringPrintf(AV_CSTR("Failed to perform processing on project file %S\n"), importFile);
        res = false;
        goto processingFailed;
    }

    for(uint32 i = 0; i < import.mappingCount; i++){
        struct ImportMapping_S mapping = import.mappings[i];
        enum DefinitionMappingType type = mapping.type;
        if(type & DEFINITION_MAPPING_PROVIDE){
            continue;
        }

        Symbol* symbol = resolveSymbol(mapping.symbol, importProject);
        if(!avStringIsEmpty(mapping.alias)){
            avStringUnsafeCopy(&symbol->identifier, mapping.alias);
        }
        if(declareSymbol(*symbol, ctx)==-1){
            semanticError(statement->line, ctx, "alias %S already defined", symbol->identifier);
            res = false;
            continue;
        }
    }
    
    avStringFree(&importFile);
    return res;

processingFailed:
parsingFailed:
    projectDestroy(importProject);
tokenizingFailed:
    avDynamicArrayDestroy(tokens);
loadingFailed:
    avStringFree(&projectFileName);
    avStringFree(&importFile);
    avStringDebugContextEnd;
    return res;
}


bool32 analyseInherit(struct Statement_S* statement, Project* ctx){
    struct InheritStatement_S inherit = statement->inheritStatement;
    bool32 ret = true;

    if(ctx->currentScope->type != SCOPE_TYPE_TOPLEVEL){
        semanticError(statement->line, ctx, "inherit statement not at top level");
        return false;
    }
    bool32 found = false;
    Project* project = ctx->parent;
    while(project){
        Symbol* sym = resolveSymbol(inherit.variable, project);
        if(sym->type == SYMBOL_VARIABLE){
            if(declareSymbol(*sym, ctx)==-1){
                semanticError(statement->line, ctx, "variable %S already defined", inherit.variable);
                ret = false;
            }
            found = true;
            break;
        }
        project = project->parent;
    }

    if(inherit.defaultValue){
        struct ExpressionFlags flags = {0};
        if(!analyseExpression(inherit.defaultValue, statement->line, &flags, ctx)){
            ret = false;
        }
        if(flags.constant==false){
            semanticError(statement->line, ctx, "default value of inherited variable %S is not constant expression", inherit.variable);
            ret = false;
        }
    }
    if(!inherit.defaultValue && !found){
        semanticError(statement->line, ctx, "inherited value for %S not found and default not provided", inherit.variable);
        ret = false;
    }
    return ret;
}

bool32 analyseVariableDefinition(struct Statement_S* statement, Project* ctx){
    struct VariableDefinition_S var = statement->variableDefinition;
    bool32 ret = true;
    
    
    if(var.size.type != EXPRESSION_TYPE_NONE){
        struct ExpressionFlags flags = {0};
        if(!analyseExpression(&var.size, statement->line, &flags, ctx)){
            ret = false;
        }
        if(flags.constant==false){
            semanticError(statement->line, ctx, "specified size of variable %S is not constant expression", var.identifier);
            ret = false;
        }
    }
    bool32 constant = false;
    if(var.initialValue.type != EXPRESSION_TYPE_NONE){
        struct ExpressionFlags flags = {0};
        if(!analyseExpression(&var.initialValue, statement->line, &flags, ctx)){
            ret = false;
        }
        if(!isInFunction(ctx) && flags.constant == false){
            semanticError(statement->line, ctx, "initial value of toplevel variable %S is not constant expression", var.identifier);
            ret = false;
        }
        constant = flags.constant;
    }
    
    Symbol symbol = {.type = SYMBOL_VARIABLE, .identifier = var.identifier, .constValue = constant};
    if(declareSymbol(symbol, ctx)==-1){
        semanticError(statement->line, ctx, "variable %S already defined", var.identifier);
        ret = false;
    }

    return ret;
}

bool32 analyseReturn(struct Statement_S* statement, Project* ctx){
    bool32 ret = true;
    if(!isInFunction(ctx)){ //ctx->currentFunction == 0 || 
        semanticError(statement->line, ctx, "return outside function");
        ret = false;
    }
    if(statement->returnStatement.value && !analyseExpression(statement->returnStatement.value, statement->line, 0, ctx)){
        ret = false;
    }
    return ret;
}


bool32 analyseStatement(struct Statement_S* statement, Project* ctx){
    switch(statement->type){
        case STATEMENT_TYPE_FUNCTION_DEFINITION:
            return analyseFunction(statement, ctx);
        case STATEMENT_TYPE_RETURN:
            return analyseReturn(statement, ctx);
        case STATEMENT_TYPE_BREAK:
        case STATEMENT_TYPE_CONTINUE:
            if(!isInLoop(ctx)){
                semanticError(statement->line, ctx, "break/continue outside loop");
                return false;
            }
        case STATEMENT_TYPE_BLOCK:
            return analyseBlock(statement, ctx);
        case STATEMENT_TYPE_IF:
            return analyseIf(statement, ctx);
        case STATEMENT_TYPE_VARIABLE_DEFINITION:
            return analyseVariableDefinition(statement, ctx);
        case STATEMENT_TYPE_IMPORT:
            return analyseImport(statement, ctx);
        case STATEMENT_TYPE_INHERIT:
            return analyseInherit(statement, ctx);
        case STATEMENT_TYPE_EXPRESSION:
            if(!isInFunction(ctx)){
                semanticError(statement->line, ctx, "Invalid statement at top level");
                return false;
            }
            return analyseExpression(&statement->expression, statement->line, 0, ctx);
        default:
            semanticError(statement->line, ctx, "Invalid statement found at line %u", statement->line);
            return false;
    }
}

bool32 processProject(Project* project){

    for(uint32 i = 0; i < builtInVariableCount; i++){
		struct BuiltInVariableDescription var = builtInVariables[i];
        Symbol symbol = {.type=SYMBOL_VARIABLE, .builtin=true, .constant=true, .constValue=true, .identifier=var.identifier, .variable.value=var.value};
		declareSymbol(symbol, project);
	}
    for(uint32 i = 0; i < builtInFunctionCount; i++){
		struct BuiltInFunctionDescription func = builtInFunctions[i];
        Symbol symbol = {.type=SYMBOL_FUNCTION, .builtin=true, .function.builtin = builtInFunctions + i, .identifier = func.identifier};
		declareSymbol(symbol, project);
	}

#ifdef _WIN32
	AvString platform = AV_CSTRA("WINDOWS");
#else
	AvString platform = AV_CSTRA("LINUX");
#endif
    declareSymbol((Symbol){.type=SYMBOL_VARIABLE, .constant = true, .constValue=true, .identifier=AV_CSTR("PROJECT_NAME"),.variable = {.value= (struct Value){.type=VALUE_TYPE_STRING,.asString=project->name}}}, project);
    declareSymbol((Symbol){.type=SYMBOL_VARIABLE, .constant = true, .constValue=true, .identifier=AV_CSTR("PROJECT_DIR"),.variable = {.value= currentDir(project, 0, nullptr)}}, project);
    declareSymbol((Symbol){.type=SYMBOL_VARIABLE, .constant = true, .constValue=true, .identifier=AV_CSTR("PLATFORM"),.variable = {.value= (struct Value){.type=VALUE_TYPE_STRING,.asString=platform}}}, project);

    bool32 ret = true;
    for(uint32 i = 0; i < project->statementCount; i++){
        struct Statement_S* statement = project->statements+i;
        if(!analyseStatement(statement, project)){
            ret = false;
        }
    }
    return ret;
}