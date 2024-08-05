#ifndef __AV_REGEX__
#define __AV_REGEX__
#include "../avDefinitions.h"
C_SYMBOLS_START
#include "../avTypes.h"
#include "../avString.h"

typedef struct AvRegexResult {
    bool8 matched;
    uint32 charCount;
} AvRegexResult;

typedef enum AvRegexError {
    AV_REGEX_PARSE_SUCCES = 0,
    AV_REGEX_PARSE_ERROR,
    AV_REGEX_PARSE_INVALID_TOKEN,
    AV_REGEX_PARSE_UNEXPECTED_END,
}AvRegexError;

/// @brief matches the string to a regex string
/// @param regex the regular expression
/// @param str the string to perform the operation on
/// @param error (optional) error codes when parsing regular expression
/// @return if the string matched and the amount of characters
AvRegexResult avRegexMatch(AvString regex, AvString str, AvRegexError* error);


C_SYMBOLS_END
#endif//__AV_REGEX__