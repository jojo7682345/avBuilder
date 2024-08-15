#include "avBuilder.h"
#include <AvUtils/avMemory.h>
#include <AvUtils/logging/avAssert.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "avProjectLang.h"

void scemanticError(const char* message, Project* project){
    project->processState = PROCESS_STATE_SCEMANTIC_ERROR;
    avStringPrintln(AV_CSTR("Scemantic Error:"));
    avStringPrintln(AV_CSTR(message));
}

struct Expression_S* processPrimaryExpression(struct Expression_S* expr, struct Primary* primary, Project* project);
struct Expression_S* processCallExpression(struct Expression_S* expr, struct Call* call, Project* project);
struct Expression_S* processFilterExpression(struct Expression_S* expr, struct Filter* filter, Project* project);
struct Expression_S* processUnaryExpression(struct Expression_S* expr, struct Unary* unary, Project* project);
struct Expression_S* processEnumerationExpression(struct Expression_S* expr, struct Enumeration* enumeration, Project* project);
struct Expression_S* processSummationExpression(struct Expression_S* expr, struct Summation* summation, Project* project);
struct Expression_S* processComparisonExpression(struct Expression_S* expr, struct Comparison* summation, Project* project);
struct Expression_S* processMultiplicationExpression(struct Expression_S* expr, struct Multiplication* multiplication, Project* project);
struct Expression_S* processArrayExpression(struct Expression_S* expr, struct Array* array, Project* project);
struct Expression_S* processExpressionExpression(struct Expression_S* expr, struct Expression* expression, Project* project);

struct ArrayExpression_S processArray(struct Array* array, Project* project){
    uint64 length = 0;
    struct Array* iterator = array;
    while(iterator && iterator->expression){
        length++;
        iterator = iterator->next;
    }
    struct Expression_S* elements = nullptr;
    if(length){
        elements = avAllocatorAllocate(sizeof(struct Expression_S)*length, &project->allocator);
    }
    iterator = array;
    uint32 index = 0;
    while(iterator && iterator->expression){
        processSummationExpression(elements + index, iterator->expression, project);
        index++;
        iterator = iterator->next;
    }
    return (struct ArrayExpression_S ) {
        .length = length,
        .elements = elements
    };
}

struct MultiplicationExpression_S processMultiplication(struct Multiplication* multiplication, Project* project){
    struct Expression_S* left = avAllocatorAllocate(sizeof(struct Expression_S)*2, &project->allocator);
    struct Expression_S* right = left+1;
    left = processEnumerationExpression(left, multiplication->left, project);
    right = processMultiplicationExpression(right, multiplication->right, project);
    return (struct MultiplicationExpression_S){
        .left=left,
        .operator = multiplication->operator,
        .right=right,
    };
}

struct SummationExpression_S processSummation(struct Summation* summation, Project* project){
    struct Expression_S* left = avAllocatorAllocate(sizeof(struct Expression_S)*2, &project->allocator);
    struct Expression_S* right = left+1;
    left = processMultiplicationExpression(left, summation->left, project);
    right = processSummationExpression(right, summation->right, project);
    return (struct SummationExpression_S){
        .left=left,
        .operator = summation->operator,
        .right=right,
    };
}

struct ComparisonExpression_S processComparison(struct Comparison* comparison, Project* project){
    struct Expression_S* left = avAllocatorAllocate(sizeof(struct Expression_S)*2, &project->allocator);
    struct Expression_S* right = left +1;
    left = processArrayExpression(left, comparison->left, project);
    right = processArrayExpression(right, comparison->right, project);
    return (struct ComparisonExpression_S){
        .left=  left,
        .operator = comparison->operator,
        .right = right,
    };
}

struct EnumerationExpression_S processEnumeration(struct Enumeration* enumeration, Project* project){
    struct Expression_S* dir = avAllocatorAllocate(sizeof(struct Expression_S), &project->allocator);
    dir = processUnaryExpression(dir, enumeration->unary, project);
    return (struct EnumerationExpression_S){
        .directory = dir,
        .recursive = enumeration->recursive,
    };
}

struct UnaryExpression_S processUnary(struct Unary* unary, Project* project){
    struct Expression_S* val = avAllocatorAllocate(sizeof(struct Expression_S), &project->allocator);
    if(unary->type == UNARY_TYPE_UNARY){
        val = processUnaryExpression(val, unary->unary, project);
    }else{
        val = processFilterExpression(val, unary->filter, project);
    }
    return (struct UnaryExpression_S){
        .expression = val,
        .operator = unary->operator,
    };
}

struct FilterExpression_S processFilter(struct Filter* filter, Project* project){
    struct Expression_S* left = avAllocatorAllocate(sizeof(struct Expression_S), &project->allocator);
    left = processCallExpression(left, filter->call, project);

    uint64 count = 0;
    struct ArrayList* iterator = filter->filter;
    while(iterator){
        count++;
        iterator = iterator->next;
    }
    struct Expression_S* filterElements = avAllocatorAllocate(sizeof(struct Expression_S)*count, &project->allocator);
    iterator = filter->filter;
    uint32 index = 0;
    while(iterator){
        processArrayExpression(filterElements+index, iterator->array, project);
        index++;
        iterator = iterator->next;
    }

    return (struct FilterExpression_S){
        .expression = left,
        .filterCount = count,
        .filters = filterElements,
    };
}

struct Expression_S* processExpression(struct Expression* expression, Project* project);
struct Expression_S* processPrimaryExpression(struct Expression_S* expr, struct Primary* primary, Project* project){
    if(primary->type == PRIMARY_TYPE_GROUPING){
        expr->type = EXPRESSION_TYPE_GROUPING;
        expr->grouping = (struct GroupExpression_S){.expression=processExpression(primary->grouping, project)};
        return expr;
    }
    if(primary->type == PRIMARY_TYPE_IDENTIFIER){
        expr->type = EXPRESSION_TYPE_IDENTIFIER;
        memcpy(&expr->identifier.identifier, &primary->identifier, sizeof(AvString));
        return expr;
    }
    if(primary->type == PRIMARY_TYPE_LITERAL){
        expr->type = EXPRESSION_TYPE_LITERAL;
        memcpy(&expr->literal.value, &primary->literal, sizeof(AvString));
        return expr;
    }
    if(primary->type == PRIMARY_TYPE_NUMBER){
        expr->type = EXPRESSION_TYPE_NUMBER;
        memcpy(&expr->number.value, &primary->number, sizeof(AvString));
        return expr;
    }

    avAssert(false, "invalid expression");
    return nullptr;
}
/*
struct CallExpression_S processCall(struct Call* call, Project* project){
    struct Expression_S* function = avAllocatorAllocate(sizeof(struct Expression_S), &project->allocator);
    function = processPrimaryExpression(function, call->function, project);

    if(function->type!=EXPRESSION_TYPE_IDENTIFIER){
        scemanticError("It is not allowed to use literal as function identifier", project);
    }

    uint64 count = 0;
    struct Argument* iterator = call->argument;
    while(iterator && iterator->expression){
        count++;
        iterator = iterator->next;
    }
    
    struct Expression_S* values  = nullptr;
    if(count != 0){
        values = avAllocatorAllocate(sizeof(struct Expression_S)*count, &project->allocator);

        iterator = call->argument;
        uint32 index = 0;
        while(iterator && iterator->expression){
            processExpressionExpression(values+index, iterator->expression, project);
            index++;
            iterator = iterator->next;
        }
    }

    return (struct CallExpression_S){
        .function = call->function->identifier,
        .argumentCount = count,
        .arguments = values,
    };
}
*/

bool32 processFunctionCall(struct CallExpression_S* call, struct Call* callStatement, Project* project);
struct Expression_S* processCallExpression(struct Expression_S* expr, struct Call* call, Project* project){
    if(call->argument){
        expr->type = EXPRESSION_TYPE_CALL;
        struct CallExpression_S callExpression = {};
        processFunctionCall(&callExpression, call, project);
        memcpy(&expr->call, &callExpression, sizeof(struct CallExpression_S));
        return expr;
    }

    return processPrimaryExpression(expr, call->function, project);
}

struct Expression_S* processFilterExpression(struct Expression_S* expr, struct Filter* filter, Project* project){
    if(filter->filter){
        expr->type = EXPRESSION_TYPE_FILTER;
        expr->filter = processFilter(filter, project);
        return expr;
    }

    return processCallExpression(expr, filter->call, project);
}

struct Expression_S* processUnaryExpression(struct Expression_S* expr, struct Unary* unary, Project* project){
    if(unary->type == UNARY_TYPE_UNARY){
        expr->type = EXPRESSION_TYPE_UNARY;
        expr->unary = processUnary(unary, project);
        return expr;
    }

    return processFilterExpression(expr, unary->filter, project);
}

struct Expression_S* processEnumerationExpression(struct Expression_S* expr, struct Enumeration* enumeration, Project* project){
    // files in
    if(enumeration->type == ENUMERATION_TYPE_ENUMERATION){
        expr->type = EXPRESSION_TYPE_ENUMERATION;
        expr->enumeration = processEnumeration(enumeration, project);
        return expr;
    }

    return processUnaryExpression(expr, enumeration->unary, project);
}

struct Expression_S* processMultiplicationExpression(struct Expression_S* expr, struct Multiplication* multiplication, Project* project){

    if(multiplication->operator!=MULTIPLICATION_OPERATOR_NONE){
        expr->type = EXPRESSION_TYPE_MULTIPLICATION;
        expr->multiplication = processMultiplication(multiplication, project);
        return expr;
    }

    return processEnumerationExpression(expr, multiplication->left, project);
}

struct Expression_S* processSummationExpression(struct Expression_S* expr, struct Summation* summation, Project* project){

    if(summation->operator!=SUMMATION_OPERATOR_NONE){
        expr->type = EXPRESSION_TYPE_SUMMATION;
        expr->summation = processSummation(summation, project);
        return expr;
    }

    return processMultiplicationExpression(expr, summation->left, project);
}

struct Expression_S* processComparisonExpression(struct Expression_S* expr, struct Comparison* comparison, Project* project){
    if(comparison->operator != COMPARISON_OPERATOR_NONE){
        expr->type = EXPRESSION_TYPE_COMPARISON;
        expr->comparison = processComparison(comparison, project);
        return expr;
    }
    return processArrayExpression(expr, comparison->left, project);
}

struct Expression_S* processArrayExpression(struct Expression_S* expr, struct Array* array, Project* project){
    if(!array){
        return nullptr;
    }
    if(array->next){
        expr->type = EXPRESSION_TYPE_ARRAY;
        expr->array = processArray(array, project);
        return expr;
    }
    if(array->expression){
        return processSummationExpression(expr, array->expression, project);
    }else{
        expr-> type = EXPRESSION_TYPE_ARRAY;
        expr->array = processArray(array, project);
        return expr;
    }
}

//This function is only to keep everything unified
struct Expression_S* processExpressionExpression(struct Expression_S* expr, struct Expression* expression, Project* project){
    return processComparisonExpression(expr, expression->comparison, project);
}

struct Expression_S* processExpression(struct Expression* expression, Project* project){
    struct Expression_S* expr = avAllocatorAllocate(sizeof(struct Expression_S), &project->allocator);
    
    return processExpressionExpression(expr, expression, project);
}

struct Statement_S* processVariableAssignmentStatement(struct VariableAssignment varStatement,Project* project){
    struct Statement_S* statement = avAllocatorAllocate(sizeof(struct Statement_S), &project->allocator);
    statement->type = STATEMENT_TYPE_VARIABLE_ASSIGNMENT;
    statement->variableAssignment.modifier = varStatement.variable->modifier;
    memcpy(&statement->variableAssignment.variableName, &varStatement.variable->name, sizeof(AvString));
    if(varStatement.variable->modifier==VARIABLE_ACCESS_MODIFIER_ARRAY){
        statement->variableAssignment.index = processExpression(varStatement.variable->index, project);
    }
    statement->variableAssignment.value = processExpression(varStatement.expression, project);
    return statement;
}

bool32  processVariableStatement(struct VariableAssignment_S* var, struct VariableAssignment* varStatement, Project* project){
    var->modifier = varStatement->variable->modifier;
    memcpy(&var->variableName, &varStatement->variable->name, sizeof(AvString));
    var->value = processExpression(varStatement->expression, project);
    if(var->modifier==VARIABLE_ACCESS_MODIFIER_ARRAY){
        var->index = processExpression(varStatement->variable->index, project);
    }
    return true;
}

bool32 processFunctionCall(struct CallExpression_S* call, struct Call* callStatement, Project* project){
    memcpy(&call->function, &callStatement->function->identifier, sizeof(AvString));
    uint64 parameterCount = 0;
    struct Argument* iterator = callStatement->argument;
    while(iterator && iterator->expression){
        parameterCount++;
        iterator = iterator->next;
    }
    struct Expression_S* parameters = nullptr;
    if(parameterCount){
        parameters = avAllocatorAllocate(sizeof(struct Expression_S)*parameterCount, &project->allocator);
        uint32 index = 0;
        iterator = callStatement->argument;
        while(iterator && iterator->expression){

            processExpressionExpression(parameters+index, iterator->expression, project);
            
            index++;
            iterator = iterator->next;
        }
    }
    call->argumentCount = parameterCount;
    call->arguments = parameters;
    return true;
}
bool32 processCommandStatementList(struct CommandStatementBody_S* body, struct CommandStatementList* statement, Project* project);
bool32 processIfCommandStatement(struct IfCommandStatement_S* stat, struct IfCommandStatement* statement, Project* project){
    if(statement->check){
        stat->check = processExpression(statement->check, project);
    }
    if(statement->alternativeBranch){
        stat->alternativeBranch = avAllocatorAllocate(sizeof(struct IfCommandStatement_S), &project->allocator);
        if(!processIfCommandStatement(stat->alternativeBranch, statement->alternativeBranch, project)){
            return false;
        }
    }
    stat->branch = avAllocatorAllocate(sizeof(struct CommandStatementBody_S), &project->allocator);
    return processCommandStatementList(stat->branch, statement->branch, project);;
}

bool32 processCommandStatement(struct CommandStatement_S* stat, struct CommandStatement* statement, Project* project){
    if(statement->type == COMMAND_STATEMENT_VARIABLE_ASSIGNMENT){
        stat->type = COMMAND_STATEMENT_VARIABLE_ASSIGNMENT;
        return processVariableStatement(&stat->variableAssignment, statement->variableAssignment, project);
    }
    if(statement->type == COMMAND_STATEMENT_FUNCTION_CALL){
        stat->type = COMMAND_STATEMENT_FUNCTION_CALL;
        return processFunctionCall(&stat->functionCall, statement->functionCall->call, project);
    }
    if(statement->type == COMMAND_STATEMENT_IF_STATEMENT){
        stat->type = COMMAND_STATEMENT_IF_STATEMENT;
        return processIfCommandStatement(&stat->ifStatement, statement->ifStatement, project);
    }
    return false;
}

bool32 processCommandStatementList(struct CommandStatementBody_S* body, struct CommandStatementList* statement, Project* project){
    uint64 statementCount = 0;
    struct CommandStatementList* iterator = statement;
    while(iterator && iterator->commandStatement){
        statementCount++;
        iterator = iterator->next;
    }

    struct CommandStatement_S* statements = avAllocatorAllocate(sizeof(struct CommandStatement_S)*statementCount, &project->allocator);
    iterator = statement;
    uint32 index = 0;
    while(iterator && iterator->commandStatement){

        if(!processCommandStatement(statements + index, iterator->commandStatement, project)){
            return false;
        }

        index++;
        iterator = iterator->next;
    }
    avStringUnsafeCopy(&body->retCodeVariable, &statement->retCodeVariable);
    avStringUnsafeCopy(&body->outputVariable, &statement->outputVariable);
    
    if(statement->retCodeIndex){
        body->retCodeIndex = processExpression(statement->retCodeIndex, project);
    }
    if(statement->outputVariableIndex){
        body->outputVariableIndex = processExpression(statement->outputVariableIndex, project);
    }
    if(statement->pipeFile){
        body->pipeFile = processExpression(statement->pipeFile, project);
    }

    body->statementCount = statementCount;
    body->statements = statements;
    return true;
}

bool32 processVariableDefinitionStatement(struct VariableDefinition_S* stat, struct VariableDefinitionStatement* statement, Project* project){
    memcpy(&stat->identifier, &statement->identifier, sizeof(AvString));
    if(statement->size){
        stat->size = processExpression(statement->size, project);
    }
    return true;
}
bool32 processPerformStatementBody(struct PerformStatementBody_S* stat, struct PerformStatement* statement, Project* project);
bool32 processIfPerformStatement(struct IfPerformStatement_S* stat, struct IfPerformStatement* statement, Project* project){
    if(statement->check){
        stat->check = processExpression(statement->check, project);
    }
    if(statement->alternativeBranch){
        stat->alternativeBranch = avAllocatorAllocate(sizeof(struct IfPerformStatement_S), &project->allocator);
        if(!processIfPerformStatement(stat->alternativeBranch, statement->alternativeBranch, project)){
            return false;
        }
    }
    stat->branch = avAllocatorAllocate(sizeof(struct PerformStatementBody_S), &project->allocator);
    return processPerformStatementBody(stat->branch, &(struct PerformStatement){.performOperationList =statement->branch}, project);
}

bool32 processPerformStatement(struct PerformStatement_S* stat, struct PerformOperation* statement, Project* project){
    
    if(statement->type==PERFORM_OPERATION_TYPE_COMMAND){
        stat->type = PERFORM_STATEMENT_TYPE_COMMAND;
        return processCommandStatementList(&stat->commandStatement, statement->commandStatementList, project);
    }
    if(statement->type==PERFORM_OPERATION_TYPE_VARIABLE_ASSIGNMENT){
        stat->type = PERFORM_STATEMENT_TYPE_VARIABLE_ASSIGNMENT;
        return processVariableStatement(&stat->variableAssignment, statement->variableAssignment, project);
    }
    if(statement->type == PERFORM_OPERATION_TYPE_IF_STATEMENT){
        stat->type = PERFORM_STATEMENT_TYPE_IF_STATEMENT;
        return processIfPerformStatement(&stat->ifStatement, statement->ifStatement, project);
    }
    if(statement->type==PERFORM_OPERATION_TYPE_FUNCTION_CALL){
        stat->type = PERFORM_STATEMENT_TYPE_FUNCTION_CALL;
        return processFunctionCall(&stat->functionCall, statement->functionCall->call, project);
    }
    if(statement->type == PERFORM_OPERATION_TYPE_VARIABLE_DEFINITION){
        stat->type = PERFORM_OPERATION_TYPE_VARIABLE_DEFINITION;
        return processVariableDefinitionStatement(&stat->variableDefinition, statement->varStatement, project);
    }
    avAssert(false, "invalid statement");
    return false;
}

bool32 processPerformStatementBody(struct PerformStatementBody_S* stat, struct PerformStatement* statement, Project* project){
    uint64 statementCount = 0;
    struct PerformOperationList* iterator = statement->performOperationList;
    while(iterator && iterator->performOperation){
        statementCount++;
        iterator = iterator->next;
    }struct PerformStatement_S* statements = nullptr;
    if(statementCount){
        statements = avAllocatorAllocate(sizeof(struct PerformStatement_S)*statementCount, &project->allocator);
        uint32 index = 0;
        iterator = statement->performOperationList;
        while(iterator && iterator->performOperation){
            processPerformStatement(statements+index, iterator->performOperation, project);
            index++;
            iterator = iterator->next;
        }
    }
    stat->statementCount = statementCount;
    stat->statements = statements;
    return true;
}

bool32 processForeachStatement(struct ForeachStatement_S* stat, struct ForeachStatement* statement, Project* project){
    memcpy(&stat->variable, &statement->variable, sizeof(AvString));
    memcpy(&stat->index, &statement->index, sizeof(AvString));
    stat->collection = processExpression(statement->collection, project);
    return processPerformStatementBody(&stat->performStatement, statement->performStatement, project); 
}

bool32 processReturnStatement(struct ReturnStatement_S* stat, struct ReturnStatement* statement, Project* project){
    stat->value = processExpression(statement->value, project);
    return true;
}
struct FunctionStatement_S* processFunctionStatement(struct FunctionStatement_S* stat, struct FunctionStatement* statement, Project* project);
bool32 processFunctionStatementBody(struct FunctionBody_S* body, struct FunctionStatementList* list, Project* project){

        uint64 statementCount = 0;
        struct FunctionStatementList* iterator = list;
        while(iterator && iterator->functionStatement){
            statementCount++;
            iterator = iterator->next;
        }
        struct FunctionStatement_S* statements =nullptr;
        if(statementCount){
            statements = avAllocatorAllocate(sizeof(struct FunctionStatement_S)*statementCount, &project->allocator);
        }
        uint64 index = 0;
        iterator = list;
        while(iterator && iterator->functionStatement){
            processFunctionStatement(statements+index, iterator->functionStatement, project);
            index++;
            iterator = iterator->next;
        }
        body->statementCount = statementCount;
        body->statements = statements;

        return true;
}

bool32 processIfFunctionStatement(struct IfFunctionStatement_S* stat, struct IfFunctionStatement* statement, Project* project){
    if(statement->check){
        stat->check = processExpression(statement->check, project);
    }
    if(statement->alternativeBranch){
        stat->alternativeBranch = avAllocatorAllocate(sizeof(struct IfFunctionStatement_S),&project->allocator);
        if(!processIfFunctionStatement(stat->alternativeBranch, statement->alternativeBranch, project)){
            return false;
        }
    }
    stat->branch = avAllocatorAllocate(sizeof(struct FunctionBody_S), &project->allocator);
    return processFunctionStatementBody(stat->branch, statement->branch, project);;
}

struct FunctionStatement_S* processFunctionStatement(struct FunctionStatement_S* stat, struct FunctionStatement* statement, Project* project){
    if(statement->type == FUNCTION_STATEMENT_TYPE_FOREACH){
        stat->type = FUNCTION_STATEMENT_TYPE_FOREACH;
        processForeachStatement(&stat->foreachStatement,statement->foreachStatement, project);
        return stat;
    }
    if(statement->type == FUNCTION_STATEMENT_TYPE_PERFORM){
        stat->type = FUNCTION_STATEMENT_TYPE_PERFORM;
        processPerformStatementBody(&stat->performStatement, statement->performStatement, project);
        return stat;
    }
    if(statement->type == FUNCTION_STATEMENT_TYPE_RETURN){
        stat->type= FUNCTION_STATEMENT_TYPE_RETURN;
        processReturnStatement(&stat->returnStatement, statement->returnStatement, project);
        return stat;
    }
    if(statement->type == FUNCTION_STATEMENT_TYPE_IF){
        stat->type = FUNCTION_STATEMENT_TYPE_IF;
        processIfFunctionStatement(&stat->ifStatement, statement->ifStatement, project);
        return stat;
    }
    if(statement->type == FUNCTION_STATEMENT_TYPE_VAR_DEFINITION){
        stat->type = FUNCTION_STATEMENT_TYPE_VAR_DEFINITION;
        processVariableDefinitionStatement(&stat->variableDefinition, statement->varStatement, project);
        return stat;
    }
    avAssert(false, "invalid statement");
    return nullptr;
}



struct Statement_S* processFunctionDefinitionStatement(struct FunctionDefinition function, Project* project){
    struct Statement_S* statement = avAllocatorAllocate(sizeof(struct Statement_S), &project->allocator);
    statement->type = STATEMENT_TYPE_FUNCTION_DEFINITION;
    memcpy(&statement->functionDefinition.functionName, &function.name, sizeof(AvString));
    
    {
        uint64 parameterCount = 0;
        struct ParameterList* iterator = function.parameterList;
        while(iterator && iterator->parameter){
            parameterCount++;
            iterator = iterator->next;
        }
        if(parameterCount!=0){
            AvString* parameters = avAllocatorAllocate(sizeof(AvString)*parameterCount, &project->allocator);
            uint64 index = 0;
            iterator = function.parameterList;
            while(iterator && iterator->parameter){
                memcpy(parameters+index, &iterator->parameter->name, sizeof(AvString));
                index++;
                iterator = iterator->next;
            }
            statement->functionDefinition.parameters = parameters;
        }
        statement->functionDefinition.parameterCount = parameterCount;
    }
    
    processFunctionStatementBody(&statement->functionDefinition.body, function.functionStatementList, project);
    return statement;
}

bool32 processImportMapping(struct ImportMapping_S* map, struct DefinitionMapping* mapping, Project* project){
    memcpy(&map->symbol, &mapping->symbol, sizeof(AvString));
    if(mapping->symbolAlias.len){
        memcpy(&map->alias, &mapping->symbolAlias, sizeof(AvString));
    }else{
        memcpy(&map->alias, &mapping->symbol, sizeof(AvString));
    }
    map->type = mapping->type;
    return true;
}

struct Statement_S* processInheritStatement(struct InheritStatement inherit, Project* project){
    struct Statement_S* statement = avAllocatorAllocate(sizeof(struct Statement_S), &project->allocator);
    statement->type = STATEMENT_TYPE_INHERIT;
    avStringUnsafeCopy(&statement->inheritStatement.variable, &inherit.variable);
    if(inherit.defaultValue){
        statement->inheritStatement.defaultValue = processExpression(inherit.defaultValue, project);
    }
    return statement;
}

struct Statement_S* processImportStatement(struct ImportStatement import, Project* project){
    struct Statement_S* statement = avAllocatorAllocate(sizeof(struct Statement_S), &project->allocator);
    statement->type = STATEMENT_TYPE_IMPORT;
    statement->importStatement.local = import.local;
    memcpy(&statement->importStatement.importFile, &import.file, sizeof(AvString));
    uint64 mappingCount = 0;
    struct DefinitionMappingList* iterator = import.definitionMappingList;
    while(iterator && iterator->definitionMapping){
        mappingCount++;
        iterator = iterator->next;
    }

    struct ImportMapping_S* mappings = avAllocatorAllocate(sizeof(struct ImportMapping_S)*mappingCount, &project->allocator);
    uint32 index = 0;
    iterator = import.definitionMappingList;
    while(iterator && iterator->definitionMapping){
        processImportMapping(mappings+index, iterator->definitionMapping, project);
        index++;
        iterator = iterator->next;
    }
    statement->importStatement.mappingCount = mappingCount;
    statement->importStatement.mappings = mappings;

    return statement;
}

static bool32 checkVariablePreviouslyDefined(AvString symbol, Project* project){
    avDynamicArrayForEachElement(struct ImportDescription, project->externals, {
        if(avStringEquals(element.identifier, symbol)){
            return true;
        }
    });
    avDynamicArrayForEachElement(struct FunctionDescription, project->functions, {
        if(avStringEquals(element.identifier, symbol)){
            return true;
        }
    });
    return false;
};

static bool32 checkPreviouslyDefined(AvString symbol, Project* project){
    if(checkVariablePreviouslyDefined(symbol, project)){
        return true;
    }
    avDynamicArrayForEachElement(struct VariableDescription, project->variables, {
        if(avStringEquals(element.identifier, symbol)){
            return true;
        }
    });
    return false;
};

bool32 processProject(void* statements, Project* project){
    struct ProjectStatementList* statementList = statements;
    project->statementCount = 0;
    while(statementList && statementList->statement){
        (project->statementCount)++;
        statementList = statementList->next;
    }

    project->statements = avAllocatorAllocate(sizeof(struct Statement_S*)*(project->statementCount), &project->allocator);
    uint32 index = 0;
    statementList = statements;
    while(statementList && statementList->statement){
        struct ProjectStatement statement = *statementList->statement;
        struct Statement_S* stat = nullptr;

        switch(statement.type){
            case PROJECT_STATEMENT_TYPE_INCLUDE:
                stat = processImportStatement(*statement.importStatement, project);
                break;
            case PROJECT_STATEMENT_TYPE_INHERIT:
                stat = processInheritStatement(*statement.inheritStatement, project);
                break;
            case PROJECT_STATEMENT_TYPE_VARIABLE_ASSIGNMENT:
                stat = processVariableAssignmentStatement(*statement.variableAssignment, project);
                break;
            case PROJECT_STATEMENT_TYPE_FUNCTION_DEFINITION:
                stat = processFunctionDefinitionStatement(*statement.functionDefinition, project);
                break;
            case PROJECT_STATEMENT_TYPE_NONE:
            default:
                avAssert(false, "Invalid project statement");
                break;
        }
        (project->statements)[index] = stat;

        index++;
        statementList = statementList->next;
    }

    for(uint32 i = 0; i < project->statementCount; i++){
        struct Statement_S* statement = (project->statements)[i];

        switch(statement->type){
            case STATEMENT_TYPE_IMPORT:{
                struct ImportStatement_S import = statement->importStatement;
                for(uint32 j = 0; j < import.mappingCount; j++){
                    struct ImportMapping_S mapping = import.mappings[j];
                    if(mapping.type & DEFINITION_MAPPING_PROVIDE){
                        struct ImportDescription external = (struct ImportDescription){
                            .identifier = {.chrs = mapping.alias.chrs+1, .len= mapping.alias.len-2 },
                            .extIdentifier = { .chrs = mapping.symbol.chrs+1, .len= mapping.symbol.len-2 },
                            .importFile = { .chrs = import.importFile.chrs+1, .len = import.importFile.len-2 },
                            .isLocalFile = !(mapping.type & DEFINITION_MAPPING_GLOBAL),
                        };
                        avDynamicArrayAdd(&external, project->libraryAliases);
                        continue;
                    }
                    if(checkPreviouslyDefined(mapping.alias, project)){
                        avStringPrintf(AV_CSTR("Multiple Definitions found of %s\n"), mapping.alias);
                        return false;
                    }
                    struct ImportDescription external = (struct ImportDescription){
                        .identifier = mapping.alias,
                        .extIdentifier = mapping.symbol,
                        .importFile = {
                            .chrs = import.importFile.chrs + 1,
                            .len = import.importFile.len - 2,
                        },
                        .isLocalFile = import.local,
                    };
                    avDynamicArrayAdd(&external, project->externals);
                }
                break;
            }
            case STATEMENT_TYPE_FUNCTION_DEFINITION:{
                struct FunctionDefinition_S function = statement->functionDefinition;
                if(checkPreviouslyDefined(function.functionName, project)){
                    avStringPrintf(AV_CSTR("Multiple Definitions found of %s\n"), function.functionName);
                    return false;
                }
                struct FunctionDescription func = (struct FunctionDescription){
                    .identifier = function.functionName,
                    .statement = i,
                    .project = project,
                };
                avDynamicArrayAdd(&func, project->functions);
                break;
            }
            case STATEMENT_TYPE_INHERIT:
            case STATEMENT_TYPE_VARIABLE_ASSIGNMENT:
            break;
            default:
                return false;
                break;
        }

    }


    return (project->processState == PROCESS_STATE_OK);

}