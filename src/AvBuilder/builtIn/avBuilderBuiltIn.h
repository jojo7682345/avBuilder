#ifndef __AV_BUILDER_BUILT_IN_FUNCTIONS__
#define __AV_BUILDER_BUILT_IN_FUNCTIONS__

#include <AvUtils/avString.h>
#include "../avProjectLang.h"
#include "../avBuilder.h"

#define VALUE_TYPE_ALL VALUE_TYPE_ARRAY|VALUE_TYPE_STRING|VALUE_TYPE_NUMBER

#include "symbols.h"

void runtimeError(Project* project, const char* message, ...);
void toConstValue(struct Value value, struct ConstValue* val, Project* project);
void toValue(struct ConstValue value, struct Value* val);
uint32 processArg(AvString arg, AvDynamicArray chars, Project* project);


#define BUILT_IN_FUNC(func, ...) struct Value func(Project* project, uint32 valueCount, struct Value* values);
BUILT_IN_FUNCS
#undef BUILT_IN_FUNC



struct BuiltInFunctionDescription{
    AvString identifier;
    uint32 argumentCount;
    enum ValueType* argTypes;
    struct Value (*function)(Project*, uint32, struct Value*);
};

struct BuiltInVariableDescription{
    AvString identifier;
    struct Value value;
};

extern const struct BuiltInFunctionDescription builtInFunctions[];
extern const uint32 builtInFunctionCount;
extern const struct BuiltInVariableDescription builtInVariables[];
extern const uint32 builtInVariableCount;

bool32 isBuiltInFunction(struct BuiltInFunctionDescription* description, AvString identifier, Project* project);

struct Value callBuiltInFunction(struct BuiltInFunctionDescription description, uint32 argumentCount, struct Value* values, Project* project);



#endif//__AV_BUILDER_BUILT_IN_FUNCTIONS__