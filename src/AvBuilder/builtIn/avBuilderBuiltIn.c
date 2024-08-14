#include "avBuilderBuiltIn.h"
#include <string.h>
#include <AvUtils/avMemory.h>
#include <unistd.h>

#define TS(str) sizeof(#str)


#define BUILT_IN_FUNC(func, ...) {\
    .identifier=AV_CSTRA(#func),\
    .argumentCount = sizeof((enum ValueType[])__VA_ARGS__)/sizeof(enum ValueType),\
    .argTypes = (enum ValueType[]) __VA_ARGS__,\
    .function = func\
},
const struct BuiltInFunctionDescription builtInFunctions[] = {
    BUILT_IN_FUNCS
};
const uint32 builtInFunctionCount = sizeof(builtInFunctions)/sizeof(struct BuiltInFunctionDescription);
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

void getUsage(AvStringRef str, AvString funcName, uint32 argCount, enum ValueType* args){
    
    AvStringMemory memory =AV_EMPTY;

    uint32 len = 0;
    for(uint32 i = 0; i < argCount; i++){
        enum ValueType type = args[i];
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
        len+=2;
    }


    avStringMemoryAllocate(funcName.len + 2 + len, &memory);
    
    avStringMemoryStore(funcName, 0, AV_STRING_FULL_LENGTH, &memory);
    avStringMemoryStore(AV_CSTRA("("), funcName.len, AV_STRING_FULL_LENGTH, &memory);
    uint32 index = funcName.len + 1;

    for(uint32 i = 0; i < argCount; i++){
        enum ValueType type = args[i];
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

        avStringMemoryStore(AV_CSTRA(", "), index, AV_STRING_FULL_LENGTH, &memory);
        index+=2;
    }

    avStringFromMemory(str, AV_STRING_WHOLE_MEMORY, &memory);

}

struct Value callBuiltInFunction(struct BuiltInFunctionDescription description, uint32 argumentCount, struct Value* values, Project* project){

    if(description.argumentCount != argumentCount){
        AvString usage = AV_EMPTY;
        getUsage(&usage, description.identifier, description.argumentCount, description.argTypes);
        runtimeError(project, 
            "Calling built-in function '%s' with an invalid amount of arguments\nUsage: %s", 
            description.identifier, usage);
        avStringFree(&usage);
    }

    for(uint32 i = 0; i < argumentCount; i++){
        if((description.argTypes[i] & values[i].type) == 0){
            AvString usage = AV_EMPTY;
            getUsage(&usage, description.identifier, description.argumentCount, description.argTypes);
            runtimeError(project, 
                "Calling built-in function '%s' with an invalid argument\nUsage: %s", 
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

    return (struct Value){
        .type = VALUE_TYPE_STRING,
        .asString = str,
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

    return (struct Value){
        .type = VALUE_TYPE_STRING,
        .asString = str,
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

struct Value makeDir(Project* project, uint32 valueCount, struct Value* values){
    AvString dir = AV_EMPTY;
    avStringClone(&dir, values[0].asString);
    int ret = mkdir(dir.chrs, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
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
#include <linux/limits.h>
#include <stdio.h>

static int _mkdir(const char *dir, unsigned int mode) {
    char tmp[PATH_MAX];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp),"%s",dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/'){
        tmp[len - 1] = 0;
    }
    for (p = tmp + 1; *p; p++){
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, mode);
            *p = '/';
        }
    }
    return mkdir(tmp, mode);
}

struct Value makeDirs(Project* project, uint32 valueCount, struct Value* values){
    AvString dir = AV_EMPTY;
    avStringClone(&dir, values[0].asString);
    int ret = _mkdir(dir.chrs, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
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
        
        struct ConstValue res = {
            .type = VALUE_TYPE_NUMBER,
            .asNumber = chdir(str.chrs),
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

struct Value currentDir(Project* project, uint32 valueCount, struct Value* values){
    
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
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