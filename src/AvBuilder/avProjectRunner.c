#include "avBuilder.h"
#include <AvUtils/avMemory.h>
#include <AvUtils/logging/avAssert.h>
#include <AvUtils/dataStructures/avFMap.h>
#include <AvUtils/filesystem/avDirectoryV2.h>
#include <AvUtils/string/avChar.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <AvUtils/string/avRegex.h>

#define AV_DYNAMIC_ARRAY_EXPOSE_MEMORY_LAYOUT
#include <AvUtils/dataStructures/avDynamicArray.h>

#include "avProjectLang.h"
#include "builtIn/avBuilderBuiltIn.h"

#define NULL_VALUE (struct Value){0}

void printValue(struct Value value);

void runtimeError(Project* project, const char* message, ...){
    va_list args;
    va_start(args, message);

    avStringPrintf(AV_CSTR("Runtime Error:\n\t"));
    avStringPrintfVA(AV_CSTR(message), args);

    avStringPrintf(AV_CSTR("\nVariables: [\n"));

    LocalContext* context = project->localContext;

    while(context){
        if(avDynamicArrayGetSize(context->variables) > 0){
            avDynamicArrayForEachElement(struct VariableDescription, context->variables, {
                struct VariableDescription var = element;
                avStringPrintf(AV_CSTR("\t%s = "), var.identifier);
                printValue(*var.value);
                avStringPrint(AV_CSTR("\n"));
            });
            if(context->previous){
                avStringPrint(AV_CSTR("], [\n"));
            }
        }
        if(!context->inherit){
            break;
        }
        context = context->previous;
    }
    avStringPrintf(AV_CSTR("]\n"));

    avStringPrintf(AV_CSTR("Globals: [\n"));
    avDynamicArrayForEachElement(struct VariableDescription, project->variables, {
        struct VariableDescription var = element;
        avStringPrintf(AV_CSTR("\t%s = "), var.identifier);
        printValue(*var.value);
        avStringPrint(AV_CSTR("\n"));
    });
    avStringPrintf(AV_CSTR("]\n"));

    avStringPrintf(AV_CSTR("Externals: [\n"));
    avDynamicArrayForEachElement(struct VariableDescription, project->externals, {
        struct VariableDescription var = element;
        avStringPrintf(AV_CSTR("\t%s\n"), var.identifier);
    });
    avStringPrintf(AV_CSTR("]\n"));

    avAssert(false, "runtime error");
}

static uint32 parseNumber(AvString string){
    

    enum NumberType {
        NUMBER_TYPE_DECIMAL,
        NUMBER_TYPE_HEXADECIMAL,
        NUMBER_TYPE_BINARY,
        NUMBER_TYPE_OCTAL,
    };
    enum NumberType type = NUMBER_TYPE_DECIMAL;
    uint32 offset = 0;
    if(string.len > 2 && string.chrs[0] == '0'){
        switch(string.chrs[1]){
            case 'x':
            case 'X':
                type = NUMBER_TYPE_HEXADECIMAL;
                offset = 2;
                if(string.len < 3){
                    avAssert(false, "invalid number");
                    return -1;
                }
            break;
            case 'b':
            case 'B':
                type = NUMBER_TYPE_BINARY;
                offset = 2;
                if(string.len < 3){
                    avAssert(false, "invalid number");
                    return -1;
                }
            break;
            default:
                if(avCharIsNumber(string.chrs[1])){
                    type = NUMBER_TYPE_OCTAL;
                    offset = 1;
                }
            break;
        }
    }

    uint32 length = string.len - offset;
    const char* chr = string.chrs+offset;
    uint32 value = 0;

    for(uint32 i = 0; length != 0; length--){
        switch(type){
            case NUMBER_TYPE_DECIMAL:
                value *= 10;
                value += chr[i]-'0';
            break;
            case NUMBER_TYPE_HEXADECIMAL:
                value *= 16;
                value += (avCharIsNumber(chr[i]))?(chr[i]-'0'):(
                    (avCharToUppercase(chr[i])-'A')+10
                );
            break;
            case NUMBER_TYPE_BINARY:
                value <<= 1;
                value |= chr[i]-'0';
            break;
            case NUMBER_TYPE_OCTAL:
                value *= 8;
                value += chr[i]-'0';
            break;
        }

        i++;
    }

    return value;
}

struct Value getValue(struct Expression_S* expression, Project* project);

void toConstValue(struct Value value, struct ConstValue* val, Project* project){
    val->type = value.type;
    switch(value.type){
        case VALUE_TYPE_ARRAY:
            runtimeError( project,"nested arrays are not allowed");
            return;
        case VALUE_TYPE_STRING:
            memcpy(&val->asString, &value.asString, sizeof(value.asString));
        break;
        case VALUE_TYPE_NUMBER:
            memcpy(&val->asNumber, &value.asNumber, sizeof(value.asNumber));
        break;
        case VALUE_TYPE_NONE:
            avAssert(false,"logic error");
        break;
    }
}

void toValue(struct ConstValue value, struct Value* val){
    val->type = value.type;
    switch(value.type){
        case VALUE_TYPE_STRING:
            memcpy(&val->asString, &value.asString, sizeof(value.asString));
        break;
        case VALUE_TYPE_NUMBER:
            memcpy(&val->asNumber, &value.asNumber, sizeof(value.asNumber));
        break;
        case VALUE_TYPE_ARRAY:
        case VALUE_TYPE_NONE:
            avAssert(false,"logic error");
        break;
    }
}

struct ArrayValue getArray(struct ArrayExpression_S array, Project* project){
    struct ArrayValue arr = { 
        .count = array.length, 
        .values = avAllocatorAllocate(sizeof(struct ConstValue)*array.length, &project->allocator),
    };
    avDynamicArrayAdd(&arr.values, project->arrays);
    for(uint32 i = 0; i < array.length; i++){
        struct ConstValue value = {0};
        toConstValue(getValue(array.elements + i, project), &value, project);
        memcpy(arr.values+i, &value, sizeof(struct ConstValue));
    }
    return arr;
}

struct Value performUnary(struct UnaryExpression_S expression, Project* project){
    switch(expression.operator){
        case UNARY_OPERATOR_MINUS:
            struct Value value = getValue(expression.expression, project);
            if(value.type != VALUE_TYPE_NUMBER){
                runtimeError( project,"unary minus operator not defined for types other than number");
                return (struct Value){0};
            }
            value.asNumber = -value.asNumber;
            return value;
        break;
        case UNARY_OPERATOR_NONE:
            avAssert(false, "should not reach here");
            break;
    }
    return NULL_VALUE;
}

struct Value concatenateStrings(struct Value left, struct Value right, Project* project){
    char buffer[4096] = {0};
    AvString lstr = {0};
    AvString rstr = {0};
    if(left.type == VALUE_TYPE_NUMBER){
        avStringPrintfToBuffer(buffer, 4095, AV_CSTR("%i"), left.asNumber);
        AvString str = AV_CSTR(buffer);
        memcpy(&lstr, &str, sizeof(AvString));
    }else{
        memcpy(&lstr, &left.asString, sizeof(AvString));
    }
    if(right.type == VALUE_TYPE_NUMBER){
        avStringPrintfToBuffer(buffer, 4095, AV_CSTR("%i"), right.asNumber);
        AvString str = AV_CSTR(buffer);
        memcpy(&rstr, &str, sizeof(AvString));
    }else{
        memcpy(&rstr, &right.asString, sizeof(AvString));
    }
    uint64 len = lstr.len + rstr.len;
    char* mem = avAllocatorAllocate(len+1, &project->allocator);
    memcpy(mem, lstr.chrs, lstr.len);
    memcpy(mem+lstr.len, rstr.chrs, rstr.len);
    AvString str = {
        .chrs = mem,
        .len = len,
        .memory = nullptr,
    };
    return (struct Value){
        .type= VALUE_TYPE_STRING,
        .asString = str,
    };
}


struct Value performSummation(struct SummationExpression_S expression, Project* project){
    uint32 value = 0;
    struct Value left = getValue(expression.left, project);
    
    struct Value right = getValue(expression.right, project);
    
    if(left.type != VALUE_TYPE_NUMBER && left.type != VALUE_TYPE_STRING){
        runtimeError( project,"add operator not defined for types other than number or string");
        return (struct Value){0};
    }
    if(right.type != VALUE_TYPE_NUMBER && right.type != VALUE_TYPE_STRING){
        runtimeError( project,"add operator not defined for types other than number or string");
        return (struct Value){0};
    }

    switch(expression.operator){
        case SUMMATION_OPERATOR_ADD:
            if(left.type == VALUE_TYPE_STRING || right.type == VALUE_TYPE_STRING){
                return concatenateStrings(left, right, project);
            }
            value = left.asNumber + right.asNumber;
        break;
        case SUMMATION_OPERATOR_SUBTRACT:
            if(left.type == VALUE_TYPE_STRING || right.type == VALUE_TYPE_STRING){
                runtimeError(project, "cannot subtract strings");
                return NULL_VALUE;
            }
            value = left.asNumber - right.asNumber;
        break;
        case SUMMATION_OPERATOR_NONE:
            avAssert(false, "should not reach here");
            break;
    }
    return (struct Value){
        .type = VALUE_TYPE_NUMBER,
        .asNumber = value,
    };

}

struct Value performMultiplication(struct MultiplicationExpression_S expression, Project* project){
    uint32 value = 0;
    struct Value left = getValue(expression.left, project);
    if(left.type != VALUE_TYPE_NUMBER){
        runtimeError( project,"unary minus operator not defined for types other than number");
        return (struct Value){0};
    }
    struct Value right = getValue(expression.right, project);
    if(right.type != VALUE_TYPE_NUMBER){
        runtimeError( project,"unary minus operator not defined for types other than number");
        return (struct Value){0};
    }
    switch(expression.operator){
        case MULTIPLICATION_OPERATOR_MULTIPLY:
            value = left.asNumber * right.asNumber;
        break;
        case MULTIPLICATION_OPERATOR_DIVIDE:
            value = left.asNumber / right.asNumber;
        break;
        case MULTIPLICATION_OPERATOR_NONE:
            avAssert(false, "should not reach here");
            break;
    }
    return (struct Value){
        .type = VALUE_TYPE_NUMBER,
        .asNumber = value,
    };
}

__warnattr("not implmented")
struct VariableDescription importVariable(struct ImportDescription import, Project* project){
    avAssert(false, "not implemented yet");
    return (struct VariableDescription){0};
}

struct VariableDescription findVariable(AvString identifier, Project* project){

    LocalContext* context = project->localContext;
    while(context){
        uint32 localVariableCount = avDynamicArrayGetSize(context->variables);
        for(uint32 i = 0; i < localVariableCount; i++){
            struct VariableDescription var = (struct VariableDescription){0};
            avDynamicArrayRead(&var, i, context->variables);
            if(avStringEquals(identifier, var.identifier)){
                return var;
            }
        }
        if(!context->inherit){
            break;
        }
        context = context->previous;
    }

    uint32 existingVariableCount = avDynamicArrayGetSize(project->variables);
    for(uint32 i = 0; i < existingVariableCount; i++){
        struct VariableDescription var = (struct VariableDescription){0};
        avDynamicArrayRead(&var, i, project->variables);
        if(avStringEquals(identifier, var.identifier)){
            return var;
        }
    }
    uint32 externalCount = avDynamicArrayGetSize(project->externals);
    for(uint32 i = 0; i < externalCount; i++){
        struct ImportDescription ext = (struct ImportDescription){0};
        avDynamicArrayRead(&ext, i, project->externals);
        if(avStringEquals(identifier, ext.identifier)){
            return importVariable(ext, project);
        }
    }
    return (struct VariableDescription){0};
}

struct Value retrieveVariableValue(struct IdentifierExpression_S identifier, Project* project){
    struct VariableDescription description = findVariable(identifier.identifier, project);
    if(!description.project){
        runtimeError( project,"unable to find variable '%s'", identifier.identifier);
        return NULL_VALUE;
    }
    if(description.value){
        return *description.value;
    }
    avAssert(description.statement <= description.project->statementCount, "error in reading import project");
    struct Statement_S* statement = (description.project->statements[description.statement]);
    if(statement->type != STATEMENT_TYPE_VARIABLE_ASSIGNMENT){
        runtimeError( project,"importing variable of wrong type");
        return NULL_VALUE;
    }
    return getValue(statement->variableAssignment.value, project);
}

static void addFilesInPath(AvString directory, AvPathRef root, bool32 recursive, AvDynamicArray files, Project* project){
    AvPath path = AV_EMPTY;
   if(!avDirectoryOpen(directory, root, &path)){
        runtimeError(project, "unable to open directory %s", directory);
        return;
    }
    
    for(uint32 i = 0; i <path.contentCount; i++){
        AvPathNode node = path.content[i];
        
        switch(node.type){
            case AV_PATH_NODE_TYPE_FILE:
                AvString str = {0};
                avStringCopyToAllocator(node.fullName, &str, &project->allocator);
                avDynamicArrayAdd(&str, files);
                break;
            case AV_PATH_NODE_TYPE_DIRECTORY:
                if(recursive){
                    addFilesInPath(node.name, &path, recursive, files, project);
                }
                break;
            case AV_PATH_NODE_TYPE_NONE:
                runtimeError(project, "unknown type file %s", node.name);
                break;
        }
    }
    
    avDirectoryClose(&path);
}

struct Value enumerateFiles(struct EnumerationExpression_S enumeration, Project* project){

    
    struct Value directory = getValue(enumeration.directory, project);
    struct ConstValue constDirectory = (struct ConstValue){0};

    uint32 directoryCount = 0;
    struct ConstValue* directories = nullptr;
    if(directory.type == VALUE_TYPE_ARRAY){
        directoryCount = directory.asArray.count;
        directories = directory.asArray.values;
    }else{
        struct ConstValue constDir = {0};
        toConstValue(directory, &constDir, project);
        directoryCount = 1;
        memcpy(&constDirectory, &constDir, sizeof(struct ConstValue));
        directories = &constDirectory;
    }

    AvDynamicArray files = AV_EMPTY;
    avDynamicArrayCreate(0, sizeof(AvString), &files);

    for(uint32 i = 0; i < directoryCount; i++){
        struct ConstValue dirValue = directories[i];
        if(dirValue.type!=VALUE_TYPE_STRING){
            runtimeError(project, "invalid directory");
            continue;
        }
        AvString dir = dirValue.asString;
        addFilesInPath(dir, nullptr, enumeration.recursive, files, project);
    }

    uint32 fileCount = avDynamicArrayGetSize(files);

    struct Value value = {
        .type = VALUE_TYPE_NONE,
        .asString = AV_EMPTY
    };
    if(fileCount == 0){
        goto end;
    }
    if(fileCount == 1){
        value.type = VALUE_TYPE_STRING;
        avDynamicArrayRead(&value.asString, 0, files);
        goto end;
    }
    value.type = VALUE_TYPE_ARRAY;
    value.asArray.count = fileCount;
    value.asArray.values = avAllocatorAllocate(sizeof(struct ConstValue)*fileCount, &project->allocator);
    avDynamicArrayReadRange(value.asArray.values, fileCount, offsetof(struct ConstValue,asString), sizeof(struct ConstValue), 0, files);
    avDynamicArrayForEachElement(AvString, files, {
        value.asArray.values[index].type = VALUE_TYPE_STRING;
    });
end:
    avDynamicArrayDestroy(files);
    return value;
}

__warnattr("not implemented")
struct Value filterValues(struct FilterExpression_S filter, Project* project){

    struct Value value = getValue(filter.expression, project);
    struct ConstValue tmpValue = {0};
    struct ConstValue* values = &tmpValue;
    uint32 count = 1;
    if(value.type==VALUE_TYPE_ARRAY){
        count = value.asArray.count;
        values = value.asArray.values;
    }else{
        toConstValue(value, &tmpValue, project);
    }
    uint32 filterCount = filter.filterCount;
    struct ConstValue** filters = avCallocate(filter.filterCount, sizeof(struct ConstValue),"filters");
    struct ConstValue* tmpValues = avCallocate(filter.filterCount, sizeof(struct ConstValue),"filters");
    uint32* filterSizes = avCallocate(filter.filterCount, sizeof(uint32), "filterSizes");
    
    for(uint32 i = 0; i < filter.filterCount; i++){
        struct Value filt = getValue(filter.filters+i, project);
        if(filt.type == VALUE_TYPE_ARRAY){
            filterSizes[i] = filt.asArray.count;
            filters[i] = filt.asArray.values;
        }else{
            toConstValue(filt, tmpValues+i, project);
            filterSizes[i] = 1;
            filters[i] = tmpValues+i;
        }
    }

    bool8* isAllowed = avCallocate(count, sizeof(bool8), "allocating filter");
    for(uint32 i = 0; i < count; i++){
        struct ConstValue val = values[i];
        bool8 allowed = true;
        for(uint32 level = 0; level < filterCount; level++){
            bool8 filterPassed = false;
            for(uint32 j = 0; j < filterSizes[level]; j++){
                struct ConstValue filt = filters[level][j];
                if(filt.type==VALUE_TYPE_NUMBER){
                    if(i==filt.asNumber){
                        filterPassed = true;
                        break;
                    }
                }
                if(filt.type==VALUE_TYPE_STRING){
                    char buffer[4096];
                    AvString str = val.asString;
                    if(val.type==VALUE_TYPE_NUMBER){
                        avStringPrintfToBuffer(buffer, 4095, AV_CSTR("%i"), val.asNumber);
                        AvString buff = AV_CSTR(buffer);
                        memcpy(&str, &buff, sizeof(AvString));
                    }
                    AvRegexResult res = avRegexMatch(filt.asString, str, nullptr);
                    if(res.matched){
                        filterPassed = true;
                        break;
                    }
                }
            }
            if(!filterPassed){
                allowed = false;
                break;
            }
        }
        isAllowed[i] = allowed;
    }
    
    avFree(filters);
    avFree(tmpValues);
    avFree(filterSizes);
    avFree(isAllowed);

    return NULL_VALUE;
}

__warnattr("not implemented")
struct FunctionDescription importFunction(struct ImportDescription import, Project* project){
    runtimeError(project, "importing functions is not implemented yet, was trying to import %s from %s", import.identifier, import.importLocation);
    return (struct FunctionDescription){0};
}

struct FunctionDescription findFunction(AvString identifier, Project* project){
    uint32 existingFunctionCount = avDynamicArrayGetSize(project->functions);
    for(uint32 i = 0; i < existingFunctionCount; i++){
        struct FunctionDescription func = (struct FunctionDescription){0};
        avDynamicArrayRead(&func, i, project->functions);
        if(avStringEquals(identifier, func.identifier)){
            return func;
        }
    }
    uint32 externalCount = avDynamicArrayGetSize(project->externals);
    for(uint32 i = 0; i < externalCount; i++){
        struct ImportDescription ext = (struct ImportDescription){0};
        avDynamicArrayRead(&ext, i, project->externals);
        if(avStringEquals(identifier, ext.identifier)){
            return importFunction(ext, project);
        }
    }
    return (struct FunctionDescription){0};
}

void assignVariable(struct VariableDescription description, struct Value value, Project* project);
void performPerform(struct PerformStatementBody_S perform, Project* project);

void performForeach(struct ForeachStatement_S foreach, Project* project){

    struct Value collection = getValue(foreach.collection, project);
    struct VariableDescription collectionVar = {
        .identifier = foreach.variable,
        .project = project,
        .statement = -1,
    };
    struct VariableDescription indexVar = {
        .identifier = foreach.index,
        .project = project,
        .statement = -1,
    };

    uint32 count = 0;
    struct ConstValue* values = nullptr;
    struct ConstValue tmpValue = {0};
    switch(collection.type){
        case VALUE_TYPE_NUMBER:
        case VALUE_TYPE_STRING:
            count = 1;
            toConstValue(collection,&tmpValue, project);
            values = &tmpValue;
        break;
        case VALUE_TYPE_ARRAY:
            count = collection.asArray.count;
            values = collection.asArray.values;
            break;
        case VALUE_TYPE_NONE:
            avAssert(false,"logic error");
            break;
    }
    for(uint32 i = 0; i < count; i++){
        startLocalContext(project, true);
        struct Value value = NULL_VALUE;
        toValue(values[i], &value);
        assignVariable(collectionVar, value, project);

        if(foreach.index.len){
            assignVariable(indexVar, (struct Value){.type=VALUE_TYPE_NUMBER, .asNumber=i}, project);
        }

        performPerform(foreach.performStatement, project);

        endLocalContext(project);
    }
    

}
struct Value callFunction(struct CallExpression_S call, Project* project);
void runVariableAssignment(struct VariableAssignment_S statement, uint32 index, Project* project);

void performCommand(struct CommandStatementBody_S command, Project* project){
    startLocalContext(project, true);

    for(uint32 i = 0; i < command.statementCount; i++){
        struct CommandStatement_S statement = command.statements[i];
        switch(statement.type){
            case COMMAND_STATEMENT_FUNCTION_CALL:
                callFunction(statement.functionCall, project);
                break;
            case COMMAND_STATEMENT_VARIABLE_ASSIGNMENT:
                runVariableAssignment(statement.variableAssignment, -1, project);
                break;
            case COMMAND_STATEMENT_NONE:
                avAssert(false, "logic error");
                break;
        }
    }

    //findVariable

    endLocalContext(project);
}

void addVariableToContext(struct VariableDescription description, Project* project);

void performVariableDefinition(struct VariableDefinition_S variable, Project* project){
    struct VariableDescription var = findVariable(variable.identifier, project);
    if(var.project){
        runtimeError(project, "Variable %s already defined", variable.identifier);
        return;
    }
    struct Value size = {0};
    if(variable.size == nullptr){
         struct Value tmpVal = {
            .type = VALUE_TYPE_NUMBER,
            .asNumber = 1,
         };
         memcpy(&size, &tmpVal, sizeof(struct Value));
    }else{
        struct Value tmpVal = getValue(variable.size, project);
        memcpy(&size, &tmpVal, sizeof(struct Value));
    }
    
    if(size.type != VALUE_TYPE_NUMBER){
        runtimeError(project, "Variable definition array can only be dimensioned with a number");
        return;
    }
    if(size.asNumber==0){
        runtimeError(project, "Array size is not allowed to be 0 (for now)");
        return;
    }
    if(size.asNumber==1){
        addVariableToContext((struct VariableDescription){
            .identifier = variable.identifier,
            .project = project,
            .statement = -1,
            .value = nullptr,
        }, project);
        return;
    }
    struct Value* value = avAllocatorAllocate(sizeof(struct Value), &project->allocator);
    value->type = VALUE_TYPE_ARRAY;
    value->asArray = (struct ArrayValue){
        .count = size.asNumber,
        .values = avAllocatorAllocate(sizeof(struct Value)*size.asNumber, &project->allocator)
    };
    addVariableToContext((struct VariableDescription){
        .identifier = variable.identifier,
        .project = project,
        .statement = -1,
        .value = value,
    }, project);
}

void performPerform(struct PerformStatementBody_S perform, Project* project){
    startLocalContext(project, true);

    for(uint32 i = 0; i < perform.statementCount; i++){

        struct PerformStatement_S statement = perform.statements[i];
        switch(statement.type){
            case PERFORM_OPERATION_TYPE_FUNCTION_CALL:
                callFunction(statement.functionCall, project);
                break;
            case PERFORM_OPERATION_TYPE_VARIABLE_ASSIGNMENT:
                runVariableAssignment(statement.variableAssignment, -1, project);
                break;
            case PERFORM_OPERATION_TYPE_COMMAND:
                performCommand(statement.commandStatement, project);
                break;
            case PERFORM_OPERATION_TYPE_VARIABLE_DEFINITION:
                performVariableDefinition(statement.variableDefinition, project);
                break;
            case PERFORM_OPERATION_TYPE_NONE:
                avAssert(false, "logic error");
                break;
            
        }
    }
    endLocalContext(project);
}

struct Value runFunction(struct FunctionDefinition_S function, Project* project){
    struct Value value = {.type=VALUE_TYPE_NUMBER, .asNumber=0 };
    bool32 done = false;
    for(uint32 i = 0; i < function.statementCount; i++){
        struct FunctionStatement_S statement = function.statements[i];
        switch(statement.type){
            case FUNCTION_STATEMENT_TYPE_FOREACH:
                performForeach(statement.foreachStatement, project);
                break;
            case FUNCTION_STATEMENT_TYPE_PERFORM:
                performPerform(statement.performStatement, project);
                break;
            case FUNCTION_STATEMENT_TYPE_RETURN:
                struct Value returnValue = getValue(statement.returnStatement.value, project);
                memcpy(&value, &returnValue, sizeof(struct Value));
                done = true;
                break;
            case FUNCTION_STATEMENT_TYPE_VAR_DEFINITION:
                performVariableDefinition(statement.variableDefinition, project);
                break;

            case FUNCTION_STATEMENT_TYPE_NONE:
                avAssert(false, "invalid statement");
                break;
        }
        if(done){
            break;
        }
    }
    return value;
}

void assignVariable(struct VariableDescription description, struct Value value, Project* project);

struct Value callFunction(struct CallExpression_S call, Project* project){

    struct Value* values = avAllocate(sizeof(struct Value)*call.argumentCount, "allocating tempArgs");
    for(uint32 i = 0; i < call.argumentCount; i++){
        struct Value tmpValue = getValue(call.arguments+i, project);
        memcpy(values+i, &tmpValue, sizeof(struct Value));
    }

    struct BuiltInFunctionDescription builtIn = {0};
    if(isBuiltInFunction(&builtIn, call.function, project)){
        struct Value value = callBuiltInFunction(builtIn, call.argumentCount, values, project);
        avFree(values);
        return value;
    }

    struct FunctionDescription description = findFunction(call.function, project);
    if(!description.project){
        runtimeError( project,"unable to find function '%s'", call.function);
        return NULL_VALUE;
    }
    avAssert(description.statement <= description.project->statementCount, "error in reading import project");
    struct Statement_S* statement = (description.project->statements[description.statement]);
    if(statement->type != STATEMENT_TYPE_FUNCTION_DEFINITION){
        runtimeError( project,"importing function of wrong type");
        return NULL_VALUE;
    }
    struct FunctionDefinition_S function = statement->functionDefinition;
    if(function.parameterCount != call.argumentCount){
        runtimeError( project,"invalid number of arguments in function call");
    }

    
    startLocalContext(project, false);
    for(uint32 i = 0; i < call.argumentCount; i++){
        struct Value value = values[i];
        struct VariableDescription variable = {
            .identifier = function.parameters[i],
            .project = project,
            .statement = description.statement,
        };
        assignVariable(variable, value, project);
    }
    avFree(values);
    struct Value returnValue = runFunction(function, project);
    endLocalContext(project);

    return returnValue;
}

struct Value getValue(struct Expression_S* expression, Project* project){
    switch(expression->type){
        case EXPRESSION_TYPE_NONE:
            avStringPrintf(AV_CSTR("Warning: null value\n"));
            return (struct Value){
                .type = VALUE_TYPE_STRING,
                .asString = {
                    .chrs = nullptr,
                    .len = 0,
                    .memory = nullptr,
                },
            };
        case EXPRESSION_TYPE_LITERAL:
            return (struct Value){
                .type = VALUE_TYPE_STRING,
                .asString = (AvString) {
                    .chrs = expression->literal.value.chrs + 1,
                    .len = expression->literal.value.len - 2,
                    .memory = nullptr,
                },
            };
        case EXPRESSION_TYPE_NUMBER:
            return (struct Value){
                .type = VALUE_TYPE_NUMBER,
                .asNumber = parseNumber(expression->number.value),
            };
        case EXPRESSION_TYPE_ARRAY:
            return (struct Value){
                .type = VALUE_TYPE_ARRAY,
                .asArray = getArray(expression->array, project),
            };
        case EXPRESSION_TYPE_GROUPING:
            return getValue(expression->grouping.expression, project);
        case EXPRESSION_TYPE_UNARY:
            return performUnary(expression->unary, project);
        case EXPRESSION_TYPE_SUMMATION:
            return performSummation(expression->summation, project);
        case EXPRESSION_TYPE_MULTIPLICATION:
            return performMultiplication(expression->multiplication, project);
        case EXPRESSION_TYPE_IDENTIFIER:
            return retrieveVariableValue(expression->identifier, project);
        case EXPRESSION_TYPE_ENUMERATION:
            return enumerateFiles(expression->enumeration, project);
        case EXPRESSION_TYPE_FILTER:
            return filterValues(expression->filter, project);
        case EXPRESSION_TYPE_CALL:
            return callFunction(expression->call, project);
    }
    return NULL_VALUE;
}
void addVariableToContext(struct VariableDescription description, Project* project);

void assignVariable(struct VariableDescription description, struct Value value, Project* project){
    struct Value* val = avAllocatorAllocate(sizeof(struct Value), &project->allocator);
    memcpy(val, &value, sizeof(struct Value));
    description.value = val;
    LocalContext* context =  project->localContext;
    while(context){
        avDynamicArrayForEachElement(struct VariableDescription, context->variables, {
            if(avStringEquals(description.identifier, element.identifier)){
                avDynamicArrayWrite(&description, index, context->variables);
                return;
            }
        });
        if(!context->inherit){
            break;
        }
        context = context->previous;
    }
    avDynamicArrayForEachElement(struct VariableDescription, project->variables, {
        if(avStringEquals(description.identifier, element.identifier)){
            avDynamicArrayWrite(&description, index, project->variables);
            return;
        }
    });
    addVariableToContext(description, project);
}

struct Value appendValue(struct Value valueA, struct Value valueB, Project* project){
    struct ArrayValue array = {0};
    if(valueA.type == VALUE_TYPE_ARRAY){
        array.count += valueA.asArray.count;
    }else{
        array.count += 1;
    }
    if(valueB.type == VALUE_TYPE_ARRAY){
        array.count += valueB.asArray.count;
    }else{
        array.count += 1;
    }
    array.values = avAllocate(sizeof(struct ConstValue)*array.count, "allocating array");
    uint32 index = 0;
    if(valueA.type == VALUE_TYPE_ARRAY){
        memcpy(array.values, valueA.asArray.values, sizeof(struct ConstValue)*valueA.asArray.count);
        index += valueA.asArray.count;
    }else{
        struct ConstValue value = {0};
        toConstValue(valueA, &value, project);
        memcpy(array.values, &value, sizeof(struct ConstValue));
        index += 1;
    }
    if(valueB.type == VALUE_TYPE_ARRAY){
        memcpy(array.values+index, valueB.asArray.values, sizeof(struct ConstValue)*valueB.asArray.count);
    }else{
        struct ConstValue value = {0};
        toConstValue(valueB, &value, project);
        memcpy(array.values + index, &value, sizeof(struct ConstValue));
    }
    return (struct Value) {
        .type = VALUE_TYPE_ARRAY,
        .asArray = array,
    };
}

void assignVariableIndexed(struct AvString identifier, uint32 index, struct Value value, Project* project){
    LocalContext* context =  project->localContext;
    AvDynamicArray variables = nullptr;
    uint32 varIndex = -1;
    while(context){
        avDynamicArrayForEachElement(struct VariableDescription, context->variables, {
            if(avStringEquals(identifier, element.identifier)){
                variables = context->variables;
                varIndex = index;
                break;
            }
        });
        if(!context->inherit || varIndex != -1){
            break;
        }
        context = context->previous;
    }
    if(varIndex != -1){
        avDynamicArrayForEachElement(struct VariableDescription, project->variables, {
            if(avStringEquals(identifier, element.identifier)){
                variables = context->variables;
                varIndex = index;
                break;
            }
        });
    }
    if(varIndex == -1){
        runtimeError(project, "accessing unknown variable '%s' with index", identifier);
        return;
    }
    struct VariableDescription* variable = avDynamicArrayGetPtr(varIndex, variables);
    if(!variable->value){
        runtimeError(project, "variable '%s' not initialized", identifier);
        return;
    }
    if((variable->value->type & (VALUE_TYPE_STRING|VALUE_TYPE_NUMBER))!=0){
        memcpy(variable->value, &value, sizeof(struct Value));
        return;
    }
    if(variable->value->type != VALUE_TYPE_ARRAY){
        runtimeError(project, "invalid variable type");
        return;
    }
    if(variable->value->asArray.count <= index){
        runtimeError(project, "accessing array index( %i ) out of bounds ( %i )", variable->value->asArray.count, index);
        return;
    }
    struct ConstValue val = {0};
    toConstValue(value, &val, project);

    memcpy(variable->value->asArray.values+index, &val, sizeof(struct ConstValue));
}

void addVariableToContext(struct VariableDescription description, Project* project){
    if(!project->localContext){
        avDynamicArrayAdd(&description, project->variables);
        return;
    }
    avDynamicArrayAdd(&description, project->localContext->variables);
}

void runVariableAssignment(struct VariableAssignment_S statement, uint32 index, Project* project){
    struct Value value = getValue(statement.value, project);
    
    if(statement.modifier==VARIABLE_ACCESS_MODIFIER_ARRAY){
        struct Value i = getValue(statement.index, project);
        if(i.type != VALUE_TYPE_NUMBER){
            runtimeError(project, "Can only index variable with a number");
            return;
        }

        assignVariableIndexed(statement.variableName, i.asNumber, value, project);
        return;
    }

    struct VariableDescription description = {
        .identifier = statement.variableName,
        .project = project,
        .statement = index,
    };
    assignVariable(description, value, project);
}


void printConstValue(struct ConstValue value){
    if(value.type == VALUE_TYPE_NUMBER){
        avStringPrintf(AV_CSTR("%i"), value.asNumber);
        return;
    }
    if(value.type == VALUE_TYPE_STRING){
        avStringPrint(value.asString);
    }
}

void printValue(struct Value value){
    if(value.type == VALUE_TYPE_NUMBER){
        avStringPrintf(AV_CSTR("%i"), value.asNumber);
        return;
    }
    if(value.type == VALUE_TYPE_STRING){
        avStringPrint(value.asString);
        return;
    }
    if(value.type == VALUE_TYPE_ARRAY){
        avStringPrintln(AV_CSTR("["));
        struct ArrayValue arr = value.asArray;
        for(uint32 i = 0; i < arr.count; i++){
            avStringPrint(AV_CSTR("\t"));
            printConstValue(arr.values[i]);
            avStringPrint(AV_CSTR(",\n"));
        }
        avStringPrint(AV_CSTR("]"));
    }
}

void printHelp(struct FunctionDefinition_S function){

    avStringPrint(AV_CSTR("Invalid number of arguments\nUsage:\n\t"));
    avStringPrintf(AV_CSTR("./avBuilder %s.project"), function.functionName);
    for(uint32 i = 0; i < function.parameterCount; i++){
        avStringPrintf(AV_CSTR(" [%s]"), function.parameters[i]);
    }
    avStringPrintf(AV_CSTR("\n"));
}

uint32 runProject(Project* project, AvDynamicArray arguments){
    
    for(uint32 i = 0; i < project->statementCount; i++){
        struct Statement_S* statement = (project->statements)[i];
        switch(statement->type){
            case STATEMENT_TYPE_IMPORT:
            case STATEMENT_TYPE_FUNCTION_DEFINITION:
                break;
            case STATEMENT_TYPE_VARIABLE_ASSIGNMENT:
                runVariableAssignment(statement->variableAssignment, i, project);
                break;
            case STATEMENT_TYPE_NONE:
                return -1;
                break;
        }

    }

    struct FunctionDescription mainFunction = findFunction(project->name, project);
    if(!mainFunction.project){
        runtimeError( project,"no entry found");
    }
    avAssert(mainFunction.statement < project->statementCount, "invalid function location");
    struct Statement_S* functionStatement = project->statements[mainFunction.statement];
    avAssert(functionStatement->type == STATEMENT_TYPE_FUNCTION_DEFINITION, "malformed entry");
    
    struct FunctionDefinition_S function = functionStatement->functionDefinition;

    uint32 argumentCount = avDynamicArrayGetSize(arguments);
    if(argumentCount != function.parameterCount){
        printHelp(function);
        return -1;
    }

    startLocalContext(project, false);
    for(uint32 i = 0; i < argumentCount; i++){
        AvString strValue = AV_EMPTY;
        avDynamicArrayRead(&strValue, i, arguments);
        struct Value value = {
            .type = VALUE_TYPE_STRING,
            .asString = strValue,
        };
        struct VariableDescription variable = {
            .identifier = function.parameters[i],
            .project = project,
            .statement = mainFunction.statement,
        };
        assignVariable(variable, value, project);
    }
    struct Value returnValue = runFunction(function, project);
    endLocalContext(project);
    if(returnValue.type == VALUE_TYPE_NUMBER){
        return returnValue.asNumber;
    }
    if(returnValue.type == VALUE_TYPE_STRING){
        return returnValue.asString.len == 0;
    }
    if(returnValue.type == VALUE_TYPE_ARRAY){
        return returnValue.asArray.count == 0;
    }
    return -1;
}