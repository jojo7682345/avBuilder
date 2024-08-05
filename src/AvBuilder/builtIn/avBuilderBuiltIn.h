#ifndef __AV_BUILDER_BUILT_IN_FUNCTIONS__
#define __AV_BUILDER_BUILT_IN_FUNCTIONS__

#include <AvUtils/avString.h>
#include "../avProjectLang.h"
#include "../avBuilder.h"

#define VALUE_TYPE_ALL VALUE_TYPE_ARRAY|VALUE_TYPE_STRING|VALUE_TYPE_NUMBER

#define BUILT_IN_FUNCS \
    BUILT_IN_FUNC(fileName, {VALUE_TYPE_STRING})\
    BUILT_IN_FUNC(fileBaseName, {VALUE_TYPE_STRING})\
    BUILT_IN_FUNC(arraySize, {VALUE_TYPE_ALL})\

void runtimeError(Project* project, const char* message, ...);


#define BUILT_IN_FUNC(func, args) struct Value func(uint32 valueCount, struct Value* values);
BUILT_IN_FUNCS
#undef BUILT_IN_FUNC

struct BuiltInFunctionDescription{
    AvString identifier;
    uint32 argumentCount;
    enum ValueType* argTypes;
    struct Value (*function)(uint32, struct Value*);
};

extern const struct BuiltInFunctionDescription builtInFunctions[];
extern const uint32 builtInFunctionCount;

bool32 isBuiltInFunction(struct BuiltInFunctionDescription* description, AvString identifier, Project* project);

struct Value callBuiltInFunction(struct BuiltInFunctionDescription description, uint32 argumentCount, struct Value* values, Project* project);



#endif//__AV_BUILDER_BUILT_IN_FUNCTIONS__