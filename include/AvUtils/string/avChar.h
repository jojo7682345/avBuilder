#ifndef __AV_CHAR__
#define __AV_CHAR__
#include "../avDefinitions.h"
C_SYMBOLS_START

#include "../avTypes.h"

bool32 avCharIsWithinRange(char chr, char start, char end);

bool32 avCharIsLetter(char chr);
bool32 avCharIsUppercaseLetter(char chr);
bool32 avCharIsLowercaseLetter(char chr);

bool32 avCharIsNumber(char chr);
bool32 avCharIsHexNumber(char chr);

char avCharToLowercase(char chr);
char avCharToUppercase(char chr);

bool32 avCharIsWhiteSpace(char chr);
bool32 avCharIsNewline(char chr);
bool32 avCharEqualsCaseInsensitive(char chrA, char chrB);

C_SYMBOLS_END
#endif//__AV_CHAR__