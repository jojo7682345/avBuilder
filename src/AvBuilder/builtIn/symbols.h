
#define BUILT_IN_VARS \
    BUILT_IN_VAR(FILTER_TYPE_ENDS_WITH, VALUE_TYPE_NUMBER, 0)\
    BUILT_IN_VAR(FILTER_TYPE_STARTS_WITH, VALUE_TYPE_NUMBER, 1)

#define BUILT_IN_FUNCS \
    BUILT_IN_FUNC(fileName, {{ .type=VALUE_TYPE_STRING, .name="filePath" }})\
    BUILT_IN_FUNC(fileFullName, {{ .type=VALUE_TYPE_STRING, .name="filePath" }})\
    BUILT_IN_FUNC(fileBaseName, {{ .type=VALUE_TYPE_STRING, .name="filePath" }})\
    BUILT_IN_FUNC(filePath, {{ .type=VALUE_TYPE_STRING, .name="filePath" }})\
    BUILT_IN_FUNC(fileLastModified, {{ .type=VALUE_TYPE_STRING, .name="filePath" }})\
    BUILT_IN_FUNC(arraySize, {{ .type=VALUE_TYPE_ALL, .name="array" }})\
    BUILT_IN_FUNC(filter, {{ .type=VALUE_TYPE_NUMBER, .name="filterType" }, { .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="filter" }, { .type=VALUE_TYPE_ALL, .name="listToFilter" }})\
    BUILT_IN_FUNC(makeDir, {{ .type=VALUE_TYPE_STRING, .name="path" }})\
    BUILT_IN_FUNC(deleteDir, {{ .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="path" }})\
    BUILT_IN_FUNC(deleteFile, {{ .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="file" }})\
    BUILT_IN_FUNC(makeDirs, {{ .type=VALUE_TYPE_STRING, .name="path" }})\
    BUILT_IN_FUNC(print, { { .type=VALUE_TYPE_ALL, .name="value" } })\
    BUILT_IN_FUNC(println, { { .type=VALUE_TYPE_ALL, .name="value" } })\
    BUILT_IN_FUNC(toUppercase, { { .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="string" } })\
    BUILT_IN_FUNC(toLowercase, { { .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="string" } })\
    BUILT_IN_FUNC(changeDir, { { .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="path" } })\
    BUILT_IN_FUNC(currentDir, {})\
    BUILT_IN_FUNC(parseDependencies, {{ .type=VALUE_TYPE_STRING, .name="filePath" }, { .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="targets" } })\
    BUILT_IN_FUNC(readFileLines, {{ .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="filePath" } })\
    BUILT_IN_FUNC(writeFileLines, {{ .type=VALUE_TYPE_STRING, .name="filePath" }, { .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="lines" } })\
    BUILT_IN_FUNC(filterUnique, { { .type=VALUE_TYPE_ARRAY, .name="list" } })\
    BUILT_IN_FUNC(compileString, { { .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="string" } })\
    BUILT_IN_FUNC(call, {{ .type=VALUE_TYPE_STRING, .name="functionName" }})\
    BUILT_IN_FUNC(callExtern, {{ .type=VALUE_TYPE_STRING, .name="projectFile" }, { .type=VALUE_TYPE_STRING, .name="functionName" } })\
    BUILT_IN_FUNC(splitString, {{ .type=VALUE_TYPE_STRING, .name="str"}, { .type=VALUE_TYPE_STRING, .name="split"}})\
    BUILT_IN_FUNC(decodeHexNumber, {{ .type=VALUE_TYPE_STRING, .name="str"}})\
    BUILT_IN_FUNC(decodeNumber, {{ .type=VALUE_TYPE_STRING, .name="str"}})\
    BUILT_IN_FUNC(encodeHexNumber, {{ .type=VALUE_TYPE_NUMBER, .name="number"}})\
    BUILT_IN_FUNC(getStringChar, {{ .type=VALUE_TYPE_STRING, .name="str"}, { .type=VALUE_TYPE_NUMBER, .name="index"}})\
    BUILT_IN_FUNC(truncateFile, {{ .type=VALUE_TYPE_STRING, .name="file"}, { .type=VALUE_TYPE_NUMBER, .name="size"}})\
    BUILT_IN_FUNC(readFileRaw, {{ .type=VALUE_TYPE_STRING, .name="file"}})\
    BUILT_IN_FUNC(writeFileRaw, {{ .type=VALUE_TYPE_STRING, .name="file"}, { .type=VALUE_TYPE_ALL, .name="data"}})\
    BUILT_IN_FUNC(trimString, {{ .type=VALUE_TYPE_STRING, .name="str"}})\
    BUILT_IN_FUNC(stringContains, {{ .type=VALUE_TYPE_STRING, .name="str"}, { .type=VALUE_TYPE_STRING, .name="find"}})\
    BUILT_IN_FUNC(stringStartsWith, {{ .type=VALUE_TYPE_STRING, .name="str"}, { .type=VALUE_TYPE_STRING, .name="find"}})\
    BUILT_IN_FUNC(stringEndsWith, {{ .type=VALUE_TYPE_STRING, .name="str"}, { .type=VALUE_TYPE_STRING, .name="find"}})\
    BUILT_IN_FUNC(stringLength, {{ .type=VALUE_TYPE_STRING, .name="str"}})\
    BUILT_IN_FUNC(stringMinimizeWhitespace, {{ .type=VALUE_TYPE_STRING, .name="str"}})\
    BUILT_IN_FUNC(stringGetSection, {{ .type=VALUE_TYPE_STRING, .name="str"}, {.type=VALUE_TYPE_NUMBER, .name="start"}, {.type=VALUE_TYPE_NUMBER, .name="end"}})\
    BUILT_IN_FUNC(formatFloat, {{ .type=VALUE_TYPE_NUMBER, .name="number"}, {.type=VALUE_TYPE_NUMBER, .name="decimals"}})\

    // BUILT_IN_FUNC(fileOpenRead, { { .type=VALUE_TYPE_STRING, .name="" } })
    // BUILT_IN_FUNC(fileOpenWrite, { { .type=VALUE_TYPE_STRING, .name="" } })
    // BUILT_IN_FUNC(fileOpenAppend, { { .type=VALUE_TYPE_STRING, .name="" } })
    // BUILT_IN_FUNC(fileClose, { { .type=VALUE_TYPE_NUMBER, .name="" }})
    // BUILT_IN_FUNC(fileWrite, { { .type=VALUE_TYPE_NUMBER, .name="" }, { .type=VALUE_TYPE_ALL, .name="" }})
    // BUILT_IN_FUNC(fileWriteLine, { { .type=VALUE_TYPE_NUMBER, .name="" }, { .type=VALUE_TYPE_ALL, .name="" }})
    // BUILT_IN_FUNC(fileReadLine, { { .type=VALUE_TYPE_NUMBER, .name="" } })
    // BUILT_IN_FUNC(fileReadChars, { { .type=VALUE_TYPE_NUMBER, .name="" }, { .type=VALUE_TYPE_NUMBER, .name="" } })
    // BUILT_IN_FUNC(fileReadRaw, { { .type=VALUE_TYPE_NUMBER, .name="" }, { .type=VALUE_TYPE_NUMBER, .name="" } })
    // BUILT_IN_FUNC(fileWriteRaw, { { .type=VALUE_TYPE_NUMBER, .name="" }, VALUE_TYPE_})

// comment