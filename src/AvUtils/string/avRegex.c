#include <AvUtils/string/avRegex.h>
#define AV_DYNAMIC_ARRAY_EXPOSE_MEMORY_LAYOUT
#include <AvUtils/dataStructures/avDynamicArray.h>
#include <AvUtils/avMemory.h>
#include <AvUtils/avLogging.h>
#include <string.h>

enum Quantifier {
    QUANTIFIER_EXACTLY_ONE,
    QUANTIFIER_ZERO_OR_ONE,
    QUANTIFIER_ZERO_OR_MORE,
};

enum RegexType {
    REGEX_TYPE_NONE,
    REGEX_TYPE_LITERAL,
    REGEX_TYPE_WILDCARD,
    REGEX_TYPE_GROUP,
};

typedef struct RegexElement {
    enum RegexType type;
    enum Quantifier quantifier;
    union {
        char chr;
        struct RegexElement* content;
    };
    struct RegexElement* next;
} RegexElement;

static RegexElement* parseRegex(AvString regex, uint32* index, AvRegexError* error) {
    RegexElement* root = nullptr;
    RegexElement* element = nullptr;
    while (*index < regex.len) {
        char chr = regex.chrs[(*index)++];
        enum Quantifier quantifier = 0;
        switch (chr) {
        case '.':
            if (element == nullptr) {
                element = avCallocate(1, sizeof(RegexElement), "allocating regex element");
                root = element;
            } else {
                element->next = avCallocate(1, sizeof(RegexElement), "allocating regex element");
                element = element->next;
            }
            element->type = REGEX_TYPE_WILDCARD;
            element->quantifier = QUANTIFIER_EXACTLY_ONE;
            break;
        case '?':
            quantifier = QUANTIFIER_ZERO_OR_ONE;
            goto setQuantifier;
        case '*':
            quantifier = QUANTIFIER_ZERO_OR_MORE;
            goto setQuantifier;
        case '+':
            if (!element || element->quantifier != QUANTIFIER_EXACTLY_ONE) {
                avAssert(!element || element->quantifier != QUANTIFIER_EXACTLY_ONE, "error");
                *error = AV_REGEX_PARSE_INVALID_TOKEN;
                return root;
            }

            element->next = avCallocate(1, sizeof(RegexElement), "allocating regex element");
            memcpy(element->next, element, sizeof(RegexElement));
            element->next->next = nullptr;
            element = element->next;

            quantifier = QUANTIFIER_ZERO_OR_MORE;
            goto setQuantifier;
        setQuantifier:
            if (!element || element->quantifier != QUANTIFIER_EXACTLY_ONE) {
                avAssert(!element || element->quantifier != QUANTIFIER_EXACTLY_ONE, "error");
                *error = AV_REGEX_PARSE_INVALID_TOKEN;
                return root;
            }
            element->quantifier = quantifier;
            break;
        case '(':
            if (element == nullptr) {
                element = avCallocate(1, sizeof(RegexElement), "allocating regex element");
                root = element;
            } else {
                element->next = avCallocate(1, sizeof(RegexElement), "allocating regex element");
                element = element->next;
            }
            element->type = REGEX_TYPE_GROUP;
            element->quantifier = QUANTIFIER_EXACTLY_ONE;
            element->content = parseRegex(regex, index, error);
            if (*error != AV_REGEX_PARSE_SUCCES) {
                return root;
            }
            if (*index >= regex.len) {
                avAssert(*index >= regex.len, "unexpected end of regex");
                *error = AV_REGEX_PARSE_UNEXPECTED_END;
                return root;
            }
            chr = regex.chrs[(*index)++];
            if (chr != ')') {
                avAssert(*index >= regex.len, "expected ')'!");
                *error = AV_REGEX_PARSE_UNEXPECTED_END;
                return root;
            }
            break;
        case ')':
            (*index)--;
            return root;
        case '\\':
            if (*index == regex.len) {
                avAssert(0, "bad escape character");
                *error = AV_REGEX_PARSE_UNEXPECTED_END;
                return root;
            }
            if (element == nullptr) {
                element = avCallocate(1, sizeof(RegexElement), "allocating regex element");
                root = element;
            } else {
                element->next = avCallocate(1, sizeof(RegexElement), "allocating regex element");
                element = element->next;
            }
            element->type = REGEX_TYPE_LITERAL;
            element->quantifier = QUANTIFIER_EXACTLY_ONE;
            element->chr = regex.chrs[(*index)++];
            switch (element->chr) {
            case 'a':
                element->chr = '\a';
                break;
            case 'b':
                element->chr = '\b';
                break;
            case 'e':
                element->chr = '\e';
                break;
            case 'f':
                element->chr = '\f';
                break;
            case 'n':
                element->chr = '\n';
                break;
            case 'r':
                element->chr = '\r';
                break;
            case 't':
                element->chr = '\t';
                break;
            case 'v':
                element->chr = '\v';
                break;
            case '\\':
                element->chr = '\\';
                break;
                break;
            default:
                break;
            }
            break;
        default:
            if (element == nullptr) {
                element = avCallocate(1, sizeof(RegexElement), "allocating regex element");
                root = element;
            } else {
                element->next = avCallocate(1, sizeof(RegexElement), "allocating regex element");
                element = element->next;
            }
            element->type = REGEX_TYPE_LITERAL;
            element->quantifier = QUANTIFIER_EXACTLY_ONE;
            element->chr = chr;
            break;
        }
    }
    return root;
}

static void freeRegex(RegexElement* element) {
    if (element == nullptr) {
        return;
    }
    if (element->type == REGEX_TYPE_GROUP) {
        freeRegex(element->content);
    }
    freeRegex(element->next);
}



static AvRegexResult regexMatch(RegexElement* root, AvString str, uint32* index) {
    AvRegexResult result = AV_EMPTY;
    result.matched = true;

    RegexElement* element = root;
    while (element) {
        uint32 prevIndex = *index;
        if (*index == str.len) {
            result.matched = false;
            return result;
        }
        char chr = str.chrs[(*index)++];
        switch (element->type) {
        case REGEX_TYPE_LITERAL:
            if (chr == element->chr) {
                result.charCount++;
            } else {
                result.matched = false;
            }
            goto handleQuantifier;
        case REGEX_TYPE_WILDCARD:
            result.charCount++;
            goto handleQuantifier;
        case REGEX_TYPE_GROUP:
        {
            AvRegexResult res = regexMatch(element->content, str, index);
            if (!res.matched) {
                result.matched = false;
            }
            result.charCount += res.charCount;
        }
        goto handleQuantifier;
    handleQuantifier:;
        switch (element->quantifier) {
        case QUANTIFIER_EXACTLY_ONE:
            element = element->next;
            continue;
        case QUANTIFIER_ZERO_OR_MORE:
            if (result.matched) {
                continue;
            } else {
                element = element->next;
                *index = prevIndex;
                continue;
            }
        case QUANTIFIER_ZERO_OR_ONE:
            if (result.matched) {
                element = element->next;
                continue;
            } else {
                element = element->next;
                *index = prevIndex;
                continue;
            }
        }
        break;
        default:
            avAssert(0, "error, invalid regex element");
            result.matched = false;
            return result;
        }
    }
    return result;
}

AvRegexResult avRegexMatch(AvString regex, AvString str, AvRegexError* error) {
    AvRegexError parseError = 0;
    uint32 index = 0;
    RegexElement* element = parseRegex(regex, &index, &parseError);
    if (error) {
        *error = parseError;
        freeRegex(element);
        return (AvRegexResult) {
            .charCount = 0,
                .matched = false
        };
    }
    index = 0;
    AvRegexResult result = regexMatch(element, str, &index);

    freeRegex(element);
    return result;
}