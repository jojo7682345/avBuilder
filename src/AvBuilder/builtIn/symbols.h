
#define BUILT_IN_VARS \
    BUILT_IN_VAR(FILTER_TYPE_ENDS_WITH, VALUE_TYPE_NUMBER, 0)\
    BUILT_IN_VAR(FILTER_TYPE_STARTS_WITH, VALUE_TYPE_NUMBER, 1)

// name, args, const
#define BUILT_IN_FUNCS \
    BUILT_IN_FUNC(fileName, false, {{ .type=VALUE_TYPE_STRING, .name="filePath" }})\
    BUILT_IN_FUNC(fileExists, false, {{ .type=VALUE_TYPE_STRING, .name="filePath" }})\
    BUILT_IN_FUNC(fileFullName, false, {{ .type=VALUE_TYPE_STRING, .name="filePath" }})\
    BUILT_IN_FUNC(fileBaseName, false, {{ .type=VALUE_TYPE_STRING, .name="filePath" }})\
    BUILT_IN_FUNC(filePath, false, {{ .type=VALUE_TYPE_STRING, .name="filePath" }})\
    BUILT_IN_FUNC(fileLastModified, false, {{ .type=VALUE_TYPE_STRING, .name="filePath" }})\
    BUILT_IN_FUNC(arraySize, false, {{ .type=VALUE_TYPE_ALL, .name="array" }})\
    BUILT_IN_FUNC(filter, false, {{ .type=VALUE_TYPE_NUMBER, .name="filterType" }, { .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="filter" }, { .type=VALUE_TYPE_ALL, .name="listToFilter" }})\
    BUILT_IN_FUNC(makeDir, false, {{ .type=VALUE_TYPE_STRING, .name="path" }})\
    BUILT_IN_FUNC(deleteDir, false, {{ .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="path" }})\
    BUILT_IN_FUNC(deleteFile, false, {{ .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="file" }})\
    BUILT_IN_FUNC(makeDirs, false, {{ .type=VALUE_TYPE_STRING, .name="path" }})\
    BUILT_IN_FUNC(print, false, { { .type=VALUE_TYPE_ALL, .name="value" } })\
    BUILT_IN_FUNC(println, false, { { .type=VALUE_TYPE_ALL, .name="value" } })\
    BUILT_IN_FUNC(toUppercase, false, { { .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="string" } })\
    BUILT_IN_FUNC(toLowercase, false, { { .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="string" } })\
    BUILT_IN_FUNC(changeDir, false, { { .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="path" } })\
    BUILT_IN_FUNC(currentDir, false, {})\
    BUILT_IN_FUNC(parseDependencies, false, {{ .type=VALUE_TYPE_STRING, .name="filePath" }, { .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="targets" } })\
    BUILT_IN_FUNC(readFileLines, false, {{ .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="filePath" } })\
    BUILT_IN_FUNC(writeFileLines, false, {{ .type=VALUE_TYPE_STRING, .name="filePath" }, { .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="lines" } })\
    BUILT_IN_FUNC(filterUnique, false, { { .type=VALUE_TYPE_ARRAY, .name="list" } })\
    BUILT_IN_FUNC(compileString, true, { { .type=VALUE_TYPE_STRING|VALUE_TYPE_ARRAY, .name="string" } })\
    BUILT_IN_FUNC(call, false, {{ .type=VALUE_TYPE_STRING, .name="functionName" }})\
    BUILT_IN_FUNC(callExtern, false, {{ .type=VALUE_TYPE_STRING, .name="projectFile" }, { .type=VALUE_TYPE_STRING, .name="functionName" } })\
    BUILT_IN_FUNC(splitString, false, {{ .type=VALUE_TYPE_STRING, .name="str"}, { .type=VALUE_TYPE_STRING, .name="split"}})\
    BUILT_IN_FUNC(decodeHexNumber, false, {{ .type=VALUE_TYPE_STRING, .name="str"}})\
    BUILT_IN_FUNC(decodeNumber, false, {{ .type=VALUE_TYPE_STRING, .name="str"}})\
    BUILT_IN_FUNC(encodeHexNumber, false, {{ .type=VALUE_TYPE_NUMBER, .name="number"}})\
    BUILT_IN_FUNC(getStringChar, false, {{ .type=VALUE_TYPE_STRING, .name="str"}, { .type=VALUE_TYPE_NUMBER, .name="index"}})\
    BUILT_IN_FUNC(truncateFile, false, {{ .type=VALUE_TYPE_STRING, .name="file"}, { .type=VALUE_TYPE_NUMBER, .name="size"}})\
    BUILT_IN_FUNC(readFileRaw, false, {{ .type=VALUE_TYPE_STRING, .name="file"}})\
    BUILT_IN_FUNC(writeFileRaw, false, {{ .type=VALUE_TYPE_STRING, .name="file"}, { .type=VALUE_TYPE_ALL, .name="data"}})\
    BUILT_IN_FUNC(trimString, false, {{ .type=VALUE_TYPE_STRING, .name="str"}})\
    BUILT_IN_FUNC(stringContains, false, {{ .type=VALUE_TYPE_STRING, .name="str"}, { .type=VALUE_TYPE_STRING, .name="find"}})\
    BUILT_IN_FUNC(stringStartsWith, false, {{ .type=VALUE_TYPE_STRING, .name="str"}, { .type=VALUE_TYPE_STRING, .name="find"}})\
    BUILT_IN_FUNC(stringEndsWith, false, {{ .type=VALUE_TYPE_STRING, .name="str"}, { .type=VALUE_TYPE_STRING, .name="find"}})\
    BUILT_IN_FUNC(stringLength, false, {{ .type=VALUE_TYPE_STRING, .name="str"}})\
    BUILT_IN_FUNC(stringMinimizeWhitespace, false, {{ .type=VALUE_TYPE_STRING, .name="str"}})\
    BUILT_IN_FUNC(stringGetSection, false, {{ .type=VALUE_TYPE_STRING, .name="str"}, {.type=VALUE_TYPE_NUMBER, .name="start"}, {.type=VALUE_TYPE_NUMBER, .name="end"}})\
    BUILT_IN_FUNC(formatFloat, false, {{ .type=VALUE_TYPE_NUMBER, .name="number"}, {.type=VALUE_TYPE_NUMBER, .name="decimals"}})\

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