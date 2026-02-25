#include "avBuilderBuiltIn.h"
#include <string.h>
#include <AvUtils/avMemory.h>
#include <AvUtils/filesystem/avDirectory.h>
#include <AvUtils/string/avChar.h>
#include <AvUtils/avMath.h>

// #ifndef _WIN32
// #include <unistd.h>
// #endif

#define TS(str) sizeof(#str)


#define BUILT_IN_FUNC(func, ...) {\
    .identifier=AV_CSTRA(#func),\
    .argumentCount = sizeof((BuiltInParameter[])__VA_ARGS__)/sizeof(BuiltInParameter),\
    .argTypes = (BuiltInParameter[]) __VA_ARGS__,\
    .function = func\
},
const struct BuiltInFunctionDescription builtInFunctions[] = {
    BUILT_IN_FUNCS
};
const uint32 builtInFunctionCount = sizeof(builtInFunctions)/sizeof(struct BuiltInFunctionDescription);
#undef BUILT_IN_FUNC


#define BUILT_IN_FUNC(func, ...) BUILT_IN_FUNC_ID_##func,
enum BuiltInFunctionId {
    BUILT_IN_FUNCS
};
#undef BUILT_IN_FUNC

#define SET_VALUE_TYPE_NUMBER(number) .asNumber=number
#define SET_VALUE_TYPE_STRING(string) .asString=string
#define SET_VALUE_TYPE_ARRAY(array) .asArray=array

#define SET_VALUE(type, val) SET_##type(val)

#define BUILT_IN_VAR(var, valType, val) {\
    .identifier=AV_CSTRA(#var),\
    .value={ \
        .type=valType,\
        SET_VALUE(valType, val),\
    },\
},
const struct BuiltInVariableDescription builtInVariables[] = {
    BUILT_IN_VARS 
};
const uint32 builtInVariableCount = sizeof(builtInVariables)/sizeof(struct BuiltInVariableDescription);
#undef BUILT_IN_VAR
#undef SET_VALUE_TYPE_STRING
#undef SET_VALUE_TYPE_ARRAY
#undef SET_VALUE_TYPE_NUMBER

#define SET_VALUE_TYPE_NUMBER(number) number

#define BUILT_IN_VAR(var, valType, val) var=SET_VALUE(valType, val),
enum BuiltInConstants {
    BUILT_IN_VARS
};
#undef BUILT_IN_VAR

bool32 isBuiltInFunction(struct BuiltInFunctionDescription* description, AvString identifier, Project* project){
    for(uint32 i = 0; i < builtInFunctionCount; i++){
        if(avStringEquals(builtInFunctions[i].identifier, identifier)){
            memcpy(description, builtInFunctions+i, sizeof(struct BuiltInFunctionDescription));
            return true;
        }
    }
    return false;
}

void getUsage(AvStringRef str, AvString funcName, uint32 argCount, BuiltInParameter* args){
    
    AvStringMemory memory =AV_EMPTY;

    uint32 len = 0;
    for(uint32 i = 0; i < argCount; i++){
        enum ValueType type = args[i].type;
        const char* argName = args[i].name;
        len += avCStringLength(argName);

        if(type == VALUE_TYPE_NONE){
            len += TS(any)+3;
            continue;
        }
        if(type & VALUE_TYPE_ARRAY){
            len += TS(array)+1;
        }
        if(type & VALUE_TYPE_NUMBER){
            len += TS(number)+1;
        }
        if(type & VALUE_TYPE_STRING){
            len += TS(number)+1;
        }
        len+=4;
    }


    avStringMemoryAllocate(funcName.len + 2 + len, &memory);
    
    avStringMemoryStore(funcName, 0, AV_STRING_FULL_LENGTH, &memory);
    avStringMemoryStore(AV_CSTRA("("), funcName.len, AV_STRING_FULL_LENGTH, &memory);
    uint32 index = funcName.len + 1;

    for(uint32 i = 0; i < argCount; i++){
        enum ValueType type = args[i].type;
        AvString argName = AV_CSTR(args[i].name);
        avStringMemoryStore(argName, index, AV_STRING_FULL_LENGTH, &memory);
        index+=argName.len;


        {
            AvString str = AV_CSTRA("<");
            avStringMemoryStore(str, index, AV_STRING_FULL_LENGTH, &memory);
            index+=str.len;
        }
        if(type == (VALUE_TYPE_ARRAY|VALUE_TYPE_STRING|VALUE_TYPE_NUMBER)){
            AvString str = AV_CSTRA("any");
            avStringMemoryStore(str, index, AV_STRING_FULL_LENGTH, &memory);
            index+=str.len;
            avStringMemoryStore(AV_CSTRA(", "), index, AV_STRING_FULL_LENGTH, &memory);
            index+=2;
            continue;
        }
        if(type & VALUE_TYPE_ARRAY){
            AvString str = AV_CSTRA("array");
            avStringMemoryStore(str, index, AV_STRING_FULL_LENGTH, &memory);
            index+=str.len;
            type &= ~VALUE_TYPE_ARRAY;
            if(type != 0){
                avStringMemoryStore(AV_CSTRA("|"), index, AV_STRING_FULL_LENGTH, &memory);
                index+=1;
            }
        }
        if(type & VALUE_TYPE_NUMBER){
            AvString str = AV_CSTRA("number");
            avStringMemoryStore(str, index, AV_STRING_FULL_LENGTH, &memory);
            index+=str.len;
            type &= ~VALUE_TYPE_NUMBER;
            if(type != 0){
                avStringMemoryStore(AV_CSTRA("|"), index, AV_STRING_FULL_LENGTH, &memory);
                index+=1;
            }
        }
        if(type & VALUE_TYPE_STRING){
            AvString str = AV_CSTRA("string");
            avStringMemoryStore(str, index, AV_STRING_FULL_LENGTH, &memory);
            index+=str.len;
            type &= ~VALUE_TYPE_STRING;
        }
        {
            AvString str = AV_CSTRA(">");
            avStringMemoryStore(str, index, AV_STRING_FULL_LENGTH, &memory);
            index+=str.len;
        }
        if(i == argCount - 1){
            avStringMemoryStore(AV_CSTRA(");"), index, AV_STRING_FULL_LENGTH, &memory);
        }else{
            avStringMemoryStore(AV_CSTRA(", "), index, AV_STRING_FULL_LENGTH, &memory);
        }
        index+=2;
    }

    avStringFromMemory(str, AV_STRING_WHOLE_MEMORY, &memory);

}

struct Value callBuiltInFunction(struct BuiltInFunctionDescription description, uint32 argumentCount, struct Value* values, Project* project){

    if(description.argumentCount > argumentCount){
        AvString usage = AV_EMPTY;
        getUsage(&usage, description.identifier, description.argumentCount, description.argTypes);
        runtimeError(project, 
            "Calling built-in function '%S' with an invalid amount of arguments\nUsage: %S", 
            description.identifier, usage);
        avStringFree(&usage);
    }

    for(uint32 i = 0; i < description.argumentCount; i++){
        if((description.argTypes[i].type & values[i].type) == 0){
            AvString usage = AV_EMPTY;
            getUsage(&usage, description.identifier, description.argumentCount, description.argTypes);
            runtimeError(project, 
                "Calling built-in function '%S' with an invalid argument\nUsage: %S", 
                description.identifier, usage);
            avStringFree(&usage);
        }
    }

    return description.function(project, argumentCount, values);

}

struct Value fileName(Project* project, uint32 valueCount, struct Value* values){
    AvString file = values[0].asString;
    AvArray filePaths = AV_EMPTY;
    avStringSplitOnChar(&filePaths, '/', file);
    AvString fileName = AV_EMPTY;
    avArrayRead(&fileName, filePaths.count-1, &filePaths);
    
    strOffset dot = avStringFindFirstOccranceOfChar(fileName, '.');
    uint64 len = (dot==AV_STRING_NULL ? fileName.len : dot);

    AvString str = {
        .chrs = fileName.chrs,
        .len = len,
        .memory = nullptr,
    };
    AvString tmpStr = AV_EMPTY;
    avStringCopyToAllocator(str, &tmpStr, &project->allocator);
    avArrayFree(&filePaths);

    return (struct Value){
        .type = VALUE_TYPE_STRING,
        .asString = tmpStr,
    };
}

struct Value fileFullName(Project* project, uint32 valueCount, struct Value* values){
    AvString file = values[0].asString;
    AvArray filePaths = AV_EMPTY;
    avStringSplitOnChar(&filePaths, '/', file);
    AvString fileName = AV_EMPTY;
    avArrayRead(&fileName, filePaths.count-1, &filePaths);

    AvString str = {
        .chrs = fileName.chrs,
        .len = fileName.len,
        .memory = nullptr,
    };
    AvString tmpStr = AV_EMPTY;
    avStringCopyToAllocator(str, &tmpStr, &project->allocator);
    avArrayFree(&filePaths);


    return (struct Value){
        .type = VALUE_TYPE_STRING,
        .asString = tmpStr,
    };
}

struct Value fileBaseName(Project* project, uint32 valueCount, struct Value* values){
    AvString file = values[0].asString;
    if(file.len == 0 || file.chrs==nullptr){
        runtimeError(project, "cannot get basename of null value");
    }
    AvArray filePaths = AV_EMPTY;
    avStringSplitOnChar(&filePaths, '/', file);
    AvString fileName = AV_EMPTY;
    avArrayRead(&fileName, filePaths.count-1, &filePaths);
    avArrayFree(&filePaths);

    strOffset dot = avStringFindFirstOccranceOfChar(fileName, '.');
    uint64 len = (dot==AV_STRING_NULL ? fileName.len : dot);

    uint32 extLen = fileName.len - len;

    AvString str = {
        .chrs = file.chrs,
        .len = file.len - extLen,
        .memory = nullptr,
    };
    AvString tmpStr = AV_EMPTY;
    avStringCopyToAllocator(str, &tmpStr, &project->allocator);
    avArrayFree(&filePaths);

    return (struct Value){
        .type = VALUE_TYPE_STRING,
        .asString = tmpStr,
    };
}

struct Value fileLastModified(Project* project, uint32 valueCount, struct Value* values){
    AvString fileName = values[0].asString;
    if(fileName.len == 0 || fileName.chrs==nullptr){
        runtimeError(project, "cannot get basename of null value");
    }
    AvFile file = avFileHandleCreate(fileName);
    if(!avFileExists(file)){
        avFileHandleDestroy(file);
        return (struct Value){
            .type = VALUE_TYPE_NUMBER,
            .asNumber = -1,
        };
    }
    AvDateTime time = avFileGetModifiedTime(file);
    avFileHandleDestroy(file);
    return (struct Value){
            .type = VALUE_TYPE_NUMBER,
            .asNumber = avTimeConvertToNumber(time),
    };
}

struct Value arraySize(Project* project, uint32 valueCount, struct Value* values){
    if(values[0].type!=VALUE_TYPE_ARRAY){
        return (struct Value) {
            .type = VALUE_TYPE_NUMBER,
            .asNumber = 1,
        };
    }
    return (struct Value){
        .type = VALUE_TYPE_NUMBER,
        .asNumber = values[0].asArray.count,
    };
}

struct Value filter(Project* project, uint32 valueCount, struct Value* values){
    uint32 filterType = values[0].asNumber;

    struct ConstValue tmpValue = {0};
    uint32 count = 1;
    struct ConstValue* vals = &tmpValue;
    if(values[2].type==VALUE_TYPE_ARRAY){
        vals = values[2].asArray.values;
        count = values[2].asArray.count;
    }else{
        toConstValue(values[2], &tmpValue, project);
    }

    AvDynamicArray newValues = {0};
    avDynamicArrayCreate(0, sizeof(struct ConstValue), &newValues);
    bool8* allowed = avCallocate(count, 1, "allowed");

    struct ConstValue tmpFilter = {0};
    uint32 filterCount = 1;
    struct ConstValue* filters = &tmpFilter;
    if(values[1].type == VALUE_TYPE_ARRAY){
        filters = values[1].asArray.values;
        filterCount = values[1].asArray.count;
    }else{
        toConstValue(values[1], &tmpFilter, project);
    }

    for(uint32 j = 0; j < filterCount; j++){
        if(filters[j].type!=VALUE_TYPE_STRING){
            runtimeError(project, "filter values can only contain strings");
            return (struct Value){0};
        }
        AvString filterStr = filters[j].asString;
        for(uint32 i = 0; i < count; i++){
            struct ConstValue value = vals[i];
            AvString str = value.asString;
            char buffer[256] = {0};
            if(value.type == VALUE_TYPE_NUMBER){
                avStringPrintfToBuffer(buffer, sizeof(buffer)-1, AV_CSTR("%i"), value.asNumber);
                AvString tmpStr = AV_CSTR(buffer);
                memcpy(&str, &tmpStr, sizeof(AvString));
            }
            switch (filterType)
            {
            case FILTER_TYPE_ENDS_WITH:
                if(avStringEndsWith(str,filterStr)){
                    allowed[i] = true;
                }
                break;
            default:
                runtimeError(project, "unsupported filter type");
                break;
            }
        }
    }
    for(uint32 i = 0; i < count; i++){
        if(allowed[i]){
            avDynamicArrayAdd(vals+i, newValues);
        }
    }
    avFree(allowed);

    struct ConstValue* filteredValues = nullptr;
    uint32 allowedCount = avDynamicArrayGetSize(newValues);
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

    return filtered;
}

struct Value filePath(Project* project, uint32 valueCount, struct Value* values){
    AvString file = values[0].asString;
    if(file.len == 0 || file.chrs==nullptr){
        runtimeError(project, "cannot get basename of null value");
    }
    AvArray filePaths = AV_EMPTY;
    avStringSplitOnChar(&filePaths, '/', file);

    if(filePaths.count==0){
        return (struct Value){
            .type = VALUE_TYPE_STRING,
            .asString = {
                .chrs="",
                .len = 0,
                .memory = nullptr,
            },
        };
    }

    AvString* paths = (AvString*)filePaths.data;
    if(paths[filePaths.count-1].len==0){
        avArrayFree(&filePaths);
        return values[0];
    }
    
    struct Value returnValue = (struct Value){
        .type = VALUE_TYPE_STRING,
        .asString = {
            .chrs=values[0].asString.chrs,
            .len = values[0].asString.len - paths[filePaths.count-1].len,
            .memory = nullptr,
        },
    };
    avArrayFree(&filePaths);
    return returnValue;
}

struct Value print(Project* project, uint32 valueCount, struct Value* values){
    struct Value value = values[0];
    switch(value.type){
        case VALUE_TYPE_STRING:
            avStringPrint(value.asString);
            break;
        case VALUE_TYPE_NUMBER:
            avStringPrintf(AV_CSTR("%i"), value.asNumber);
            break;
        case VALUE_TYPE_ARRAY:
            for(uint32 i = 0; i < value.asArray.count; i++){
                struct ConstValue val = value.asArray.values[i];
                switch(val.type){
                    case VALUE_TYPE_STRING:
                        avStringPrint(val.asString);
                        break;
                    case VALUE_TYPE_NUMBER:
                        avStringPrintf(AV_CSTR("%i"), val.asNumber);
                        break;
                    default:
                        runtimeError(project, "logic error");
                        break;
                }
                if(i < value.asArray.count-1){
                    avStringPrint(AV_CSTRA(" "));
                }
            }
            break;
        default:
            runtimeError(project, "logic error");
    }

    return values[0];
}

struct Value println(Project* project, uint32 valueCount, struct Value* values){
    struct Value value = values[0];
    switch(value.type){
        case VALUE_TYPE_STRING:
            avStringPrintln(value.asString);
            break;
        case VALUE_TYPE_NUMBER:
            avStringPrintf(AV_CSTR("%i\n"), value.asNumber);
            break;
        case VALUE_TYPE_ARRAY:
            for(uint32 i = 0; i < value.asArray.count; i++){
                struct ConstValue val = value.asArray.values[i];
                switch(val.type){
                    case VALUE_TYPE_STRING:
                        avStringPrint(val.asString);
                        break;
                    case VALUE_TYPE_NUMBER:
                        avStringPrintf(AV_CSTR("%i"), val.asNumber);
                        break;
                    default:
                        runtimeError(project, "logic error");
                        break;
                }
                avStringPrint(AV_CSTRA("\n"));
            }
            break;
        default:
            runtimeError(project, "logic error");
    }

    return values[0];
}

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <AvUtils/filesystem/avDirectory.h>

struct Value makeDir(Project* project, uint32 valueCount, struct Value* values){
    AvString dir = AV_EMPTY;
    avStringClone(&dir, values[0].asString);
    int ret = avMakeDirectory(dir);
    if(ret == -1){
        avStringFree(&dir);
        struct ConstValue* vals = avAllocatorAllocate(sizeof(struct ConstValue)*2, &project->allocator);
        vals[0].type = VALUE_TYPE_NUMBER;
        vals[0].asNumber = errno;
        memcpy(&vals[1].asString, &AV_CSTR(strerror(errno)), sizeof(AvString));
        vals[1].type = VALUE_TYPE_STRING;
        return (struct Value) {
            .type=VALUE_TYPE_ARRAY,
            .asArray = {
                .count = 2,
                .values = vals,
            },
        };
    }
    avStringFree(&dir);
    return values[0];
}
#ifndef _WIN32
#include <linux/limits.h>
#else
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#endif
#include <stdio.h>

struct Value makeDirs(Project* project, uint32 valueCount, struct Value* values){
    AvString dir = AV_EMPTY;
    avStringClone(&dir, values[0].asString);
    int ret = avMakeDirectoryRecursive(dir);
    if(ret == -1){
        avStringFree(&dir);
        struct ConstValue* vals = avAllocatorAllocate(sizeof(struct ConstValue)*2, &project->allocator);
        vals[0].type = VALUE_TYPE_NUMBER;
        vals[0].asNumber = errno;
        vals[1].type = VALUE_TYPE_STRING;
        AvString error = AV_CSTR(strerror(errno));
        memcpy(&(vals[1].asString), &error, sizeof(AvString));
        return (struct Value) {
            .type=VALUE_TYPE_ARRAY,
            .asArray = {
                .count = 2,
                .values = vals,
            },
        };
    }
    avStringFree(&dir);
    return values[0];
}

struct Value deleteDir(Project* project, uint32 valueCount, struct Value* values){
    struct Value result = {.type=VALUE_TYPE_NUMBER, .asNumber=0};
    
    struct ConstValue tmpValue = {0};
    uint32 count = 1;
    struct ConstValue* vals = &tmpValue;
    if(values[0].type == VALUE_TYPE_ARRAY){
        count = values[0].asArray.count;
        vals = values[0].asArray.values;
    }else{
        toConstValue(values[0], vals, project);
    }
    if(count == 0){
        return result;
    }

    for(uint32 i = 0; i < count; i++){
        if(vals[i].type != VALUE_TYPE_STRING){
            runtimeError(project, "Invalid variable type in argument %S", builtInFunctions[BUILT_IN_FUNC_ID_deleteDir].argTypes[i].name);
            return result;
        }

        result.asNumber += avDirectoryDelete(vals[i].asString, AV_DIRECTORY_DELETE_RECURSIVE);
    }

    return result;
}

struct Value deleteFile(Project* project, uint32 valueCount, struct Value* values){
    struct Value result = {.type=VALUE_TYPE_NUMBER, .asNumber=0};
    
    struct ConstValue tmpValue = {0};
    uint32 count = 1;
    struct ConstValue* vals = &tmpValue;
    if(values[0].type == VALUE_TYPE_ARRAY){
        count = values[0].asArray.count;
        vals = values[0].asArray.values;
    }else{
        toConstValue(values[0], vals, project);
    }
    if(count == 0){
        return result;
    }

    for(uint32 i = 0; i < count; i++){
        if(vals[i].type != VALUE_TYPE_STRING){
            runtimeError(project, "Invalid variable type in argument %S", builtInFunctions[BUILT_IN_FUNC_ID_deleteDir].argTypes[i].name);
            return result;
        }

        AvFile file = avFileHandleCreate(vals->asString);
        if(!avFileExists(file)){
            continue;
        }
        result.asNumber += avFileDelete(file);
        avFileHandleDestroy(file);
    }

    return result;
}

struct Value compileString(Project* project, uint32 valueCount, struct Value* values){
    struct Value result = {.type=VALUE_TYPE_ARRAY, .asArray={.count=0}};
    
    struct ConstValue tmpValue = {0};
    uint32 count = 1;
    struct ConstValue* vals = &tmpValue;
    if(values[0].type == VALUE_TYPE_ARRAY){
        count = values[0].asArray.count;
        vals = values[0].asArray.values;
    }else{
        toConstValue(values[0], vals, project);
    }
    if(count == 0){
        return result;
    }

    struct ConstValue* results = avAllocatorAllocate(sizeof(struct ConstValue)*count, &project->allocator);

    for(uint32 i = 0; i < count; i++){
        if(vals[i].type!=VALUE_TYPE_STRING){
            runtimeError(project, "invalid type");
            return result;
        }
        AvDynamicArray finalArg = AV_EMPTY;
        avDynamicArrayCreate(0, sizeof(char), &finalArg);

        processArg(vals[i].asString, finalArg, project);

        uint32 count = avDynamicArrayGetSize(finalArg);
        char* buffer = avAllocatorAllocate(count+1, &project->allocator);
        avDynamicArrayReadRange(buffer, count, 0, 1, 0, finalArg);
        avDynamicArrayDestroy(finalArg);

        struct ConstValue res = {
            .type = VALUE_TYPE_STRING,
            .asString = AV_CSTR(buffer),
        };
        memcpy(results+i, &res, sizeof(struct ConstValue));
    }

    if(count == 1){
        struct Value res = {0};
        toValue(results[0], &res);
        return res;
    }else{
        return (struct Value){
            .type=VALUE_TYPE_ARRAY,
            .asArray = {
                .count = count,
                .values = results,
            },
        };
    }
}


struct Value toUppercase(Project* project, uint32 valueCount, struct Value* values){
    struct Value result = {.type=VALUE_TYPE_ARRAY, .asArray={.count=0}};
    
    struct ConstValue tmpValue = {0};
    uint32 count = 1;
    struct ConstValue* vals = &tmpValue;
    if(values[0].type == VALUE_TYPE_ARRAY){
        count = values[0].asArray.count;
        vals = values[0].asArray.values;
    }else{
        toConstValue(values[0], vals, project);
    }
    if(count == 0){
        return result;
    }

    struct ConstValue* results = avAllocatorAllocate(sizeof(struct ConstValue)*count, &project->allocator);

    for(uint32 i = 0; i < count; i++){
        if(vals[i].type!=VALUE_TYPE_STRING){
            runtimeError(project, "invalid type");
            return result;
        }
        
        AvString str =vals[i].asString;
        avStringToUppercase(&str);
        AvString tmpStr = AV_EMPTY;
        avStringCopyToAllocator(str, &tmpStr, &project->allocator);
        avStringFree(&str);

        struct ConstValue res = {
            .type = VALUE_TYPE_STRING,
            .asString = tmpStr,
        };
        memcpy(results+i, &res, sizeof(struct ConstValue));
    }

    if(count == 1){
        struct Value res = {0};
        toValue(results[0], &res);
        return res;
    }else{
        return (struct Value){
            .type=VALUE_TYPE_ARRAY,
            .asArray = {
                .count = count,
                .values = results,
            },
        };
    }
}

struct Value toLowercase(Project* project, uint32 valueCount, struct Value* values){
    struct Value result = {.type=VALUE_TYPE_ARRAY, .asArray={.count=0}};
    
    struct ConstValue tmpValue = {0};
    uint32 count = 1;
    struct ConstValue* vals = &tmpValue;
    if(values[0].type == VALUE_TYPE_ARRAY){
        count = values[0].asArray.count;
        vals = values[0].asArray.values;
    }else{
        toConstValue(values[0], vals, project);
    }
    if(count == 0){
        return result;
    }

    struct ConstValue* results = avAllocatorAllocate(sizeof(struct ConstValue)*count, &project->allocator);

    for(uint32 i = 0; i < count; i++){
        if(vals[i].type!=VALUE_TYPE_STRING){
            runtimeError(project, "invalid type");
            return result;
        }
        
        AvString str =vals[i].asString;
        avStringToUppercase(&str);
        AvString tmpStr = AV_EMPTY;
        avStringCopyToAllocator(str, &tmpStr, &project->allocator);
        avStringFree(&str);
        
        struct ConstValue res = {
            .type = VALUE_TYPE_STRING,
            .asString = tmpStr,
        };
        memcpy(results+i, &res, sizeof(struct ConstValue));
    }

    if(count == 1){
        struct Value res = {0};
        toValue(results[0], &res);
        return res;
    }else{
        return (struct Value){
            .type=VALUE_TYPE_ARRAY,
            .asArray = {
                .count = count,
                .values = results,
            },
        };
    }
}

struct Value changeDir(Project* project, uint32 valueCount, struct Value* values){
    struct Value result = {.type=VALUE_TYPE_ARRAY, .asArray={.count=0}};
    
    struct ConstValue tmpValue = {0};
    uint32 count = 1;
    struct ConstValue* vals = &tmpValue;
    if(values[0].type == VALUE_TYPE_ARRAY){
        count = values[0].asArray.count;
        vals = values[0].asArray.values;
    }else{
        toConstValue(values[0], vals, project);
    }
    if(count == 0){
        return result;
    }

    struct ConstValue* results = avAllocatorAllocate(sizeof(struct ConstValue)*count, &project->allocator);

    for(uint32 i = 0; i < count; i++){
        if(vals[i].type!=VALUE_TYPE_STRING){
            runtimeError(project, "invalid type");
            return result;
        }
        
        AvString str =vals[i].asString;
        AvString tmpStr = AV_EMPTY;
        avStringClone(&tmpStr, str);
        
        struct ConstValue res = {
            .type = VALUE_TYPE_NUMBER,
            .asNumber = avChangeCurrentDir(tmpStr),
        };
        avStringFree(&tmpStr);
        memcpy(results+i, &res, sizeof(struct ConstValue));
    }

    if(count == 1){
        struct Value res = {0};
        toValue(results[0], &res);
        return res;
    }else{
        return (struct Value){
            .type=VALUE_TYPE_ARRAY,
            .asArray = {
                .count = count,
                .values = results,
            },
        };
    }
}

struct Value currentDir(Project* project, uint32 valueCount, struct Value* values){
    
    char cwd[PATH_MAX];
    if (avGetCurrentDir(sizeof(cwd), cwd) != 0) {
        struct Value res = {
            .type= VALUE_TYPE_STRING,
        };
        avStringCopyToAllocator(AV_CSTR(cwd), &res.asString, &project->allocator);
        return res;
    } else {
        runtimeError(project, "getcwd() error");
        return (struct Value) {
            .type = VALUE_TYPE_ARRAY,
        };
    }

}

struct Value call(Project* project, uint32 valueCount, struct Value* values){
	AvString functionIdentifier = values[0].asString;


    struct FunctionDescription description = findFunction(functionIdentifier, project);
    if(!description.project){
        runtimeError( project,"unable to find function '%S'", functionIdentifier);
        return (struct Value) {.type=VALUE_TYPE_NONE};
    }
    struct Statement_S* statement = (description.project->statements[description.statement]);
    if(statement->type != STATEMENT_TYPE_FUNCTION_DEFINITION){
        runtimeError( project,"importing function of wrong type");
        return (struct Value) {.type=VALUE_TYPE_NONE};
    }
    struct FunctionDefinition_S function = statement->functionDefinition;
    if(function.parameterCount != valueCount-1){
        runtimeError( project,"invalid number of arguments calling function %S", functionIdentifier);
    }
    
    startLocalContext(description.project, false);
    for(uint32 i = 1; i < valueCount; i++){
        struct Value value = values[i];
        struct VariableDescription variable = {
            .identifier = function.parameters[i-1].name,
            .project = description.project,
            .statement = description.statement,
        };
        assignVariable(variable, value, description.project);
    }
    struct Value returnValue = runFunction(function, description.project);
    endLocalContext(description.project);

    return returnValue;
} 

struct Value callExtern(Project* project, uint32 valueCount, struct Value* values){
    AvString projectFile = values[0].asString;
    AvString functionName = values[1].asString;

    struct FunctionDescription func = importFunction((struct ImportDescription) {
        .extIdentifier=functionName,
        .importFile = projectFile,
        .isLocalFile = true,
        .identifier = functionName,
    }, project);

    if(!func.project){
        runtimeError(project, "unable to import %S from %S", functionName, projectFile);
        return (struct Value) {.type=VALUE_TYPE_ARRAY};
    }

    if(func.statement >= func.project->statementCount){
        runtimeError(project, "malformed import %S", functionName);
        return (struct Value) {.type=VALUE_TYPE_ARRAY};
    }

    struct Statement_S* statement = func.project->statements[func.statement];
    if(statement->type!=STATEMENT_TYPE_FUNCTION_DEFINITION){
        runtimeError(project, "malformed import %S", functionName);
        return (struct Value) {.type=VALUE_TYPE_ARRAY};
    }

    struct FunctionDefinition_S function = statement->functionDefinition;
    
    if(valueCount - 2 > function.parameterCount){
        runtimeError(project, "invalid number of arguments");
        return (struct Value) {.type=VALUE_TYPE_ARRAY};
    }
    
    startLocalContext(func.project, false);
    for(uint32 i = 0; i < function.parameterCount; i++){
        struct Value value = values[i+2];
        struct VariableDescription variable = {
            .identifier = function.parameters[i].name,
            .project = func.project,
            .statement = func.statement,
        };
        assignVariable(variable, value, func.project);
    }
    
    struct Value returnValue = runFunction(function, func.project);
    endLocalContext(func.project);

    return returnValue;

}
struct Rule{
    AV_DS(AvDynamicArray, AvString) targets;
    AV_DS(AvDynamicArray, AvString) dependencies;
};
static void deallocateRule(void* ptr, uint64 size){
    avDynamicArrayForEachElement(AvString, ((struct Rule*)ptr)->dependencies, { avStringFree(&element);});
    avDynamicArrayDestroy(((struct Rule*)ptr)->dependencies);
    avDynamicArrayForEachElement(AvString, ((struct Rule*)ptr)->targets, { avStringFree(&element);});
    avDynamicArrayDestroy(((struct Rule*)ptr)->targets);
}

static void allocateRule(struct Rule* rule){
    avDynamicArrayCreate(0, sizeof(AvString), &rule->targets);
    avDynamicArrayCreate(0, sizeof(AvString), &rule->dependencies);
}

struct Value parseDependencies(Project* project, uint32 valueCount, struct Value* values){
    avStringDebugContextStart;
    struct Value result = {.type=VALUE_TYPE_ARRAY, .asArray={.count=0}};

    AvString fileName = values[0].asString;

    if(fileName.len == 0 || fileName.chrs==nullptr){
        runtimeError(project, "cannot parse dependencies of null value");
    }

    AvFile file = avFileHandleCreate(fileName);
    
    if(!avFileOpen(file, AV_FILE_OPEN_READ_DEFAULT)){
        avFileHandleDestroy(file);
        return (struct Value) {.type=VALUE_TYPE_ARRAY};
    }

    uint64 size = avFileGetSize(file);
    char* buffer = avAllocate(size + 1, "allocating buffer");
    AvString fileContentStr = (AvString) {.chrs = buffer, .len = size, .memory = NULL};
    avFileRead(buffer, size, file);

    avStringReplace(&fileContentStr, fileContentStr, AV_CSTRA("\r\n"), AV_CSTRA("\n"));
    AvString fileContent = AV_EMPTY;
    avStringReplace(&fileContent, fileContentStr, AV_CSTRA("\\\n"), AV_CSTRA(""));
    avStringFree(&fileContentStr);
    
    AV_DS(AvDynamicArray, struct Rule) rules = AV_EMPTY;
    avDynamicArrayCreate(0, sizeof(struct Rule), &rules);
    avDynamicArraySetDeallocateElementCallback(deallocateRule, rules);

    enum parserState {
        STATE_WHITESPACE_BEFORE_TARGET,
        STATE_TARGET,
        STATE_WHITESPACE_AFTER_TARGET,
        STATE_DEPENDENCY,
        STATE_WHITESPACE_AFTER_DEPEND,
        STATE_END_RULE,
    } state = STATE_WHITESPACE_BEFORE_TARGET;
    AvDynamicArray currentString = AV_EMPTY;
    avDynamicArrayCreate(32, 1, &currentString);

    struct Rule currentRule = { 0 };
    allocateRule(&currentRule);
    
    bool32 escaped = 0;
    for (uint64 readIndex = 0; readIndex < fileContent.len; readIndex++) {
        char c = fileContent.chrs[readIndex];
        if(escaped){
            escaped -= 1;
        }
        if(c=='\\' && !escaped){ // only new escapes
            escaped = 2;
            continue;
        }
        
        switch(state){
            case STATE_WHITESPACE_BEFORE_TARGET:
                if(!avCharIsWhiteSpace(c) && !avCharIsNewline(c)){
                    state = STATE_TARGET;
                    readIndex--;
                }
                break;
            case STATE_TARGET:
                if(avCharIsWhiteSpace(c) && !escaped){
                    state = STATE_WHITESPACE_AFTER_TARGET;
                    readIndex--;
                } else
                if(c == ':' && !escaped){
                    state = STATE_WHITESPACE_AFTER_DEPEND;
                }else{
                    avDynamicArrayAdd(&c, currentString);
                    break;
                }
                if(avCharIsNewline(c) && !escaped){
                    state = STATE_WHITESPACE_AFTER_TARGET;
                }
                if(avDynamicArrayGetSize(currentString)){
                    AvStringHeapMemory mem;
                    avStringMemoryHeapAllocate(avDynamicArrayGetSize(currentString), &mem);
                    avDynamicArrayReadRange(mem->data, AV_DYNAMIC_ARRAY_FULL_RANGE, currentString);
                    AvString target = {0};
                    avStringFromMemory(&target, AV_STRING_WHOLE_MEMORY, mem);
                    avDynamicArrayAdd(&target, currentRule.targets);
                    avDynamicArrayClear(0, currentString);
                }
                break;
            case STATE_WHITESPACE_AFTER_TARGET:
                if(avCharIsWhiteSpace(c)){
                    break;
                }
                if(c==':' && !escaped){
                    state = STATE_WHITESPACE_AFTER_DEPEND;
                    break;
                }
                state = STATE_TARGET;
                readIndex--;
                break;
            case STATE_WHITESPACE_AFTER_DEPEND:
                if(avCharIsWhiteSpace(c)){
                    break;
                }
                if(avCharIsNewline(c) && !escaped){
                    state = STATE_END_RULE;
                    break;
                }
                state = STATE_DEPENDENCY;
                readIndex--;
                break;
            case STATE_DEPENDENCY:
                if(avCharIsWhiteSpace(c) && !escaped){
                    state = STATE_WHITESPACE_AFTER_DEPEND;
                    readIndex--;
                } else
                if(avCharIsNewline(c) && !escaped){
                    state = STATE_END_RULE;
                }else{
                    avDynamicArrayAdd(&c, currentString);
                    break;
                }
                if(avDynamicArrayGetSize(currentString)){
                    AvStringHeapMemory mem;
                    avStringMemoryHeapAllocate(avDynamicArrayGetSize(currentString), &mem);
                    avDynamicArrayReadRange(mem->data, AV_DYNAMIC_ARRAY_FULL_RANGE, currentString);
                    AvString target = {0};
                    avStringFromMemory(&target, AV_STRING_WHOLE_MEMORY, mem);
                    avDynamicArrayAdd(&target, currentRule.dependencies);
                    avDynamicArrayClear(0, currentString);
                }
                break;
            case STATE_END_RULE:
                if(avDynamicArrayGetSize(currentRule.targets) == 0){
                    runtimeError(project, "invalid dependency file content" AV_STRING_PRINTF_CODE, fileName);
                    goto doneParse;
                }
                avDynamicArrayAdd(&currentRule, rules);
                allocateRule(&currentRule);

                state = STATE_WHITESPACE_BEFORE_TARGET;
                readIndex--;
                break;
        }
    }
    if(state==STATE_DEPENDENCY){ // fix any dependency at the end of a file
        if(avDynamicArrayGetSize(currentString)){
            AvStringHeapMemory mem;
            avStringMemoryHeapAllocate(avDynamicArrayGetSize(currentString), &mem);
            avDynamicArrayReadRange(mem->data, AV_DYNAMIC_ARRAY_FULL_RANGE, currentString);
            AvString target = {0};
            avStringFromMemory(&target, AV_STRING_WHOLE_MEMORY, mem);
            avDynamicArrayAdd(&target, currentRule.dependencies);
            avDynamicArrayClear(0, currentString);
        }
        state = STATE_WHITESPACE_AFTER_DEPEND;
    }
    if(state==STATE_WHITESPACE_AFTER_DEPEND){
        avDynamicArrayAdd(&currentRule, rules);
        allocateRule(&currentRule);
        state = STATE_WHITESPACE_BEFORE_TARGET;
    }
    if(state != STATE_WHITESPACE_BEFORE_TARGET){
        runtimeError(project, "invalid dependency file content" AV_STRING_PRINTF_CODE, fileName);
        goto doneParse;
    }

    //convert to list of dependencies

    struct ConstValue tmpValue = {0};
    uint32 count = 1;
    struct ConstValue* vals = &tmpValue;
    if(values[1].type == VALUE_TYPE_ARRAY){
        count = values[1].asArray.count;
        vals = values[1].asArray.values;
        for(uint32 i = 0; i < count; i++){
            if(vals[i].type!=VALUE_TYPE_STRING){
                runtimeError(project, "target can only be a string");
                goto doneParse;
            }
        }
    }else{
        toConstValue(values[1], vals, project);
    }
    if(count == 0){
        goto doneParse;
    }

    AvDynamicArray dependencies;
    avDynamicArrayCreate(0, sizeof(AvString), &dependencies);
    for(uint32 i = 0; i < count; i++){
        AvString dependency = vals[i].asString;
        // find first rule with matching targets;
        bool32 foundRule = false;
        struct Rule rule;
        for(uint32 j = 0; j < avDynamicArrayGetSize(rules); j++){
            avDynamicArrayRead(&rule, j, rules);
            bool32 found = false;
            avDynamicArrayForEachElement(AvString, rule.targets, {
                if(avStringEquals(element, dependency)){
                    found = true;
                    break;
                }
            });
            if(found){
                foundRule = true;
                break;
            }
        }
        if(!foundRule){
            continue;
        }

        // add all target dependency that are not already in the list
        avDynamicArrayForEachElement(AvString, rule.dependencies, {
            bool32 unique = true;
            for(uint32 k = 0; k < avDynamicArrayGetSize(dependencies); k++){
                AvString dep = {0};
                avDynamicArrayRead(&dep, k, dependencies);
                if(avStringEquals(dep, element)){
                    unique = false;
                    break;
                }
            }
            if(unique){
                avDynamicArrayAdd(&element, dependencies);
            }
        });
    }

    if(avDynamicArrayGetSize(dependencies)==0){
        goto doneConvert;
    }
    //convert to value
    struct ConstValue* results = avAllocatorAllocate(sizeof(struct ConstValue)*avDynamicArrayGetSize(dependencies), &project->allocator);
    for(uint32 i = 0; i < avDynamicArrayGetSize(dependencies); i++){
        
        AvString str = {0};
        avDynamicArrayRead(&str, i, dependencies);
        AvString tmpStr = AV_EMPTY;
        avStringCopyToAllocator(str, &tmpStr, &project->allocator);
        struct ConstValue res = {
            .type = VALUE_TYPE_STRING,
            .asString = tmpStr,
        };
        memcpy(results+i, &res, sizeof(struct ConstValue));
    }

    if(avDynamicArrayGetSize(dependencies) == 1){
        struct Value res = {0};
        toValue(results[0], &res);
        memcpy(&result, &res, sizeof(struct Value));
    }else{
        struct Value ret = (struct Value){
            .type=VALUE_TYPE_ARRAY,
            .asArray = {
                .count = avDynamicArrayGetSize(dependencies),
                .values = results,
            },
        };
        memcpy(&result, &ret, sizeof(struct Value));
    }

doneConvert:
    avDynamicArrayDestroy(dependencies);
doneParse:
    avDynamicArrayDestroy(rules);
    avDynamicArrayDestroy(currentString);
    deallocateRule(&currentRule,0); // deallocate uncommitted rule
    avStringFree(&fileContent);
    avFree(buffer);
    avFileClose(file);
    avFileHandleDestroy(file);
    avStringDebugContextEnd;
    return result;
}

#define STR(x) #x

struct Value readFileLines(Project* project, uint32 valueCount, struct Value* values){
    struct Value result = {.type = VALUE_TYPE_ARRAY, .asArray.count = 0};
    AvDynamicArray lines = {0};
    avDynamicArrayCreate(0, sizeof(AvString), &lines);
    
    struct ConstValue tmpValue = {0};
    uint32 count = 1;
    struct ConstValue* vals = &tmpValue;
    if(values[0].type == VALUE_TYPE_ARRAY){
        count = values[0].asArray.count;
        vals = values[0].asArray.values;
    }else{
        toConstValue(values[0], vals, project);
    }
    if(count == 0){
        return result;
    }

    for(uint32 i = 0; i < count; i++){
        if(vals[i].type != VALUE_TYPE_STRING){
            runtimeError(project, "Invalid variable type in argument %S", builtInFunctions[BUILT_IN_FUNC_ID_deleteDir].argTypes[i].name);
            return result;
        }
        
        AvString str = vals[i].asString;
        
        AvFile file = avFileHandleCreate(str);
        if(!avFileExists(file)){
            avFileHandleDestroy(file);
            continue;
        }
        if(!avFileOpen(file, AV_FILE_OPEN_READ_DEFAULT)){
            avFileHandleDestroy(file);
            continue;
        }
        uint64 size = avFileGetSize(file);
        if(size == 0){
            avFileHandleDestroy(file);
            continue;
        }

        AvStringMemory memory = {0};
        avStringMemoryAllocate(size+1, &memory);
        avFileRead(memory.data, size, file);
        avFileHandleDestroy(file);

        AvString fileContent = {0};
        avStringFromMemory(&fileContent, 0, size, &memory);
        AvString fileContentStr = {0};
        avStringReplace(&fileContentStr, fileContent, AV_CSTRA("\r\n"), AV_CSTRA("\n"));
        avStringFree(&fileContent);

        AvArray lineStrs = {0};
        avStringSplitOnChar(&lineStrs, '\n', fileContentStr);
        avStringFree(&fileContentStr);
        avArrayForEachElement(AvString, line, j, &lineStrs, {
            AvString tmp = AV_CSTRA("");
            if(!avStringIsEmpty(line)){
                avStringClone(&tmp, line);
            }
            avDynamicArrayAdd(&tmp, lines);
        });
        avArrayFree(&lineStrs);
    }

    uint32 lineCount = avDynamicArrayGetSize(lines);
    if(lineCount == 1){
        AvString tmp;
        avDynamicArrayRead(&tmp, 0, lines);
        avStringCopyToAllocator(tmp, &result.asString, &project->allocator);
        result.type = VALUE_TYPE_STRING;
    }else if(lineCount != 0){
        struct ConstValue* retVals = avAllocatorAllocate(sizeof(struct ConstValue)*lineCount, &project->allocator);
        for(uint32 index = 0; index < avDynamicArrayGetSize(lines); index++) { 
            AvString element; avDynamicArrayRead(&element, index, (lines)); 
            
            retVals[index].type = VALUE_TYPE_STRING; 
            if(avStringIsEmpty(element)){ 
                AvString tmp = AV_CSTRA("");
                avStringUnsafeCopy(&retVals[index].asString, tmp); 
                continue; 
            } 
            avStringCopyToAllocator(element, &retVals[index].asString, &project->allocator); 
            avStringFree(&element);
        };
        result.asArray.count = lineCount;
        result.asArray.values = retVals;
    }

    avDynamicArrayDestroy(lines);
    return result;

}

struct Value writeFileLines(Project* project, uint32 valueCount, struct Value* values){
    struct Value result = {.type = VALUE_TYPE_NUMBER, .asNumber = 0};
    
    AvString filePath = values[0].asString;

    struct ConstValue tmpConst = {0};
    struct ConstValue* lines = &tmpConst;
    uint32 lineCount = 1;

    if(values[1].type == VALUE_TYPE_ARRAY){
        lines = values[1].asArray.values;
        lineCount = values[1].asArray.count;
    }else{
        toConstValue(values[1], &tmpConst, project);
    }

    AvFile file = avFileHandleCreate(filePath);
    if(!avFileOpen(file, AV_FILE_OPEN_WRITE_DEFAULT)){
        avFileHandleDestroy(file);
        return result;
    }
    AvFileDescriptor fd = avFileGetDescriptor(file);
    for(uint32 i = 0; i < lineCount; i++){
        struct ConstValue val = lines[i];
        switch(val.type){
            case VALUE_TYPE_NUMBER:
                avStringPrintfToFileDescriptor(fd, AV_CSTRA("%lli\n"), val.asNumber);
                break;
            case VALUE_TYPE_STRING:
                avStringPrintfToFileDescriptor(fd, AV_CSTRA("%S\n"), val.asString);
                break;
            default:
                runtimeError(project, "invalid value type");
                break;
        }
    }

    avFileHandleDestroy(file);
    result.asNumber = 1;
    return result;

}

struct Value filterUnique(Project* project, uint32 valueCount, struct Value* values){
    struct Value result = {.type = VALUE_TYPE_ARRAY, .asArray.count = 0};
    if(values[0].type != VALUE_TYPE_ARRAY){
        return values[0];
    }
    uint32 maxItemCount = values[0].asArray.count;
    if(maxItemCount == 0){
        return result;
    }
    if(maxItemCount == 1){
        toValue(values[0].asArray.values[0], &result);
        return result;
    }

    struct ConstValue* vals = avAllocatorAllocate(sizeof(struct ConstValue)*maxItemCount, &project->allocator);
    uint32 uniqueCount = 0;
    for(uint32 i = 0; i < values[0].asArray.count; i++){
        struct ConstValue val = values[0].asArray.values[i];
        if(val.type == VALUE_TYPE_NONE){
            runtimeError(project, "unable to determine uniqueness of none value");
            return result;
        }
        
        bool32 unique = true;
        for(uint32 j = 0; j < uniqueCount; j++){
            if(vals[j].type != val.type){
                continue;
            }
            switch(val.type){
                case VALUE_TYPE_NUMBER:
                    if(val.asNumber==vals[j].asNumber){
                        unique = false;
                    }
                    break;
                case VALUE_TYPE_STRING:
                    if(avStringEquals(val.asString, vals[j].asString)){
                        unique = false;
                    }
                    break;
                default:
                    runtimeError(project, "logic error");
                    break;
            }
        }
        if(unique){
            avMemcpy(&vals[uniqueCount++], &val, sizeof(struct ConstValue));
        }
    }
    result.asArray.values = vals;
    result.asArray.count = uniqueCount;
    return result;
}


struct Value splitString(Project* project, uint32 valueCount, struct Value* values){


    AvString str = values[0].asString;
    AvString split = values[1].asString;

    AvArray strs = {0};
    uint32 subStrings = avStringSplit(&strs, split, str);

    if(subStrings == 0){
        avArrayFree(&strs);
        return values[0];
    }

    struct ConstValue* vals = avAllocatorAllocate(sizeof(struct ConstValue)*subStrings, &project->allocator);
    for(uint32 index = 0; index < (&strs)->count; index++) { 
        AvString subStr; avArrayRead(&subStr, index, (&strs)); 
        { 
            avMemset(vals + index, 0, sizeof(struct ConstValue));
            vals[index].type = VALUE_TYPE_STRING;
            avStringCopyToAllocator(subStr, &vals[index].asString, &project->allocator); 
        }
    };
    avArrayFree(&strs);

    return (struct Value){
        .type = VALUE_TYPE_ARRAY,
        .asArray = {
            .count = subStrings,
            .values = vals,
        },
    };
}

struct Value decodeHexNumber(Project* project, uint32 valueCount, struct Value* values){
    struct Value ret = {
        .type = VALUE_TYPE_NUMBER,
        .asNumber = -1,
    };

    AvString str = values[0].asString;

    uint64 value = 0;
    bool8 prefix = false;
    for(uint32 i = 0; i < str.len; i++){

        
        char c = str.chrs[i];
        if(i == 0 && c == '0'){
            prefix = true;
            continue;
        }
        if(prefix && i == 1 && (c=='x' || c=='X')){
            continue;
        }

        if(!avCharIsHexNumber(c)){
            return ret;
        }

        c = avCharToLowercase(c);
        switch(c){
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                value += c - '0';
                break;
            case 'a':
            case 'b':
            case 'c':
            case 'd':
            case 'e':
            case 'f':
                value += c - 'a' + 10;
                break;
        }
        if(i != str.len - 1){
            value <<= 4;
        }
    }

    ret.asNumber = value;

    return ret;
}

struct Value decodeNumber(Project* project, uint32 valueCount, struct Value* values){
    struct Value ret = {
        .type = VALUE_TYPE_NUMBER,
        .asNumber = -1,
    };

    AvString str = values[0].asString;

    uint64 value = 0;
    for(uint32 i = 0; i < str.len; i++){

        
        char c = str.chrs[i];

        if(!avCharIsNumber(c)){
            return ret;
        }

        c = avCharToLowercase(c);
        value += c - '0';
        if(i != str.len - 1){
            value *= 10;
        }
    }

    ret.asNumber = value;

    return ret;
}

struct Value encodeHexNumber(Project* project, uint32 valueCount, struct Value* values){
    char buffer[512];

    struct Value ret = {
        .type = VALUE_TYPE_STRING,
        .asString = AV_CSTRA("0"),
    };

    uint64 num = values[0].asNumber;
    uint32 len = 0;
    // print hex number
    //avStringPrintfToBuffer(buffer, 511, AV_CSTR("%x"), num);
    // Inline hex conversion (like %x)
    {
        static const char hex[] = "0123456789abcdef";
        char tmp[32];          // enough for 64-bit hex
        int i = 0;

        if (num == 0) {
            buffer[0] = '0';
            buffer[1] = '\0';
            len = 1;
        } else {
            while (num > 0) {
                tmp[i++] = hex[num & 0xF];
                num >>= 4;
            }

            // reverse into buffer
            int j = 0;
            while (i > 0) {
                buffer[j++] = tmp[--i];
            }
            buffer[j] = '\0';
            len = j;
        }
    }
    buffer[len] = '\0';
    avStringCopyToAllocator(AV_STR(buffer, len), &ret.asString, &project->allocator);
    return ret;
}

struct Value getStringChar(Project* project, uint32 valueCount, struct Value* values){

    struct Value ret = {
        .type = VALUE_TYPE_STRING,
        .asString = AV_CSTRA(""),
    };

    AvString str = values[0].asString;
    if(str.len == 0){
        return ret;
    }
    if(str.len <= values[1].asNumber){
        return ret;
    }
    AvString chr = (AvString){.chrs = str.chrs + values[1].asNumber, .len = 1, .memory = nullptr};
    avStringCopyToAllocator(chr, &ret.asString, &project->allocator);
    return ret;
}



struct Value truncateFile(Project* project, uint32 valueCount, struct Value* values){

    struct Value result = {
        .type = VALUE_TYPE_NUMBER,
        .asNumber = 0,
    };

    AvString filePath = values[0].asString;
    
    AvFile file = avFileHandleCreate(filePath);
    if(!avFileOpen(file, AV_FILE_OPEN_READ_BINARY_DEFAULT)){
        avFileHandleDestroy(file);
        return result;
    }
    uint64 truncSize = values[1].asNumber;
    uint64 size = avFileGetSize(file);
    if(size == 0){
        avFileHandleDestroy(file);
        return result;
    }
    byte* buffer = avAllocate(truncSize, "file buffer");
    avMemset(buffer, 0xff, truncSize);
    avFileRead(buffer, AV_MIN(truncSize, size), file);
    avFileClose(file);
    if(!avFileOpen(file, AV_FILE_OPEN_WRITE_BINARY_DEFAULT)){
        avFileHandleDestroy(file);
        avFree(buffer);
        return result;
    }
    avFileWrite(buffer, truncSize, file);
    avFree(buffer);
    avFileHandleDestroy(file);
    result.asNumber = 1;
    return result;
}

struct Value trimString(Project* project, uint32 valueCount, struct Value* values){

    struct Value ret = {
        .type = VALUE_TYPE_STRING,
        .asString = values[0].asString,
    };

    AvString str = values[0].asString;

    uint32 start = 0;
    uint32 end = str.len;

    // Trim leading whitespace
    while(start < end){
        char c = str.chrs[start];
        if(c != ' ' && c != '\t' && c != '\n' && c != '\r'){
            break;
        }
        start++;
    }

    // Trim trailing whitespace
    while(end > start){
        char c = str.chrs[end - 1];
        if(c != ' ' && c != '\t' && c != '\n' && c != '\r'){
            break;
        }
        end--;
    }

    // If unchanged, return original string
    if(start == 0 && end == str.len){
        return ret;
    }

    // Create trimmed string
    AvString trimmed = AV_STR(
        str.chrs + start,
        end - start
    );

    avStringCopyToAllocator(
        trimmed,
        &ret.asString,
        &project->allocator
    );

    return ret;
}

struct Value stringContains(Project* project, uint32 valueCount, struct Value* values){

    struct Value ret = {
        .type = VALUE_TYPE_NUMBER,
        .asNumber = 0,
    };

    AvString str = values[0].asString;
    if(avStringContains(str, values[1].asString)){
        ret.asNumber = 1;
    }

    return ret;

}

struct Value stringStartsWith(Project* project, uint32 valueCount, struct Value* values){

    struct Value ret = {
        .type = VALUE_TYPE_NUMBER,
        .asNumber = 0,
    };

    AvString str = values[0].asString;
    if(avStringStartsWith(str, values[1].asString)){
        ret.asNumber = 1;
    }

    return ret;

}

struct Value stringEndsWith(Project* project, uint32 valueCount, struct Value* values){

    struct Value ret = {
        .type = VALUE_TYPE_NUMBER,
        .asNumber = 0,
    };

    AvString str = values[0].asString;
    if(avStringEndsWith(str, values[1].asString)){
        ret.asNumber = 1;
    }

    return ret;

}
struct Value stringLength(Project* project, uint32 valueCount, struct Value* values){

    struct Value ret = {
        .type = VALUE_TYPE_NUMBER,
        .asNumber = 0,
    };

    AvString str = values[0].asString;
    ret.asNumber = str.len;
    return ret;

}

struct Value stringGetSection(Project* project, uint32 valueCount, struct Value* values){

    struct Value ret = {
        .type = VALUE_TYPE_STRING,
        .asString = AV_EMPTY_STRING,
    };

    AvString str = values[0].asString;
    AvString tmp = AV_EMPTY;
    int64 start = values[1].asNumber;
    if(start < 0){
        return ret;
    }
    if(start >= str.len){
        return ret;
    }
    int64 end =  values[2].asNumber;
    if(end >= str.len){
        end = str.len - 1;
    }
    if(end < start){
        return ret;
    }
    uint64 length = end - start + 1;
    avStringCopySection(&tmp, start, length, str);
    avStringCopyToAllocator(tmp, &ret.asString, &project->allocator);
    avStringFree(&tmp);
    return ret;

}

struct Value stringMinimizeWhitespace(Project* project, uint32 valueCount, struct Value* values){

    struct Value ret = {
        .type = VALUE_TYPE_STRING,
        .asString = AV_EMPTY_STRING,
    };

    AvString str = values[0].asString;

    AvStringMemory memory = AV_EMPTY;
    avStringMemoryAllocate(str.len, &memory);
    uint64 index = 0;
    bool32 hasText = false;
    for(uint64 i = 0; i < str.len; i++){
        char c = str.chrs[i];
        if(avCharIsWhiteSpace(c)){
            if(hasText){
                memory.data[index++] = ' ';
                hasText = false;
            }
            continue;
        }
        hasText = true;
        memory.data[index++] = c;
    }
    if(index == 0){
        return ret;
    }
    AvString tmp = AV_EMPTY;
    avStringFromMemory(&tmp, 0, index, &memory);
    avStringCopyToAllocator(tmp, &ret.asString, &project->allocator);
    avStringFree(&tmp);
    return ret;

}

struct Value formatFloat(Project* project, uint32 valueCount, struct Value* values){

    struct Value ret = {
        .type = VALUE_TYPE_STRING,
        .asString = AV_CSTRA("0"),
    };

    int64 num = values[0].asNumber;
    AvStringMemory memory = AV_EMPTY;
    avStringMemoryAllocate(512, &memory);
    uint64 index = 0;
    if(num==0){
        return ret;
    }
    bool32 neg = false;
    if(num < 0){
        neg = true;
        num = -num;
    }

    while(num){
        int64 digit = num % 10;
        memory.data[index++] = '0' + digit;
        if(index == values[1].asNumber){
            memory.data[index++] = '.';
        }
        num /= 10;
    }
    if(values[1].asNumber == index-1){
        memory.data[index++] = '0';
    }
    if(neg){
        memory.data[index++] = '-';
    }


    AvString tmp = AV_EMPTY;
    avStringFromMemory(&tmp, 0, index, &memory);
    avStringFlip(&tmp);
    avStringCopyToAllocator(tmp, &ret.asString, &project->allocator);
    avStringFree(&tmp);
    return ret;

}

struct Value readFileRaw(Project* project, uint32 valueCount, struct Value* values){
    struct Value result = {.type = VALUE_TYPE_STRING, .asString = {0}};
    
    AvString str = values[0].asString;

    AvFile file = avFileHandleCreate(str);
    if(!avFileExists(file)){
        avFileHandleDestroy(file);
        return result;
    }
    
    if(!avFileOpen(file, AV_FILE_OPEN_READ_BINARY_DEFAULT)){
        avFileHandleDestroy(file);
        return result;
    }
    
    uint64 size = avFileGetSize(file);
    if(size == 0){
        avFileHandleDestroy(file);
        return result;
    }
    
    AvStringMemory memory = {0};
    avStringMemoryAllocate(size+1, &memory);
    avFileRead(memory.data, size, file);
    avFileHandleDestroy(file);
    avStringFromMemory(&result.asString, 0, size, &memory);
    return result;

}

struct Value writeFileRaw(Project* project, uint32 valueCount, struct Value* values){
    struct Value result = {.type = VALUE_TYPE_NUMBER, .asNumber = 0};
    
    AvString filePath = values[0].asString;

    struct ConstValue tmpConst = {0};
    struct ConstValue* lines = &tmpConst;
    uint32 lineCount = 1;

    if(values[1].type == VALUE_TYPE_ARRAY){
        lines = values[1].asArray.values;
        lineCount = values[1].asArray.count;
    }else{
        toConstValue(values[1], &tmpConst, project);
    }

    AvFile file = avFileHandleCreate(filePath);
    if(!avFileOpen(file, AV_FILE_OPEN_WRITE_BINARY_DEFAULT)){
        avFileHandleDestroy(file);
        return result;
    }
    for(uint32 i = 0; i < lineCount; i++){
        struct ConstValue val = lines[i];
        switch(val.type){
            case VALUE_TYPE_NUMBER:
                byte data = val.asNumber & 0xff;
                avFileWrite(&data, 1, file);
                break;
            case VALUE_TYPE_STRING:
                avFileWrite((void*)val.asString.chrs, val.asString.len, file);
                break;
            default:
                runtimeError(project, "invalid value type");
                break;
        }
    }

    avFileHandleDestroy(file);
    result.asNumber = 1;
    return result;

}