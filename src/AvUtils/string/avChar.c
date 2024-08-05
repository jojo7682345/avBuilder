#include <AvUtils/string/avChar.h>

bool32 avCharIsWithinRange(char chr, char start, char end) {
    return (chr >= start) && (chr <= end);
}

bool32 avCharIsLetter(char chr) {
    return avCharIsLowercaseLetter(chr) || avCharIsUppercaseLetter(chr);
}
bool32 avCharIsUppercaseLetter(char chr) {
    return avCharIsWithinRange(chr, 'A', 'Z');
}
bool32 avCharIsLowercaseLetter(char chr) {
    return avCharIsWithinRange(chr, 'a', 'z');
}

bool32 avCharIsNumber(char chr) {
    return avCharIsWithinRange(chr, '0', '9');
}
bool32 avCharIsHexNumber(char chr) {
    return avCharIsNumber(chr) || avCharIsWithinRange(chr, 'A', 'F') || avCharIsWithinRange(chr, 'a', 'f');
}

char avCharToLowercase(char chr) {
    return avCharIsUppercaseLetter(chr) ? chr + ('a' - 'A') : chr;
}
char avCharToUppercase(char chr) {
    return avCharIsLowercaseLetter(chr) ? chr - ('a' - 'A') : chr;
}

bool32 avCharIsWhiteSpace(char chr) {
    switch (chr) {
    case ' ':
    case '\n':
    case '\r':
    case '\t':
    case '\f':
        return true;
    default:
        return false;
    }
}

bool32 avCharIsNewline(char chr) {
    switch (chr) {
    case '\n':
    case '\r':
        return true;
    default:
        return false;
    }
}

bool32 avCharEqualsCaseInsensitive(char chrA, char chrB) {
    return avCharToUppercase(chrA) == avCharToUppercase(chrB);
}