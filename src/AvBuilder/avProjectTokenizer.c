#include "avBuilder.h"
#define AV_DYNAMIC_ARRAY_EXPOSE_MEMORY_LAYOUT
#include <AvUtils/dataStructures/avDynamicArray.h>
#include <AvUtils/dataStructures/avTable.h>
#include <AvUtils/memory/avAllocator.h>
#include <AvUtils/string/avChar.h>
#include <AvUtils/avMath.h>
#include <AvUtils/filesystem/avFile.h>
#include <AvUtils/avMemory.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

bool32 consumeToken(uint64* const readIndex, const AvString projectFileContent, const uint32 tokenCount, const AvString * const tokens){
    uint32 keywordLength = 0;
    uint32 longestKeyword = -1;
    for(uint32 i = 0; i < tokenCount; i++){
        const AvString keyword = tokens[i];
        const uint64 remainingLength = projectFileContent.len - *readIndex;
        if(!avStringEquals(keyword, AV_STR(projectFileContent.chrs+*readIndex, AV_MIN(keyword.len, remainingLength)))){
            continue;
        }
        if(keyword.len == remainingLength){
            (*readIndex)++;
            return true;
        }            
        if(remainingLength >= keyword.len && keyword.len != 1){
            char next = (projectFileContent.chrs[*readIndex + keyword.len]);
            if(avCharIsLetter(next)||avCharIsNumber(next)){
                continue;
            }
        }

        if(keyword.len > keywordLength){
            keywordLength = keyword.len;
            longestKeyword = i;
        }
    }
    if(longestKeyword == -1){
        return false;
    }
    *readIndex += keywordLength;
    return true;
}

void consumeComments(uint64* const readIndex, uint32* const lineIndex, const AvString projectFileContent){

    uint64 index = *readIndex;
    if(projectFileContent.chrs[index]=='#'){
        if(index > 0 && projectFileContent.chrs[index-1]!='\n'){
            return;
        }
        index++;
        for(; index < projectFileContent.len; index++){
            if(projectFileContent.chrs[index]=='\n'){
                break;
            }
        }
        (*readIndex) = index;
        return;
    }
    
    if(projectFileContent.chrs[index++]!='/'){
        return;
    }
    if(projectFileContent.len - index >= 1){
        if(projectFileContent.chrs[index]=='/'){
            index++;
            for(; index < projectFileContent.len; index++){
                if(projectFileContent.chrs[index]=='\n'){
                    break;
                }
            }
            (*readIndex) = index;
            return;
        }
        if(projectFileContent.chrs[index]=='*'){
            index++;
            for(; index < projectFileContent.len; index++){
                if(projectFileContent.chrs[index]=='\n'){
                    (*lineIndex)++;
                }
                if(projectFileContent.chrs[index]=='/' && projectFileContent.chrs[index-1]=='*'){
                    index++;
                    break;
                }
            }
            (*readIndex)= index;
            return;
        }

    }

}

bool32 consumeEnclosed(uint64* const readIndex, const AvString projectFileContent, char start, char end){
    if(projectFileContent.chrs[*readIndex]!=start){
        return false;
    }

    uint64 index = *readIndex + 1;
    for(; index < projectFileContent.len; index++){
        char chr = projectFileContent.chrs[index];

        if(chr == '\\'){
            index++;
            continue;
        }

        if(chr == end){
            index++;
            break;
        }

    }
    *readIndex = index;
    return true;
}

bool32 consumeString(uint64* const readIndex, const AvString projectFileContent){
    return consumeEnclosed(readIndex, projectFileContent, '"', '"');
}

bool32 consumeSpecialString(uint64* const readIndex, const AvString projectFileContent){
    return consumeEnclosed(readIndex, projectFileContent, '<', '>');
}


bool32 consumeNumber(uint64 * const readIndex, const AvString projectFileContent){
    enum NumberType {
        NUMBER_TYPE_DECIMAL,
        NUMBER_TYPE_HEXADECIMAL,
        NUMBER_TYPE_BINARY,
        NUMBER_TYPE_OCTAL,
    };
    enum NumberType type = NUMBER_TYPE_DECIMAL;
    uint64 index = *readIndex;
    if(projectFileContent.chrs[index]=='0'){
        index++;
        if(projectFileContent.len - *readIndex < 2){
            (*readIndex)++;
            return true;
        }
        
        char c = projectFileContent.chrs[index++];
        if(avCharIsNumber(c)){
            type = NUMBER_TYPE_OCTAL;
            goto numberTypeFound;
        }
        if(projectFileContent.len - *readIndex < 3){
            (*readIndex)++;
            return true;
        }
        if(c == 'x' || c=='X'){
            type = NUMBER_TYPE_HEXADECIMAL;
            goto numberTypeFound;
        }
        if(c=='b' || c=='B'){
            type = NUMBER_TYPE_BINARY;
            goto numberTypeFound;
        }
        
        (*readIndex)++;
        return true;
    }

    if(!avCharIsNumber(projectFileContent.chrs[*readIndex])){
        return false;
    }

numberTypeFound:

    for(; index < projectFileContent.len; index++){
        char c = projectFileContent.chrs[index];
        
        switch(type){
            case NUMBER_TYPE_DECIMAL:
                if(avCharIsNumber(c)){
                    continue;
                }
                break;
            break;
            case NUMBER_TYPE_HEXADECIMAL:
                if(avCharIsHexNumber(c)){
                    continue;
                }
            break;
            case NUMBER_TYPE_BINARY:
                if(avCharIsWithinRange(c, '0','1')){
                    continue;
                }
            break;
            case NUMBER_TYPE_OCTAL:
                if(avCharIsWithinRange(c, '0', '7')){
                    continue;
                }
        }
        break;
    }
    (*readIndex) = index;
    return true;
}

bool32 consumeText(uint64 * const readIndex, const AvString projectFileContent){

    uint64 index = *readIndex;

    for(; index < projectFileContent.len; index++){
        char chr = projectFileContent.chrs[index];

        if(avCharIsWhiteSpace(chr)){
            break;
        }

        bool32 isPunctuator = false;
        for(uint32 i = 0; i < punctuatorCount; i++){
            if(punctuators[i].chrs[0] == chr){
                isPunctuator = true;
                break;
            }
        }
        if(isPunctuator){
            break;
        }

        if(chr=='/'){
            if(projectFileContent.len-index>1 && projectFileContent.chrs[index+1] == '*'){
                break;
            }
            if(projectFileContent.len-index>1 && projectFileContent.chrs[index+1] == '/'){
                break;
            }
        }
        
        
    }

    *readIndex = index;
    return true;

}

__attribute__((unused))
static void printTokenList(AvDynamicArray tokens){
    avDynamicArrayForEachElement(Token, tokens, {
        avStringPrintf(AV_CSTR("line %i type %i %s\n"), element.line, element.type, element.str);

    });
}

bool32 tokenizeProject(const AvString projectFileContent, const AvString projectFileName, AvDynamicArray tokens){

    uint32 line = 1;

    uint64 readIndex = 0;
    uint64 tokenStart = 0;

    TokenType tokenType = TOKEN_TYPE_NONE;

    while(readIndex < projectFileContent.len){
        consumeComments(&readIndex, &line, projectFileContent);
        tokenStart = readIndex;
        
        if(avCharIsWhiteSpace(projectFileContent.chrs[readIndex])){
            if(avCharIsNewline(projectFileContent.chrs[readIndex])){
                line++;
            }
            readIndex++;
            continue;
        }
        if(consumeToken(&readIndex, projectFileContent, keywordCount, keywords)){
            tokenType = TOKEN_TYPE_KEYWORD;
            goto tokenFound;
        }
        if(consumeToken(&readIndex, projectFileContent, punctuatorCount, punctuators)){
            tokenType = TOKEN_TYPE_PUNCTUATOR;
            goto tokenFound;
        }

        if(consumeString(&readIndex, projectFileContent)){
            tokenType = TOKEN_TYPE_STRING;
            goto tokenFound;
        }

        if(consumeSpecialString(&readIndex, projectFileContent)){
            tokenType = TOKEN_TYPE_SPECIAL_STRING;
            goto tokenFound;
        }

        if(consumeNumber(&readIndex, projectFileContent)){
            tokenType = TOKEN_TYPE_NUMBER;
            goto tokenFound;
        }

        if(consumeText(&readIndex, projectFileContent)){
            tokenType = TOKEN_TYPE_TEXT;
            goto tokenFound;
        }

        readIndex++;
        continue;

    tokenFound:
        {
            uint64 tokenlength = readIndex - tokenStart;
            Token token = {
                .str = AV_STR(projectFileContent.chrs + tokenStart, tokenlength),
                .type = tokenType,
                .file = projectFileName,
                .line = line,
                .character = tokenStart,
            };

            #define TOKEN(tokenType, tokenName, symbol) \
            if(avStringEquals(token.str, AV_CSTR(symbol))) { \
                 token.type = TOKEN_TYPE_##tokenType##_##tokenName;\
            }
            LIST_OF_TOKENS
            #undef TOKEN

            avDynamicArrayAdd(&token, tokens);
        }
    }

    //printTokenList(tokens);
    return true;
}




