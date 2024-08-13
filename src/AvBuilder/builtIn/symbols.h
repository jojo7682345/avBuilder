
#define BUILT_IN_VARS \
    BUILT_IN_VAR(FILTER_TYPE_ENDS_WITH, VALUE_TYPE_NUMBER, 0)\
    BUILT_IN_VAR(FILTER_TYPE_STARTS_WITH, VALUE_TYPE_NUMBER, 1)

#define BUILT_IN_FUNCS \
    BUILT_IN_FUNC(fileName, {VALUE_TYPE_STRING})\
    BUILT_IN_FUNC(fileBaseName, {VALUE_TYPE_STRING})\
    BUILT_IN_FUNC(filePath, {VALUE_TYPE_STRING})\
    BUILT_IN_FUNC(arraySize, {VALUE_TYPE_ALL})\
    BUILT_IN_FUNC(filter, {VALUE_TYPE_NUMBER, VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, VALUE_TYPE_ALL})\
    BUILT_IN_FUNC(makeDir, {VALUE_TYPE_STRING})\
    BUILT_IN_FUNC(makeDirs, {VALUE_TYPE_STRING})\
    BUILT_IN_FUNC(print, { VALUE_TYPE_ALL })\
    BUILT_IN_FUNC(println, { VALUE_TYPE_ALL })
