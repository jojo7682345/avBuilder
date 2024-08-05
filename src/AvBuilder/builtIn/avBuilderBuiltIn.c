#include "avBuilderBuiltIn.h"
#include <string.h>

#define TS(str) sizeof(#str)


#define BUILT_IN_FUNC(func, args) {\
    .identifier=AV_CSTRA(#func),\
    .argumentCount = sizeof((enum ValueType[])args)/sizeof(enum ValueType),\
    .argTypes = (enum ValueType[]) args,\
    .function = func\
},
const struct BuiltInFunctionDescription builtInFunctions[] = {
    BUILT_IN_FUNCS
};
const uint32 builtInFunctionCount = sizeof(builtInFunctions)/sizeof(struct BuiltInFunctionDescription);
#undef BUILT_IN_FUNC

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

    return description.function(argumentCount, values);

}

struct Value fileName(uint32 valueCount, struct Value* values){
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

struct Value fileBaseName(uint32 valueCount, struct Value* values){
    AvString file = values[0].asString;
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

struct Value arraySize(uint32 valueCount, struct Value* values){
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

