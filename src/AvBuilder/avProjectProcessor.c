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
	//avAllocatorCreate(0, AV_ALLOCATOR_TYPE_DYNAMIC, &scope->allocator);
	//ctx->allocator = &scope->allocator;
	scope->parent = ctx->currentScope;
    scope->type = type;
    scope->project = ctx;
	ctx->currentScope = scope;
    if(statement) statement->attachedScope = scope;
	avDynamicArrayCreate(0, sizeof(Symbol), &scope->symbols);
    if(type == SCOPE_TYPE_FUNCTION || type == SCOPE_TYPE_FOREACH){
        Symbol returnValue = {
            .constant = true,
            .constValue = false,
            .builtin = true,
            .identifier = AV_EMPTY_STRING,
            .localIndex = 0,
            .scope = scope,
            .type = -1,
            .function.definition = NULL,
        };
        avDynamicArrayAdd(&returnValue, scope->symbols);
    }
}

void exitScope(Project* ctx){
	avAssert(ctx->currentScope!=NULL, "scope inbalance");
	Scope* scope = ctx->currentScope;
	ctx->currentScope = scope->parent;
    // semantic scope information is cleaned up during project destruction
	
	//ctx->allocator = &ctx->currentScope->allocator;
    // avAllocatorDestroy(&scope->allocator);
	// avDynamicArrayDestroy(scope->symbols);
}

Symbol* resolveSymbol(AvString identifier, int32* depth, Project* ctx){
    Scope* scope = ctx->currentScope;
    if(depth) *depth = -1;
    while(scope){
        if(depth) (*depth)++;
        uint32 symbolCount = avDynamicArrayGetSize(scope->symbols);
        for(uint32 i = 0; i < symbolCount; i++){
            Symbol* symbol = avDynamicArrayGetPtr(i, scope->symbols);
            if(avStringEquals(identifier, symbol->identifier)){
                return symbol;
            }
        }
        scope = scope->parent;
    }
    if(depth) *depth = 0;
    return NULL;
}

Symbol* declareSymbol(Symbol symbol, Project* ctx){
    Symbol* resolved = resolveSymbol(symbol.identifier, 0, ctx);
    if(resolved){
        return 0;
    }
    symbol.scope = ctx->currentScope;
    uint32 localIndex = avDynamicArrayAdd(&symbol, ctx->currentScope->symbols);
    Symbol* sym = avDynamicArrayGetPtr(localIndex, ctx->currentScope->symbols);
    sym->localIndex = localIndex;
    return sym;
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

int32 getScopeTypeDepth(enum ScopeType type, Project* ctx){
    Scope* scope = ctx->currentScope;
    int32 i = 0;
    while(scope){
        if(scope->type == type){
            return i;
        }
        i++;
        scope = scope->parent;
    }
    return -1;
}

Scope* getFunctionScope(Project* ctx){
    Scope* scope = ctx->currentScope;
    while(scope){
        if(scope->type == SCOPE_TYPE_FUNCTION){
            return scope;
        }

        scope = scope->parent;
    }
    return NULL;
}

struct ExpressionFlags {
    bool8 constant;
};

bool32 analyseExpression(struct Expression_S* expression, uint32 line, struct ExpressionFlags* flags, Project* ctx);

bool32 analyseIdentifier(struct Expression_S* expr, uint32 line, struct ExpressionFlags* flags, Project* ctx){
    int32 depth = 0;
    Symbol* sym = resolveSymbol(expr->identifier.identifier, &depth, ctx);
    if(sym==0){
        semanticError(line, ctx, "Unidentified identifier %S", expr->identifier.identifier);
        if(flags) flags->constant = false;
        return false;
    }
    if(flags) flags->constant = sym->constValue;
    expr->identifier.resolvedSymbol = sym;
    expr->identifier.depth = depth;
    return true;
}

bool32 analyseAssignment(struct Expression_S* expr, uint32 line,  struct ExpressionFlags* flags, Project* ctx){
    bool8 ret = true;

    if(expr->assignment.index){
        if(!analyseExpression(expr->assignment.index, line, 0, ctx)){
            ret = false;
        }
    }
    
    Symbol* sym = resolveSymbol(expr->assignment.variable, &expr->assignment.depth, ctx);
    if(!sym || sym->type!=SYMBOL_VARIABLE){
        semanticError(line, ctx, "Unidentified identifier %S", expr->identifier.identifier);
        ret = false;
    }
    expr->assignment.resolvedSymbol = sym;
    if(sym && (sym->constant || sym->builtin)){
        semanticError(line, ctx, "%S is a constant and cannot be assigned", expr->assignment.variable);
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
    int32 depth = 0;
    Symbol* sym = resolveSymbol(call.function, &depth, ctx);
    if(sym==NULL || sym->type != SYMBOL_FUNCTION){
        semanticError(line, ctx, "Function %S not found", call.function);
        return false;
    }
    expr->call.depth = depth;
    expr->call.resolvedSymbol = sym;
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
    
    if(command.retCode && command.retCode->type==EXPRESSION_TYPE_INDEX){
        if(command.retCode->index.expression->type != EXPRESSION_TYPE_IDENTIFIER){
            ret = false;
            semanticError(line, ctx, "Retcode variable must be modifiable");
        }else if(!analyseIndex(command.retCode, line, 0, ctx)){
            ret = false;
        }
    }else if(command.retCode && command.retCode->type==EXPRESSION_TYPE_IDENTIFIER){
        if(!analyseIdentifier(command.retCode, line, 0, ctx)){
            ret = false;
        }
    }

    if(!analyseExpression(command.command, line, 0, ctx)){
        ret = false;
    }
    if(command.pipeOutput){
        switch(command.outputType){
            case COMMAND_OUTPUT_TYPE_PIPE:
                if(command.pipeOutput->type != EXPRESSION_TYPE_COMMAND){
                    semanticError(line, ctx, "Can only pipe into other command");
                    ret = false;
                }else if (!analyseCommand(command.pipeOutput, line, 0, ctx)){
                    ret = false;
                }
                break;
            case COMMAND_OUTPUT_TYPE_WRITE:
            case COMMAND_OUTPUT_TYPE_APPEND:
                if(!analyseExpression(command.pipeOutput, line, 0, ctx)){
                    ret = false;
                }
                break;
        }
    }

    return ret;
}

bool32 analyseTernary(struct Expression_S* expr, uint32 line, struct ExpressionFlags* flags, Project* ctx){
    bool32 ret = true;
    struct ExpressionFlags exprFlags = {0};
    if(!analyseExpression(expr->ternary.expr, line, &exprFlags, ctx)){
        ret = false;
    }
    struct ExpressionFlags trueFlags = {0};
    if(!analyseExpression(expr->ternary.truePath, line, &trueFlags, ctx)){
        ret = false;
    }
    struct ExpressionFlags falseFlags = {0};
    if(!analyseExpression(expr->ternary.falsePath, line, &falseFlags, ctx)){
        ret = false;
    }

    if(flags) flags->constant = exprFlags.constant && trueFlags.constant && falseFlags.constant;

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
        case EXPRESSION_TYPE_TERNARY:
            return analyseTernary(expression, line, flags, ctx);
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
    if(declareSymbol(symbol, ctx)==NULL){
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
    statement->attachedScope = ctx->currentScope;

    for(uint32 i = 0; i < func->parameterCount; i++){
        struct FunctionParameter_S param = func->parameters[i];
        avAssert(!avStringIsEmpty(param.name), "parameter must be valid");
        Symbol paramSym = {.type = SYMBOL_VARIABLE, .identifier = param.name,};
        struct ExpressionFlags flags = {0};
        if(param.size.type != EXPRESSION_TYPE_NONE && !analyseExpression(&param.size, statement->line, &flags, ctx)){
            ret = false;
        }
        // if(param.unknownSize && !flags.constant){
        //     semanticError(statement->line, ctx, "parameter %S's size is not constant or variadic", param.name);
        //     ret = false;
        // }
        if(param.unknownSize && i != func->parameterCount - 1){
            semanticError(statement->line, ctx, "variadic parameter %S's is not last parameter", param.name);
            ret = false;
        }
        Symbol* sym = declareSymbol(paramSym, ctx);
        func->parameters[i].resolvedSymbol = sym;
        if(sym==NULL){
            semanticError(statement->line, ctx, "parameter %S already defined", param.name);
            ret = false;
        }
    }
    ctx->skipScope = true;
    if(!func->body){
        ret = false;
        semanticError(statement->line, ctx, "Function without body");
    }else{
        if(func->body->type==STATEMENT_TYPE_NONE){
            ret = true;
        }else{
            ret = analyseStatement(func->body, ctx);
        }
    }
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
    bool32 skip = ctx->skipScope;
    ctx->skipScope = false;
    if(!skip){
        enterScope(SCOPE_TYPE_BLOCK, statement, ctx);
    }
    for(uint32 i = 0; i < block.statementCount; i++){
        if(!analyseStatement(block.statements + i, ctx)){
            ret = false;
        }
    }
    if(!skip){
        exitScope(ctx);
    }
    return ret;
}

bool32 analyseForeach(struct Statement_S* statement, Project* ctx){
    struct ForeachStatement_S foreach = statement->foreachStatement;

    bool32 ret = true;

    if(!analyseExpression(foreach.collection, statement->line, 0, ctx)){
        ret = false;
    }

    enterScope(SCOPE_TYPE_FOREACH, statement, ctx);
    if(!avStringIsEmpty(foreach.variable) && (statement->foreachStatement.resolvedVarSymbol = declareSymbol((Symbol){.type=SYMBOL_VARIABLE,.identifier=foreach.variable}, ctx))==NULL){
        semanticError(statement->line, ctx, "variable %S already defined", foreach.variable);
        ret = false;
    }
    if(!avStringIsEmpty(foreach.index) && (statement->foreachStatement.resolvedIndexSymbol = declareSymbol((Symbol){.type=SYMBOL_VARIABLE,.identifier=foreach.index}, ctx))==NULL){
        semanticError(statement->line, ctx, "variable %S already defined", foreach.index);
        ret = false;
    }

    ctx->skipScope = true;
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

bool32 importProjectFile(AvString importFileLoc, bool32 isLocal, Project** proj, uint32 mappingCount, struct ImportMapping_S* mappings, AvString projectFile, Project* ctx){
    bool32 res = true;

    AvString importFile = {0};
    avStringClone(&importFile, importFileLoc);
    avDynamicArrayForEachElement(struct Alias, ctx->libraryAliases, {
        if(avStringEquals(importFile, element.identifier)){
            avStringClone(&importFile, element.alias);
            isLocal = 1;
        }
    });

    if(!isLocal){
        AvString homeDir = AV_EMPTY;
		extern void getInConfigFolder(AvStringRef dest, AvString subDir);
		getInConfigFolder(&homeDir, templatePath);
        AvString tmp = {0};
		avStringJoin(&tmp, homeDir, importFile);
		avStringPathNormalize(&tmp);
        avStringClone(&importFile, tmp);
        avStringFree(&tmp);
        avStringFree(&homeDir);
    }else{
        AvString tmp = {0};
        avStringPathResolveRelative(&tmp, projectFile, importFile);
        avStringClone(&importFile, tmp);
        avStringFree(&tmp);
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
   
    Project* importProject = avAllocatorAllocate(sizeof(Project), &ctx->baseAllocator);
    projectCreate(importProject, projectFileName, importFile, projectFileContent, false);
    avMemcpy(&importProject->options, &ctx->options, sizeof(ctx->options));
    uint32 failedIndex = -1;
    if(!parseProject(tokens, importProject)){
        avStringPrintf(AV_CSTR("Failed to parse project file %S\n"), importFile);
        res = false;
        goto parsingFailed;
    }
    

    AvDynamicArray aliases;
    avDynamicArrayClone(ctx->libraryAliases, &aliases);
    avDynamicArrayAppend(importProject->libraryAliases, &aliases);

    for(uint32 i = 0; i < mappingCount; i++){
        struct ImportMapping_S mapping = mappings[i];
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

            // AvString tmp = {0};
            // avStringPathResolveRelative(&tmp, ctx->projectFileName, mapping.alias);
            // avStringCopyToAllocator(tmp, &libraryMapping, ctx->allocator);
            // avStringFree(&tmp);
        }

        struct Alias alias = {
            .identifier = mapping.symbol,
            .alias = libraryMapping,
        };
        avDynamicArrayAdd(&alias, importProject->libraryAliases);
    }
    importProject->parent = ctx;
    failedIndex = avDynamicArrayAdd(&importProject, ctx->importedProjects);
    if(!processProject(importProject)){
        avStringPrintf(AV_CSTR("Failed to perform processing on project file %S\n"), importFile);
        res = false;
        goto processingFailed;
    }

    

    (*proj) = importProject;
    avStringFree(&importFile);
    avStringFree(&projectFileName);
    return res;

processingFailed:
parsingFailed:
    projectDestroy(importProject);
    if(failedIndex != -1){
        avDynamicArrayRemove(failedIndex, ctx->importedProjects);
    }
tokenizingFailed:
    avDynamicArrayDestroy(tokens);
loadingFailed:
    avStringFree(&projectFileName);
    avStringFree(&importFile);
    return res;
}

bool32 analyseImport(struct Statement_S* statement, Project* ctx){
    avStringDebugContextStart;
    struct ImportStatement_S import = statement->importStatement;
    if(ctx->currentScope->type != SCOPE_TYPE_TOPLEVEL){
        semanticError(statement->line, ctx, "Import statement not at top level");
        avStringDebugContextEnd;
        return false;
    }
    AvString importFile = AV_EMPTY;
    avStringClone(&importFile, import.importFile);
    Project* importProject;
    bool32 res = importProjectFile(importFile, import.local, &importProject, import.mappingCount, import.mappings, ctx->projectFileName, ctx);
    if(!res){
        avStringFree(&importFile);
        avStringDebugContextEnd;
        return false;
    }
    statement->importStatement.project = importProject;

    for(uint32 i = 0; i < import.mappingCount; i++){
        struct ImportMapping_S mapping = import.mappings[i];
        enum DefinitionMappingType type = mapping.type;
        if(type & DEFINITION_MAPPING_PROVIDE){
            continue;
        }

        Symbol* sym = resolveSymbol(mapping.symbol, 0, importProject);
        if(!sym){
            res = false;
            continue;
        }
        
        statement->importStatement.mappings[i].resolvedExternal = sym;
        
        if(resolveSymbol(mapping.alias, 0, ctx) != NULL){
            semanticError(statement->line, ctx, "alias %S already defined", mapping.alias);
            res = false;
            continue;
        }

        Symbol symbol = *sym;
        if(!avStringIsEmpty(mapping.alias)){
            avStringUnsafeCopy(&symbol.identifier, mapping.alias);
        }
        symbol.external = true;
        statement->importStatement.mappings[i].resolvedSymbol = avDynamicArrayGetPtr(avDynamicArrayAdd(&symbol, ctx->currentScope->symbols), ctx->currentScope->symbols);
    }

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

    if(resolveSymbol(inherit.variable, 0, ctx)){
        semanticError(statement->line, ctx, "variable %S already defined", inherit.variable);
        ret = false;
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

    bool32 found = false;
    Project* project = ctx->parent;
    Symbol* symbol = NULL;
    while(project){
        symbol = resolveSymbol(inherit.variable, 0, project);
        if(symbol && symbol->type==SYMBOL_VARIABLE){
            found = true;
        }
        project = project->parent;
    }

    if(!found){
        if(!inherit.defaultValue){
            semanticError(statement->line, ctx, "inherited value for %S not found and default not provided", inherit.variable);
            ret = false;
        }else{
            Symbol sym = {
                .type = SYMBOL_VARIABLE,
                .identifier = inherit.variable,
            };
            if((symbol = declareSymbol(sym, ctx))==NULL){
                ret = false;
            }
        }
    }else{
        symbol = avDynamicArrayGetPtr(avDynamicArrayAdd(symbol, ctx->currentScope->symbols), ctx->currentScope->symbols);
        symbol->external = true;
    }
    statement->inheritStatement.resolvedSymbol = symbol;

    return ret;

    // Symbol s = {
    //     .identifier = statement->inheritStatement.variable,
    //     .type = SYMBOL_VARIABLE,
    //     .constValue = true,
    // };
    // Symbol* sym = &s;
    
    // if((sym = declareSymbol(*sym, ctx))==NULL){
    //     semanticError(statement->line, ctx, "variable %S already defined", inherit.variable);
    //     ret = false;
    // }
    // statement->inheritStatement.resolvedSymbol = sym;
    // while(project){
    //     Symbol* symbol = resolveSymbol(inherit.variable, 0, project);
    //     if(symbol && symbol->type == SYMBOL_VARIABLE){
    //         found = true;
    //         statement->inheritStatement.resolvedSymbol = symbol;
    //     }
    //     project = project->parent;
    // }
    // statement->inheritStatement.resolvedSymbol->external = true;

    // if(inherit.defaultValue){
    //     struct ExpressionFlags flags = {0};
    //     if(!analyseExpression(inherit.defaultValue, statement->line, &flags, ctx)){
    //         ret = false;
    //     }
    //     if(flags.constant==false){
    //         semanticError(statement->line, ctx, "default value of inherited variable %S is not constant expression", inherit.variable);
    //         ret = false;
    //     }
    // }
    // if(!inherit.defaultValue && !found){
    //     semanticError(statement->line, ctx, "inherited value for %S not found and default not provided", inherit.variable);
    //     ret = false;
    // }
    // return ret;
}

bool32 analyseVariableDefinition(struct Statement_S* statement, Project* ctx){
    struct VariableDefinition_S var = statement->variableDefinition;
    bool32 ret = true;
    
    
    if(var.size && var.size->type != EXPRESSION_TYPE_NONE){
        struct ExpressionFlags flags = {0};
        if(!analyseExpression(var.size, statement->line, &flags, ctx)){
            ret = false;
        }
        // if(flags.constant==false){
        //     semanticError(statement->line, ctx, "specified size of variable %S is not constant expression", var.identifier);
        //     ret = false;
        // }
    }
    bool32 constant = false;
    if(var.initialValue.type != EXPRESSION_TYPE_NONE){
        struct ExpressionFlags flags = {0};
        if(!analyseExpression(&statement->variableDefinition.initialValue, statement->line, &flags, ctx)){
            ret = false;
        }
        if(!isInFunction(ctx) && flags.constant == false){
            semanticError(statement->line, ctx, "initial value of toplevel variable %S is not constant expression", var.identifier);
            ret = false;
        }
        constant = flags.constant;
    }
    
    Symbol symbol = {.type = SYMBOL_VARIABLE, .identifier = var.identifier, .constValue = constant};
    if((statement->variableDefinition.resolvedSymbol = declareSymbol(symbol, ctx))==NULL){
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

    statement->returnStatement.returnDepth = getScopeTypeDepth(SCOPE_TYPE_FUNCTION, ctx);
    Scope* functionScope = getFunctionScope(ctx);
    statement->returnStatement.resolvedVirtualSymbol = avDynamicArrayGetPtr(0, functionScope->symbols);
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
        case STATEMENT_TYPE_FOREACH:
            return analyseForeach(statement, ctx);
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
    project->skipScope = false;
    enterScope(SCOPE_TYPE_TOPLEVEL, NULL, project);
    project->toplevelScope = project->currentScope;

    for(uint32 i = 0; i < builtInVariableCount; i++){
		struct BuiltInVariableDescription var = builtInVariables[i];
        Symbol symbol = {.type=SYMBOL_VARIABLE, .builtin=true, .constant=true, .constValue=true, .identifier=var.identifier, .variable.constValue=var.value};
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
    declareSymbol((Symbol){.type=SYMBOL_VARIABLE, .builtin = true, .constant = true, .constValue=true, .identifier=AV_CSTR("PROJECT_NAME"),.variable = {.constValue= (struct Value){.type=VALUE_TYPE_STRING,.asString=project->name}}}, project);
    declareSymbol((Symbol){.type=SYMBOL_VARIABLE, .builtin = true, .constant = true, .constValue=true, .identifier=AV_CSTR("PROJECT_DIR"),.variable = {.constValue= currentDir(project, 0, nullptr)}}, project);
    declareSymbol((Symbol){.type=SYMBOL_VARIABLE, .builtin = true, .constant = true, .constValue=true, .identifier=AV_CSTR("PLATFORM"),.variable = {.constValue= (struct Value){.type=VALUE_TYPE_STRING,.asString=platform}}}, project);

    bool32 ret = true;
    for(uint32 i = 0; i < project->statementCount; i++){
        struct Statement_S* statement = project->statements+i;
        if(!analyseStatement(statement, project)){
            ret = false;
        }
    }

    exitScope(project);
    project->currentScope = project->toplevelScope;
    return ret;
}