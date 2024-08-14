#include "avBuilder.h"
#include <AvUtils/avMemory.h>
#include <AvUtils/logging/avAssert.h>
#include <AvUtils/dataStructures/avFMap.h>
#include <AvUtils/filesystem/avDirectoryV2.h>
#include <AvUtils/string/avChar.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <AvUtils/avProcess.h>
#include <AvUtils/avEnvironment.h>
#include <AvUtils/process/avPipe.h>

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
            for(uint32 index = 0; index < avDynamicArrayGetSize(context->variables); index++) { 
                struct VariableDescription element; avDynamicArrayRead(&element, index, (context->variables)); { 
                    struct VariableDescription var = element; 
                    avStringPrintf(AV_CSTR("\t%s = "), var.identifier); 
                    if(var.value){
                        printValue(*var.value); 
                    }else{
                        avStringPrint(AV_CSTR("NULL"));
                    }
                    avStringPrint(AV_CSTR("\n")); 
                } 
            }
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
    for(uint32 index = 0; index < avDynamicArrayGetSize(project->variables); index++) {
         struct VariableDescription element; 
         avDynamicArrayRead(&element, index, (project->variables)); 
         { 
            struct VariableDescription var = element; 
            avStringPrintf(((AvString){
                .chrs="\t%s = ", 
                .len=avCStringLength("\t%s = "), 
                .memory=((AvStringMemory*)0)
            }), var.identifier); 
            if(var.value){
                printValue(*var.value); 
            }
            avStringPrint(((AvString){
                .chrs="\n", .len=avCStringLength("\n"), .memory=((AvStringMemory*)0)
            })); 
        } 
    };
    avStringPrintf(AV_CSTR("]\n"));

    avStringPrintf(AV_CSTR("Constants: [\n"));
    avDynamicArrayForEachElement(struct VariableDescription, project->constants, {
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
        .values = array.length ? avAllocatorAllocate(sizeof(struct ConstValue)*array.length, &project->allocator) : nullptr,
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
        case UNARY_OPERATOR_MINUS:{
            struct Value value = getValue(expression.expression, project);
            if(value.type != VALUE_TYPE_NUMBER){
                runtimeError( project,"unary minus operator not defined for types other than number");
                return (struct Value){0};
            }
            value.asNumber = -value.asNumber;
            return value;
        }
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

struct Value performComparison(struct ComparisonExpression_S expression, Project* project){
    struct Value left = getValue(expression.left, project);
    struct Value right = getValue(expression.right, project);

    if((left.type & (VALUE_TYPE_STRING|VALUE_TYPE_NUMBER)) != (right.type & (VALUE_TYPE_STRING|VALUE_TYPE_NUMBER)) && left.type != VALUE_TYPE_ARRAY){
        runtimeError(project, "Comparing two different types is not allowed");
        return (struct Value){
            .type = VALUE_TYPE_NUMBER,
            .asNumber = 0,
        };
    }
    if(left.type == VALUE_TYPE_NONE){
        runtimeError(project, "comparing null value");
        return (struct Value){
            .type = VALUE_TYPE_NUMBER,
            .asNumber = 0,
        };
    }
    if(right.type == VALUE_TYPE_ARRAY){
        runtimeError(project, "Arrays must be the left operant");
        return (struct Value){
            .type = VALUE_TYPE_NUMBER,
            .asNumber = 0,
        };
    }
    if(left.type == VALUE_TYPE_ARRAY && left.type == right.type){
        runtimeError(project, "Comparing two arrays is not allowed");
        return (struct Value){
            .type = VALUE_TYPE_NUMBER,
            .asNumber = 0,
        };
    }
    if(left.type == VALUE_TYPE_ARRAY){
        struct ArrayValue array = left.type==VALUE_TYPE_ARRAY?left.asArray:right.asArray;
        struct Value val = left.type==VALUE_TYPE_ARRAY?right:left;
        if(array.count == 0){
            return (struct Value){
                .type = VALUE_TYPE_NUMBER,
                .asNumber = 0,
            }; 
        }
        struct ConstValue* values = avAllocatorAllocate(sizeof(struct ConstValue)*array.count, &project->allocator);
        for(uint32 i = 0; i < array.count; i++){
            struct ConstValue v = array.values[i];
            uint32 value = 0;
            if(v.type != val.type){
                runtimeError(project, "Comparing two different types is not allowed");
                continue;
            }
            switch(val.type){
                case VALUE_TYPE_NUMBER:
                    switch(expression.operator){
                        case COMPARISON_OPERATOR_EQUALS:
                            value = v.asNumber==val.asNumber;
                            break;
                        case COMPARISON_OPERATOR_NOT_EQUALS:
                            value = v.asNumber!=val.asNumber;
                            break;
                        case COMPARISON_OPERATOR_LESS_THAN:
                            value = v.asNumber<val.asNumber;
                            break;
                        case COMPARISON_OPERATOR_GREATER_THAN:
                            value = v.asNumber>val.asNumber;
                            break;
                        case COMPARISON_OPERATOR_LESS_THAN_OR_EQUAL:
                            value = v.asNumber<=val.asNumber;
                            break;
                        case COMPARISON_OPERATOR_GREATER_THAN_OR_EQUAL:
                            value = v.asNumber>=val.asNumber;
                            break;
                        default:
                            runtimeError(project, "invalid comparison operator");
                            break;
                    }
                    values[i].type = VALUE_TYPE_NUMBER;
                    values[i].asNumber = value;
                    continue;   
                case VALUE_TYPE_STRING:
                    switch(expression.operator){
                        case COMPARISON_OPERATOR_EQUALS:
                            value = avStringEquals(v.asString, val.asString);
                            break;
                        case COMPARISON_OPERATOR_NOT_EQUALS:
                            value = !avStringEquals(v.asString, val.asString);
                            break;
                        default:
                            runtimeError(project, "invalid comparison operator");
                            break;
                    }
                    values[i].type = VALUE_TYPE_NUMBER;
                    values[i].asNumber = value;
                    continue;
                default:
                    runtimeError(project, "invalid value");
                    return (struct Value){
                        .type = VALUE_TYPE_NUMBER,
                        .asNumber = 0,
                    };
            }
        }
        return (struct Value){
            .type = VALUE_TYPE_ARRAY,
            .asArray = {
                .count = array.count,
                .values = values,
            },
        };
    }

    if(left.type == VALUE_TYPE_NUMBER){
        uint32 value = 0;
        switch(expression.operator){
            case COMPARISON_OPERATOR_EQUALS:
                value = left.asNumber==right.asNumber;
                break;
            case COMPARISON_OPERATOR_NOT_EQUALS:
                value = left.asNumber!=right.asNumber;
                break;
            case COMPARISON_OPERATOR_LESS_THAN:
                value = left.asNumber<right.asNumber;
                break;
            case COMPARISON_OPERATOR_GREATER_THAN:
                value = left.asNumber>right.asNumber;
                break;
            case COMPARISON_OPERATOR_LESS_THAN_OR_EQUAL:
                value = left.asNumber<=right.asNumber;
                break;
            case COMPARISON_OPERATOR_GREATER_THAN_OR_EQUAL:
                value = left.asNumber>=right.asNumber;
                break;
            default:
                runtimeError(project, "invalid comparison operator");
                break;
        }
        return (struct Value){
            .type = VALUE_TYPE_NUMBER,
            .asNumber = value,
        };
    }
    if(left.type == VALUE_TYPE_STRING){
        uint32 value = 0;
        switch(expression.operator){
            case COMPARISON_OPERATOR_EQUALS:
                value = avStringEquals(left.asString, right.asString);
                break;
            case COMPARISON_OPERATOR_NOT_EQUALS:
                value = !avStringEquals(left.asString, right.asString);
                break;
            default:
                runtimeError(project, "invalid comparison operator");
                break;
        }
        return (struct Value){
            .type = VALUE_TYPE_NUMBER,
            .asNumber = value,
        };
    }

    runtimeError(project, "logic error");
    return (struct Value){
        .type = VALUE_TYPE_NUMBER,
        .asNumber = 0,
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

void assignConstant(struct VariableDescription description, struct Value value, Project* project);
void assignVariable(struct VariableDescription description, struct Value value, Project* project);

void addVariableToContext(struct VariableDescription description, Project* project);
void addVariableToGlobalContext(struct VariableDescription description, Project* project);

struct VariableDescription findVariableInGlobalScope(AvString identifier, Project* project);

Project* importProject(AvString projectFile, bool32 local, Project* baseProject){
    avStringDebugContextStart;

    AvString projectFileStr = AV_EMPTY;
    if(!local){
        AvString homeDir = AV_EMPTY;
        if(!avGetEnvironmentVariable(AV_CSTR("HOME"), &homeDir)){
            runtimeError(baseProject, "Could not get HOME environment variable");
            return nullptr;
        }
        avStringJoin(&projectFileStr, homeDir, AV_CSTRA("/"), configPath, templatePath, projectFile);
        avStringFree(&homeDir);
    }else{
        avStringClone(&projectFileStr, projectFile);
    }

    AvString projectFileContent = AV_EMPTY;
    AvString projectFileName = AV_EMPTY;
    if(!loadProjectFile(projectFileStr, &projectFileContent, &projectFileName)){
        avStringPrintf(AV_CSTR("Failed to load project file %s\n"), projectFileStr);
        goto loadingFailed;
    }

    AV_DS(AvDynamicArray, Token) tokens = AV_EMPTY;
    avDynamicArrayCreate(0, sizeof(Token), &tokens);
    if(!tokenizeProject(projectFileContent, projectFileName, tokens)){
        avStringPrintf(AV_CSTR("Failed to tokenize project file %s\n"), projectFileStr);
        goto tokenizingFailed;
    }
    //printTokenList(tokens);
   
    Project* project = avAllocatorAllocate(sizeof(Project), &baseProject->allocator);
    avDynamicArrayAdd(&project, baseProject->importedProjects);

    projectCreate(project, projectFileName, projectFileStr, projectFileContent);
    struct ProjectStatementList* statements = nullptr;
    if(!parseProject(tokens, (void**)&statements, project)){
        avStringPrintf(AV_CSTR("Failed to parse project file %s\n"), projectFileStr);
        goto parsingFailed;
    }
    
    if(!processProject(statements, project)){
        avStringPrintf(AV_CSTR("Failed to perform processing on project file %s\n"), projectFileStr);
        goto processingFailed;
    }

    uint32 providesCount = avDynamicArrayGetSize(baseProject->libraryAliases);
    for(uint32 i = 0; i < providesCount; i++){
        struct ImportDescription* description = avDynamicArrayGetPtr(i, baseProject->libraryAliases);
        if(!avStringEquals(projectFile, description->importFile)){
            continue;
        }
        uint32 libraryImportCount = avDynamicArrayGetSize(project->externals);
        for(uint32 j = 0; j < libraryImportCount; j++){
            struct ImportDescription* mapping = avDynamicArrayGetPtr(j, project->externals);
            if(avStringEquals(mapping->importFile, description->extIdentifier)){
                avStringFree(&mapping->importFile);
                avStringUnsafeCopy(&mapping->importFile, &description->identifier);
            }
        }
    }

    for(uint32 i = 0; i < builtInVariableCount; i++){
        struct BuiltInVariableDescription var = builtInVariables[i];
        assignConstant((struct VariableDescription){
            .identifier = var.identifier,
            .project = project,
            .statement = -1,
        }, var.value, project);
    }

    assignConstant((struct VariableDescription){
            .identifier = AV_CSTR("PROJECT_NAME"),
            .project = project,
            .statement = -1,
    }, (struct Value){.type=VALUE_TYPE_STRING,.asString=project->name}, project);

    for(uint32 i = 0; i < project->statementCount; i++){
        struct Statement_S* statement = (project->statements)[i];
        switch(statement->type){
            case STATEMENT_TYPE_IMPORT:
            case STATEMENT_TYPE_FUNCTION_DEFINITION:
                break;
            case STATEMENT_TYPE_INHERIT:
                struct VariableDescription var = findVariableInGlobalScope(statement->inheritStatement.variable, baseProject);
                if(var.project){
                    if(!var.value){
                        if(!statement->inheritStatement.defaultValue){
                            runtimeError(project, "inheriting variable %s defined in parent project, but has no value", var.identifier);
                            break;
                        }
                        assignVariable(var, getValue(statement->inheritStatement.defaultValue, project), project);
                        break;
                    }
                    addVariableToGlobalContext((struct VariableDescription){
                        .identifier = statement->inheritStatement.variable,
                        .project = project,
                        .statement = i,
                        .value = var.value,
                    }, project);
                    break;
                }else{
                    if(statement->inheritStatement.defaultValue){
                        assignVariable((struct VariableDescription){
                            .identifier = statement->inheritStatement.variable,
                            .project = project,
                            .statement = i,
                        }, getValue(statement->inheritStatement.defaultValue, project), project);
                        break;
                    }
                    runtimeError(project, "inheriting variable %s but not defined in parent project", var.identifier);
                    break;
                }
                break;
            case STATEMENT_TYPE_VARIABLE_ASSIGNMENT:
                addVariableToContext((struct VariableDescription){
                    .identifier = statement->variableAssignment.variableName,
                    .project = project,
                    .statement = i,
                    .value = nullptr,
                }, project);
                break;
            case STATEMENT_TYPE_NONE:
                avAssert(false, "logic error");
                goto processingFailed;
        }
    }
    avDynamicArrayDestroy(tokens);
    avStringFree(&projectFileStr);
    memcpy(&project->options, &baseProject->options, sizeof(struct ProjectOptions));
    avStringFree(&projectFileName);
    avStringDebugContextEnd;
    return project;

processingFailed:
parsingFailed:
    projectDestroy(project);
tokenizingFailed:
    avDynamicArrayDestroy(tokens);
loadingFailed:
    avStringFree(&projectFileName); 
    avStringFree(&projectFileContent);
    avStringFree(&projectFileStr);
    avStringDebugContextEnd;
    return nullptr;
}

struct VariableDescription findVariable(AvString identifier, Project* project);

struct VariableDescription importVariable(struct ImportDescription import, Project* project){

    bool32 found = false;
    Project* extProject = nullptr;
    for(uint32 index = 0; index < avDynamicArrayGetSize(project->importedProjects); index++) { 
        Project* element; 
        avDynamicArrayRead(&element, index, (project->importedProjects)); 
        { 
            if(avStringEquals(import.importFile, element->projectFileName)){
                found = true;
                extProject = element;
                break;
            }
        }
    }

    if(!found){
        extProject = importProject(import.importFile, import.isLocalFile, project);
    }
    if(!extProject){
        runtimeError(project, "failed to import project file %s", import.importFile);
        return (struct VariableDescription) {0};
    }

    return findVariable(import.extIdentifier, extProject);
}

struct VariableDescription findVariableInGlobalScope(AvString identifier, Project* project){
    uint32 existingVariableCount = avDynamicArrayGetSize(project->variables);
    for(uint32 i = 0; i < existingVariableCount; i++){
        struct VariableDescription var = (struct VariableDescription){0};
        avDynamicArrayRead(&var, i, project->variables);
        if(avStringEquals(identifier, var.identifier)){
            return var;
        }
    }
    uint32 existingConstantCount = avDynamicArrayGetSize(project->constants);
    for(uint32 i = 0; i < existingConstantCount; i++){
        struct VariableDescription var = (struct VariableDescription){0};
        avDynamicArrayRead(&var, i, project->constants);
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

    return findVariableInGlobalScope(identifier, project);
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
    return getValue(statement->variableAssignment.value, description.project);
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
            case AV_PATH_NODE_TYPE_FILE:{
                AvString str = {0};
                avStringCopyToAllocator(node.fullName, &str, &project->allocator);
                avDynamicArrayAdd(&str, files);
                break;
            }
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
                    runtimeError(project, "filtering with strings is not supported");
                    filterPassed = false;
                }
            }
            if(!filterPassed){
                allowed = false;
                break;
            }
        }
        isAllowed[i] = allowed;
    }

    AvDynamicArray newValues = {0};
    avDynamicArrayCreate(0, sizeof(struct ConstValue), &newValues);
    bool32 allowedCount = 0;
    for(uint32 i = 0; i < count; i++){
        if(isAllowed[i]){
            avDynamicArrayAdd(values+i, newValues);
            allowedCount++;
        }
    }

    struct ConstValue* filteredValues = nullptr;
    if(allowedCount > 0){
        filteredValues = avAllocatorAllocate(sizeof(struct ConstValue)*allowedCount, &project->allocator);
        avDynamicArrayReadRange(filteredValues, allowedCount, 0, sizeof(struct ConstValue), 0, newValues);
    }
    struct Value filtered = {
        .type = VALUE_TYPE_ARRAY,
        .asArray.count = allowedCount,
        .asArray.values = filteredValues,
    };
    if(allowedCount==1){
        toValue(filteredValues[0], &filtered);
    }

    avDynamicArrayDestroy(newValues);
    avFree(filters);
    avFree(tmpValues);
    avFree(filterSizes);
    avFree(isAllowed);

    return filtered;
}

struct FunctionDescription findFunction(AvString identifier, Project* project);
struct FunctionDescription importFunction(struct ImportDescription import, Project* project){
    bool32 found = false;
    Project* extProject = nullptr;
    for(uint32 index = 0; index < avDynamicArrayGetSize(project->importedProjects); index++) { 
        Project* element; 
        avDynamicArrayRead(&element, index, (project->importedProjects)); 
        { 
            if(avStringEquals(import.importFile, element->projectFileName)){
                found = true;
                extProject = element;
                break;
            }
        }
    }

    if(!found){
        extProject = importProject(import.importFile, import.isLocalFile, project);
    }
    if(!extProject){
        runtimeError(project, "failed to import project file %s", import.importFile);
        return (struct FunctionDescription) {0};
    }

    return findFunction(import.extIdentifier, extProject);
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

enum NodeType{
    TYPE_STRING,
    TYPE_NESTED,
};
struct Node {
    enum NodeType type;
    union {
        AvString string;
        AvDynamicArray nested;
    };
};
struct CommandDescription {
    AvDynamicArray args;
    char* command;
};

uint32 processArg(AvString arg, AvDynamicArray chars, Project* project){
    uint32 start = avDynamicArrayGetSize(chars);
    bool32 ignoreNext = false;
    for(uint32 i = 0; i < arg.len; i++){
        char c = arg.chrs[i];
        if(c=='$' && !ignoreNext){
            uint32 j = i+1;
            for(; j < arg.len; j++){
                char chr = arg.chrs[j];
                if(!(avCharIsLetter(chr) || (j-i > 2 && avCharIsNumber(chr)) || chr=='_')){
                    break;
                }
            }
            AvString varName = {
                .chrs = arg.chrs + i + 1,
                .len = j - i - 1,
                .memory = nullptr,
            };
            struct VariableDescription var = findVariable(varName, project);
            if(!var.value){
                if(!var.project){
                    runtimeError(project, "unable to find variable %s", varName);
                    goto invalidValue;
                }
                if(var.statement >= var.project->statementCount){
                    goto invalidValue;
                }
                struct Statement_S* statement = var.project->statements[var.statement];
                if(statement->type != STATEMENT_TYPE_VARIABLE_ASSIGNMENT){
                    goto invalidValue;
                }
                if(!statement->variableAssignment.value){
                    goto invalidValue;
                }
                struct Value* value = avAllocatorAllocate(sizeof(struct Value), &project->allocator);
                struct Value tmpValue = getValue(statement->variableAssignment.value, project);
                memcpy(value, &tmpValue, sizeof(struct Value));
                var.value = value;
                invalidValue:
            }
            if(!var.project || var.value==nullptr || var.value->type==VALUE_TYPE_NONE){
                avDynamicArrayAddRange((char*)varName.chrs-1, varName.len+1, 0, 1, chars);
                i = j-1;
                continue;
            }
            struct Value value = *var.value;
            if(value.type == VALUE_TYPE_STRING){
                processArg(value.asString, chars, project);
                i = j-1;
                continue;
            }
            if(value.type == VALUE_TYPE_NUMBER){
                char buffer[256] = {0};
                avStringPrintfToBuffer(buffer, sizeof(buffer)-1, AV_CSTR("%i"), value.asNumber);
                avDynamicArrayAddRange(buffer, avCStringLength(buffer), 0, 1, chars);
                i = j-1;
                continue;
            }
            if(value.type == VALUE_TYPE_ARRAY){
                uint32 count = value.asArray.count;
                struct ConstValue* values = value.asArray.values;
                for(uint32 k = 0; k < count; k++){
                    struct ConstValue val = values[k];
                    bool32 isEmpty = true;
                    if(val.type == VALUE_TYPE_STRING){
                        if(processArg(val.asString, chars, project)){
                            isEmpty = false;
                        }
                    }
                    if(val.type == VALUE_TYPE_NUMBER){
                        char buffer[256] = {0};
                        avStringPrintfToBuffer(buffer, sizeof(buffer)-1, AV_CSTR("%i"), val.asNumber);
                        avDynamicArrayAddRange(buffer, avCStringLength(buffer), 0, 1, chars);
                        isEmpty = true;
                    }
                    if(k < count -1 && !isEmpty){
                        char tmpChar = ' ';
                        avDynamicArrayAdd(&tmpChar, chars);
                    }
                }

                i = j-1;
            }

            continue;
        }
        if(c=='*' && !ignoreNext){
            uint32 j = i+1;
            for(; j < arg.len; j++){
                char chr = arg.chrs[j];
                if(!(avCharIsLetter(chr) || (j-i > 2 && avCharIsNumber(chr)) || chr=='_')){
                    break;
                }
            }
            AvString varName = {
                .chrs = arg.chrs + i + 1,
                .len = j - i - 1,
                .memory = nullptr,
            };
            struct VariableDescription var = findVariable(varName, project);
            if(!var.project || var.value==nullptr || var.value->type==VALUE_TYPE_NONE){
                avDynamicArrayAddRange((char*)varName.chrs-1, varName.len+1, 0, 1, chars);
                i = j-1;
                continue;
            }
            struct Value value = *var.value;
            if(value.type == VALUE_TYPE_STRING){
                processArg(value.asString, chars, project);
                i = j-1;
                continue;
            }
            if(value.type == VALUE_TYPE_NUMBER){
                char buffer[256] = {0};
                avStringPrintfToBuffer(buffer, sizeof(buffer)-1, AV_CSTR("%i"), value.asNumber);
                avDynamicArrayAddRange(buffer, avCStringLength(buffer), 0, 1, chars);
                i = j-1;
                continue;
            }
            if(value.type == VALUE_TYPE_ARRAY){
                uint32 count = value.asArray.count;
                struct ConstValue* values = value.asArray.values;
                uint32 left = i;
                uint32 right = j;
                for(; left != -1; left--){
                    char c = arg.chrs[left];
                    if(c==' '){
                        break;
                    }
                }
                for(; right < arg.len; right++){
                    char c = arg.chrs[right];
                    if(c==' '){
                        break;
                    }
                }
                
                AvString leftMost = {
                    .chrs = arg.chrs + (left+1),
                    .len = i - (left+1),
                };
                AvString rightMost = {
                    .chrs = arg.chrs + j,
                    .len = right - j,
                };
                avDynamicArraySetAllowRelocation(true, chars);
                for(uint32 k = 0; k < leftMost.len; k++){
                    avDynamicArrayRemove(AV_DYNAMIC_ARRAY_LAST, chars);
                }
                avDynamicArraySetAllowRelocation(false, chars);

                for(uint32 k = 0; k < count; k++){
                    struct ConstValue val = values[k];
                    avDynamicArrayAddRange((char*)leftMost.chrs, leftMost.len, 0, 1, chars);
                    bool32 isEmpty = true;
                    if(val.type == VALUE_TYPE_STRING){
                        if(processArg(val.asString, chars, project)){
                            isEmpty = false;
                        }
                    }
                    if(val.type == VALUE_TYPE_NUMBER){
                        char buffer[256] = {0};
                        avStringPrintfToBuffer(buffer, sizeof(buffer)-1, AV_CSTR("%i"), val.asNumber);
                        avDynamicArrayAddRange(buffer, avCStringLength(buffer), 0, 1, chars);
                        isEmpty = false;
                    }

                    avDynamicArrayAddRange((char*)rightMost.chrs, rightMost.len, 0, 1, chars);

                    if(k < count -1 && !isEmpty){
                        char tmpChar = ' ';
                        avDynamicArrayAdd(&tmpChar, chars);
                    }
                }   

                i = j-1;
            }
            continue;
        }
        ignoreNext = false;
        if(c=='\\'){
            ignoreNext = true;
            continue;
        }

        avDynamicArrayAdd(&c, chars);
    }
    
    return avDynamicArrayGetSize(chars)-start;
}

void parseCommandString(AvString str, struct CommandDescription** dst, Project* project){
    AvDynamicArray finalArg = AV_EMPTY;
    avDynamicArrayCreate(0, sizeof(char), &finalArg);

    processArg(str, finalArg, project);

    uint32 count = avDynamicArrayGetSize(finalArg);
    char* buffer = avCallocate(count+1, 1, "");
    avDynamicArrayReadRange(buffer, count, 0, 1, 0, finalArg);

    AvDynamicArray args = AV_EMPTY;
    avDynamicArrayCreate(0, sizeof(struct AvString), &args);

    uint32 start = -1;
    for(uint32 i = 0; i < count; i++){
        char c = buffer[i];
        if(start == -1){
            if(avCharIsWhiteSpace(c)){
                continue;
            }
            start = i;
            continue;
        }
        if(avCharIsWhiteSpace(c)){
            AvString arg = {
                .chrs = buffer + start,
                .len = i - start,
                .memory = nullptr,
            };
            avDynamicArrayAdd(&arg, args);
            start = -1;
        }
    }
    if(start != -1){
        AvString arg ={
            .chrs = buffer + start,
            .len = count - start,
            .memory = nullptr,
        };
        avDynamicArrayAdd(&arg, args);
    }

    *dst = avAllocate(sizeof(struct CommandDescription), "commandDescription");
    (*dst)->args = args;
    (*dst)->command = buffer;

    avDynamicArrayDestroy(finalArg);
}

void assignVariableIndexed(struct AvString identifier, uint32 index, struct Value value, Project* project);

void runIfCommandStatement(struct IfCommandStatement_S statement, Project* project){
    struct Value value = getValue(statement.check, project);
    bool32 pass = false;
    switch(value.type){
        case VALUE_TYPE_NONE:
            runtimeError(project, "encountered null value in if statement");
            return;
        case VALUE_TYPE_NUMBER:
            pass = value.asNumber != 0;
            break;
        case VALUE_TYPE_STRING:
            pass = value.asString.len != 0;
            break;
        case VALUE_TYPE_ARRAY:
            pass = value.asArray.count != 0;
            break;
    }
    if(pass){
        for(uint32 i = 0; i < statement.branch->statementCount; i++){
            struct CommandStatement_S stat = statement.branch->statements[i];
            switch(stat.type){
                case COMMAND_STATEMENT_FUNCTION_CALL:
                    callFunction(stat.functionCall, project);
                    break;
                case COMMAND_STATEMENT_VARIABLE_ASSIGNMENT:
                    runVariableAssignment(stat.variableAssignment, -1, project);
                    break;
                case COMMAND_STATEMENT_IF_STATEMENT:
                    runIfCommandStatement(stat.ifStatement, project);
                    break;
                case COMMAND_STATEMENT_NONE:
                    avAssert(false, "logic error");
                    break;
            }
        }
    }else{
        if(statement.alternativeBranch->check==nullptr){
            for(uint32 i = 0; i < statement.alternativeBranch->branch->statementCount; i++){
                struct CommandStatement_S stat = statement.alternativeBranch->branch->statements[i];
                switch(stat.type){
                    case COMMAND_STATEMENT_FUNCTION_CALL:
                        callFunction(stat.functionCall, project);
                        break;
                    case COMMAND_STATEMENT_VARIABLE_ASSIGNMENT:
                        runVariableAssignment(stat.variableAssignment, -1, project);
                        break;
                    case COMMAND_STATEMENT_IF_STATEMENT:
                        runIfCommandStatement(stat.ifStatement, project);
                        break;
                    case COMMAND_STATEMENT_NONE:
                        avAssert(false, "logic error");
                        break;
                }
            }
        }else{
            runIfCommandStatement(*statement.alternativeBranch, project);
        }
    }
}

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
            case COMMAND_STATEMENT_IF_STATEMENT:
                runIfCommandStatement(statement.ifStatement, project);
                break;
            case COMMAND_STATEMENT_NONE:
                avAssert(false, "logic error");
                break;
        }
    }

    struct VariableDescription commandVar = findVariable(AV_CSTR("command"), project);
    if(commandVar.value==nullptr || commandVar.value->type != VALUE_TYPE_STRING){
        runtimeError(project, "command value is not string");
        endLocalContext(project);
        return;
    }
    
    AvString commandUnformated = commandVar.value->asString;
    struct CommandDescription* commandDescription = nullptr;
    parseCommandString(commandUnformated, &commandDescription, project);

    endLocalContext(project);


    uint32 argCount = avDynamicArrayGetSize(commandDescription->args);
    avDynamicArrayMakeContiguous(commandDescription->args);
    AvString* strings = avDynamicArrayGetPageDataPtr(0, commandDescription->args);

    AvProcessStartInfo info = AV_EMPTY;
	avProcessStartInfoPopulateARR(&info, strings[0], (AvString)AV_EMPTY, argCount, strings);

    AvPipe pipe = AV_EMPTY;
    if(command.outputVariable.len){
        avPipeCreate(&pipe);
        info.output = &pipe.write;
    }

    AvProcess process = AV_EMPTY;
	avProcessStart(info, &process);
    avPipeConsumeWriteChannel(&pipe);
	int32 retCode = avProcessWaitExit(process);
    avProcessDiscard(process);

    avProcessStartInfoDestroy(&info);

    if(command.outputVariable.len){
        char buffer[4096] = {0};
        AvDynamicArray data = AV_EMPTY;
        avDynamicArrayCreate(0, 1, &data);
        while(true){
            int readBytes = read(pipe.read, buffer,  sizeof(buffer));
            if(readBytes == -1){
                runtimeError(project, "error reading from pipe");
                return;
            }
            if(readBytes == 0){
                break;
            }
            avDynamicArrayAddRange(buffer, readBytes, 0, 1, data);
        }
        avDynamicArrayMakeContiguous(data);
        uint32 dataSize = avDynamicArrayGetSize(data);
        char* strData = avAllocatorAllocate(dataSize+1, &project->allocator);
        avDynamicArrayReadRange(strData, dataSize, 0, 1, 0, data);
        avDynamicArrayDestroy(data);

        if(command.outputVariableIndex){
            struct Value indexValue = getValue(command.outputVariableIndex, project);
            if(indexValue.type != VALUE_TYPE_NUMBER){
                runtimeError(project, "cannot index with non number");
                return;
            }
            uint32 index = indexValue.asNumber;
            struct Value value = {
                .type = VALUE_TYPE_STRING,
                .asString = {
                    .chrs = strData,
                    .len = dataSize,
                    .memory = nullptr,
                },
            };
            assignVariableIndexed(command.outputVariable, index, value, project);
        }else{
            struct VariableDescription var = findVariable(command.outputVariable, project);
            AvDynamicArray strs = AV_EMPTY;
            avDynamicArrayCreate(0, sizeof(AvString), &strs);
            uint32 start =  0;
            uint32 end = 0;
            for(; end < dataSize; end++){
                char c = strData[end];
                if(c=='\n'){
                    AvString str = {
                        .chrs = strData+start,
                        .len = end - start,
                        .memory = nullptr,
                    };
                    start = end+1;
                    if(str.len != 0){
                        avDynamicArrayAdd(&str, strs);
                    }
                }
            }
            if(start != dataSize){
                AvString str = {
                    .chrs = strData+start,
                    .len = end - start,
                    .memory = nullptr,
                };
                start = end+1;
                avDynamicArrayAdd(&str, strs);
            }

            struct ConstValue* values = nullptr;
            if(avDynamicArrayGetSize(strs)){
                values = avAllocatorAllocate(sizeof(struct ConstValue) * avDynamicArrayGetSize(strs), &project->allocator);
            }
            for(uint32 i = 0; i < avDynamicArrayGetSize(strs); i++){
                values[i].type = VALUE_TYPE_STRING;
            }
            avDynamicArrayReadRange(values, avDynamicArrayGetSize(strs), offsetof(struct ConstValue, asString), sizeof(struct ConstValue), 0, strs);
            if(var.project){
                assignVariable(var, (struct Value){
                    .type = VALUE_TYPE_ARRAY,
                    .asArray = {
                        .count = avDynamicArrayGetSize(strs),
                        .values = values,
                    },
                } ,project);
            }else{
                struct Value* value = avAllocatorAllocate(sizeof(struct Value), &project->allocator);
                value->type = VALUE_TYPE_ARRAY,
                value->asArray = (struct ArrayValue){
                    .count = avDynamicArrayGetSize(strs),
                    .values = values,
                };
                addVariableToContext((struct VariableDescription){
                    .identifier = command.outputVariable,
                    .project = project,
                    .statement = -1,
                    .value = value,
                }, project);
            }
            avDynamicArrayDestroy(strs);
        }

        avPipeDestroy(&pipe);
    }
    
    if(project->options.commandDebug){
        avStringPrintf(AV_CSTR("%i = %s\n"), retCode, AV_CSTR(commandDescription->command));
    }

    avFree(commandDescription->command);
    avDynamicArrayDestroy(commandDescription->args);
    avFree(commandDescription);

    if(command.retCodeVariable.len){
        struct VariableDescription var = findVariable(command.retCodeVariable, project);
        if(!var.project){
            if(command.retCodeIndex){
                runtimeError(project, "unable to index unknown variable %s", command.retCodeVariable);
                return;
            }

            struct Value* retValue = avAllocatorAllocate(sizeof(struct Value), &project->allocator);
            retValue->type= VALUE_TYPE_NUMBER;
            retValue->asNumber = retCode;
            addVariableToContext((struct VariableDescription){
                .identifier= command.retCodeVariable, 
                .project= project, 
                .statement=-1,
                .value = retValue,
            }, project);
        }else{
            if(command.retCodeIndex){
                struct Value index = getValue(command.retCodeIndex, project);
                if(index.type != VALUE_TYPE_NUMBER){
                    runtimeError(project, "can only index with number");
                }
                assignVariableIndexed(command.retCodeVariable,index.asNumber, (struct Value){.type=VALUE_TYPE_NUMBER, .asNumber=retCode}, project);
            }else{
                assignVariable((struct VariableDescription){
                    .identifier = command.retCodeVariable,
                    .project = project,
                    .statement = -1,
                }, (struct Value){.type=VALUE_TYPE_NUMBER, .asNumber=retCode}, project);
            }
        }

    }

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

    struct Value* value = avAllocatorAllocate(sizeof(struct Value), &project->allocator);
    value->type = VALUE_TYPE_NUMBER,
    value->asNumber = 0;
    if(size.asNumber==1){
        addVariableToContext((struct VariableDescription){
            .identifier = variable.identifier,
            .project = project,
            .statement = -1,
            .value = value,
        }, project);
        return;
    }
    
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

void runIfPerformStatement(struct IfPerformStatement_S statement, Project* project){
    struct Value value = getValue(statement.check, project);
    bool32 pass = false;
    switch(value.type){
        case VALUE_TYPE_NONE:
            runtimeError(project, "encountered null value in if statement");
            return;
        case VALUE_TYPE_NUMBER:
            pass = value.asNumber != 0;
            break;
        case VALUE_TYPE_STRING:
            pass = value.asString.len != 0;
            break;
        case VALUE_TYPE_ARRAY:
            pass = value.asArray.count != 0;
            break;
    }
    if(pass){
        for(uint32 i = 0; i < statement.branch->statementCount; i++){
            struct PerformStatement_S stat = statement.branch->statements[i];
            switch(stat.type){
                case PERFORM_OPERATION_TYPE_FUNCTION_CALL:
                    callFunction(stat.functionCall, project);
                    break;
                case PERFORM_OPERATION_TYPE_VARIABLE_ASSIGNMENT:
                    runVariableAssignment(stat.variableAssignment, -1, project);
                    break;
                case PERFORM_OPERATION_TYPE_COMMAND:
                    performCommand(stat.commandStatement, project);
                    break;
                case PERFORM_OPERATION_TYPE_VARIABLE_DEFINITION:
                    performVariableDefinition(stat.variableDefinition, project);
                    break;
                case PERFORM_OPERATION_TYPE_IF_STATEMENT:
                    runIfPerformStatement(stat.ifStatement, project);
                    break;
                case PERFORM_OPERATION_TYPE_NONE:
                    avAssert(false, "logic error");
                    break;
            }
        }
    }else{
        if(!statement.alternativeBranch){
            return;
        }
        if(statement.alternativeBranch->check==nullptr){
            for(uint32 i = 0; i < statement.alternativeBranch->branch->statementCount; i++){
                struct PerformStatement_S stat = statement.alternativeBranch->branch->statements[i];
                switch(stat.type){
                    case PERFORM_OPERATION_TYPE_FUNCTION_CALL:
                        callFunction(stat.functionCall, project);
                        break;
                    case PERFORM_OPERATION_TYPE_VARIABLE_ASSIGNMENT:
                        runVariableAssignment(stat.variableAssignment, -1, project);
                        break;
                    case PERFORM_OPERATION_TYPE_COMMAND:
                        performCommand(stat.commandStatement, project);
                        break;
                    case PERFORM_OPERATION_TYPE_VARIABLE_DEFINITION:
                        performVariableDefinition(stat.variableDefinition, project);
                        break;
                    case PERFORM_OPERATION_TYPE_IF_STATEMENT:
                        runIfPerformStatement(stat.ifStatement, project);
                        break;
                    case PERFORM_OPERATION_TYPE_NONE:
                        avAssert(false, "logic error");
                        break;
                }
            }
        }else{
            runIfPerformStatement(*statement.alternativeBranch, project);
        }
    }
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
            case PERFORM_OPERATION_TYPE_IF_STATEMENT:
                runIfPerformStatement(statement.ifStatement, project);
                break;
            case PERFORM_OPERATION_TYPE_NONE:
                avAssert(false, "logic error");
                break;
            
        }
    }
    endLocalContext(project);
}

struct Value runIfFunctionStatement(struct IfFunctionStatement_S statement, bool32* returned, Project* project){
    struct Value check = getValue(statement.check, project);
    bool32 pass = false;
    switch(check.type){
        case VALUE_TYPE_NONE:
            runtimeError(project, "encountered null value in if statement");
            return NULL_VALUE;
        case VALUE_TYPE_NUMBER:
            pass = check.asNumber != 0;
            break;
        case VALUE_TYPE_STRING:
            pass = check.asString.len != 0;
            break;
        case VALUE_TYPE_ARRAY:
            pass = check.asArray.count != 0;
            break;
    }
    struct Value value = NULL_VALUE;
    if(pass){
        for(uint32 i = 0; i < statement.branch->statementCount; i++){
            struct FunctionStatement_S stat = statement.branch->statements[i];
            switch(stat.type){
                case FUNCTION_STATEMENT_TYPE_FOREACH:
                    performForeach(stat.foreachStatement, project);
                    break;
                case FUNCTION_STATEMENT_TYPE_PERFORM:
                    performPerform(stat.performStatement, project);
                    break;
                case FUNCTION_STATEMENT_TYPE_RETURN:{
                    struct Value returnValue = getValue(stat.returnStatement.value, project);
                    memcpy(&value, &returnValue, sizeof(struct Value));
                    *returned = true;
                    break;
                }
                case FUNCTION_STATEMENT_TYPE_IF:
                    bool32 rett = false;
                    struct Value retValue = runIfFunctionStatement(stat.ifStatement, &rett, project);
                    if(rett){
                        memcpy(&value, &retValue, sizeof(struct Value));
                        *returned = true;
                    }
                    break;
                case FUNCTION_STATEMENT_TYPE_VAR_DEFINITION:
                    performVariableDefinition(stat.variableDefinition, project);
                    break;
                case FUNCTION_STATEMENT_TYPE_NONE:
                    avAssert(false, "invalid statement");
                    break;
            }
            if(*returned){
                break;
            } 
        }
    }else{
        if(!statement.alternativeBranch){
            return NULL_VALUE;
        }
        if(statement.alternativeBranch->check==nullptr){
            for(uint32 i = 0; i < statement.alternativeBranch->branch->statementCount; i++){
                struct FunctionStatement_S stat = statement.alternativeBranch->branch->statements[i];
                switch(stat.type){
                    case FUNCTION_STATEMENT_TYPE_FOREACH:
                        performForeach(stat.foreachStatement, project);
                        break;
                    case FUNCTION_STATEMENT_TYPE_PERFORM:
                        performPerform(stat.performStatement, project);
                        break;
                    case FUNCTION_STATEMENT_TYPE_RETURN:{
                        struct Value returnValue = getValue(stat.returnStatement.value, project);
                        memcpy(&value, &returnValue, sizeof(struct Value));
                        *returned = true;
                        break;
                    }
                    case FUNCTION_STATEMENT_TYPE_IF:
                        bool32 rett = false;
                        struct Value retValue = runIfFunctionStatement(stat.ifStatement, &rett, project);
                        if(rett){
                            memcpy(&value, &retValue, sizeof(struct Value));
                            *returned = true;
                        }
                        break;
                    case FUNCTION_STATEMENT_TYPE_VAR_DEFINITION:
                        performVariableDefinition(stat.variableDefinition, project);
                        break;

                    case FUNCTION_STATEMENT_TYPE_NONE:
                        avAssert(false, "invalid statement");
                        break;
                }
                if(*returned){
                    break;
                } 
            }
        }else{
            return runIfFunctionStatement(*statement.alternativeBranch, returned, project);
        }
    }
    return value;
}

struct Value runFunction(struct FunctionDefinition_S function, Project* project){
    struct Value value = {.type=VALUE_TYPE_NUMBER, .asNumber=0 };
    bool32 done = false;
    for(uint32 i = 0; i < function.body.statementCount; i++){
        struct FunctionStatement_S statement = function.body.statements[i];
        switch(statement.type){
            case FUNCTION_STATEMENT_TYPE_FOREACH:
                performForeach(statement.foreachStatement, project);
                break;
            case FUNCTION_STATEMENT_TYPE_PERFORM:
                performPerform(statement.performStatement, project);
                break;
            case FUNCTION_STATEMENT_TYPE_RETURN:{
                struct Value returnValue = getValue(statement.returnStatement.value, project);
                memcpy(&value, &returnValue, sizeof(struct Value));
                done = true;
                break;
            }
            case FUNCTION_STATEMENT_TYPE_IF:
                bool32 returned = false;
                struct Value retValue = runIfFunctionStatement(statement.ifStatement, &returned, project);
                if(returned){
                    memcpy(&value, &retValue, sizeof(struct Value));
                    done = true;
                }
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
        runtimeError( project,"invalid number of arguments calling function %s", call.function);
    }

    
    startLocalContext(description.project, false);
    for(uint32 i = 0; i < call.argumentCount; i++){
        struct Value value = values[i];
        struct VariableDescription variable = {
            .identifier = function.parameters[i],
            .project = description.project,
            .statement = description.statement,
        };
        assignVariable(variable, value, description.project);
    }
    avFree(values);
    struct Value returnValue = runFunction(function, description.project);
    endLocalContext(description.project);

    return returnValue;
}

#define REPLACE(seq, chr) avStringFree(newString); if(avStringReplace(newString, *prevString, AV_CSTR(seq), AV_CSTR(chr))){ SWAP(*(uint64*)&newString, *(uint64*)&prevString); finalString = newString; } else { finalString = prevString;}
#define SWAP(a, b) a^=b; b^=a; a^=b;
void sanitizeString(AvStringRef str, Project* project){
    avStringDebugContextStart;
    AvString newStr = AV_EMPTY;
    AvString seqs[] = {
        AV_CSTRA("\\n"),AV_CSTR("\n"),
        AV_CSTRA("\\\""),AV_CSTRA("\""),
        AV_CSTRA("\\\'"),AV_CSTRA("\'"),
        AV_CSTRA("\\r") ,AV_CSTRA("\r"),
        AV_CSTRA("\\t") ,AV_CSTRA("\t"),
        AV_CSTRA("\\\\"),AV_CSTRA("\\"),
        AV_CSTRA("\\b") ,AV_CSTRA("\b"),
    };
    avStringReplaceAll(&newStr, *str, sizeof(seqs)/sizeof(AvString)/2, sizeof(AvString)*2, seqs, seqs+1);
    memset(str, 0 ,sizeof(AvString));
    if(newStr.len){
        avStringCopyToAllocator(newStr, str, &project->allocator);
    }
    avStringFree(&newStr);
    avStringDebugContextEnd;
}

struct Value getValue(struct Expression_S* expression, Project* project){
    switch(expression->type){
        case EXPRESSION_TYPE_NONE:
            return (struct Value){
                .type = VALUE_TYPE_ARRAY,
                .asArray = {
                    .count=0,
                    .values=nullptr,
                },
            };
        case EXPRESSION_TYPE_LITERAL:{
            AvString str = {
                .chrs = expression->literal.value.chrs + 1,
                .len = expression->literal.value.len - 2,
                .memory = nullptr,
            };
            sanitizeString(&str, project);
            return (struct Value){
                .type = VALUE_TYPE_STRING,
                .asString = str,
            };
        }
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
        case EXPRESSION_TYPE_COMPARISON:
            return performComparison(expression->comparison, project);
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

void assignVariableInGlobalScope(struct VariableDescription description, struct Value value, Project* project){
    avDynamicArrayForEachElement(struct VariableDescription, project->variables, {
        if(avStringEquals(description.identifier, element.identifier)){
            avDynamicArrayWrite(&description, index, project->variables);
            return;
        }
    });
    avDynamicArrayForEachElement(struct VariableDescription, project->constants, {
        if(avStringEquals(description.identifier, element.identifier)){
            runtimeError(project, "Cant write to constant");
            return;
        }
    });
    addVariableToContext(description, project);
}

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
    assignVariableInGlobalScope(description, value, project);
}

void assignConstant(struct VariableDescription description, struct Value value, Project* project){
    struct Value* val = avAllocatorAllocate(sizeof(struct Value), &project->allocator);
    memcpy(val, &value, sizeof(struct Value));
    description.value = val;
    avDynamicArrayForEachElement(struct VariableDescription, project->variables, {
        if(avStringEquals(description.identifier, element.identifier)){
            runtimeError(project, "constant already exists!");
        }
    });
    avDynamicArrayAdd(&description, project->constants);
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
    if(varIndex == -1){
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

void addVariableToGlobalContext(struct VariableDescription description, Project* project){
    avDynamicArrayAdd(&description, project->variables);
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
    
    for(uint32 i = 0; i < builtInVariableCount; i++){
        struct BuiltInVariableDescription var = builtInVariables[i];
        assignConstant((struct VariableDescription){
            .identifier = var.identifier,
            .project = project,
            .statement = -1,
        }, var.value, project);
    }

    assignConstant((struct VariableDescription){
            .identifier = AV_CSTR("PROJECT_NAME"),
            .project = project,
            .statement = -1,
    }, (struct Value){.type=VALUE_TYPE_STRING,.asString=project->name}, project);

    for(uint32 i = 0; i < project->statementCount; i++){
        struct Statement_S* statement = (project->statements)[i];
        switch(statement->type){
            case STATEMENT_TYPE_IMPORT:
            case STATEMENT_TYPE_FUNCTION_DEFINITION:
                break;
            case STATEMENT_TYPE_INHERIT:
                if(!statement->inheritStatement.defaultValue){
                    struct Value* value = avAllocatorAllocate(sizeof(struct Value), &project->allocator);
                    struct Value tmpValue = getValue(statement->inheritStatement.defaultValue, project);
                    memcpy(value, &tmpValue, sizeof(AvString));
                    addVariableToContext((struct VariableDescription){
                        .identifier = statement->inheritStatement.variable,
                        .project = project,
                        .statement = i,
                        .value = value,
                    }, project);
                }
                break;
            case STATEMENT_TYPE_VARIABLE_ASSIGNMENT:
                runVariableAssignment(statement->variableAssignment, i, project);
                break;
            case STATEMENT_TYPE_NONE:
                return -1;
                break;
        }

    }
    AvString* entry = &project->name;
    if(project->options.entry.len > 0 && project->options.entry.chrs){
        entry = &project->options.entry;
    }

    struct FunctionDescription mainFunction = findFunction(*entry, project);
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