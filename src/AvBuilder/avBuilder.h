#ifndef __AV_BUILDER__
#define __AV_BUILDER__

#include <AvUtils/avString.h>
#include <AvUtils/dataStructures/avDynamicArray.h>
#include <AvUtils/memory/avAllocator.h>
#include "avProjectLang.h"

#define TOKEN_TEXT(type, token, symbol)
#define LIST_OF_TOKENS \
    TOKEN(KEYWORD,      foreach,    "foreach")\
    TOKEN(KEYWORD,      command,    "command")\
    TOKEN(KEYWORD,      perform,    "perform")\
    TOKEN(KEYWORD,      import,     "import")\
    TOKEN(KEYWORD,      files,      "files")\
    TOKEN(KEYWORD,      from,       "from")\
    TOKEN(KEYWORD,      in,         "in")\
    TOKEN(KEYWORD,      as,         "as")\
    TOKEN(KEYWORD,      return,     "return")\
    TOKEN(KEYWORD,      recursive,  "recursive")\
    TOKEN(KEYWORD,      var,        "var")\
    \
    TOKEN(PUNCTUATOR,   bracket_open, "[")\
    TOKEN(PUNCTUATOR,   bracket_close, "]")\
    TOKEN(PUNCTUATOR,   star, "*")\
    TOKEN(PUNCTUATOR,   equals, "=")\
    TOKEN(PUNCTUATOR,   brace_open, "{")\
    TOKEN(PUNCTUATOR,   brace_close, "}")\
    TOKEN(PUNCTUATOR,   parenthese_open, "(")\
    TOKEN(PUNCTUATOR,   parenthese_close, ")")\
    TOKEN(PUNCTUATOR,   comma, ",")\
    TOKEN(PUNCTUATOR,   dot, ".")\
    TOKEN(PUNCTUATOR,   plus, "+")\
    TOKEN(PUNCTUATOR,   minus, "-")\
    TOKEN(PUNCTUATOR,   divide, "/")\
    TOKEN(PUNCTUATOR,   semicolon, ";")\
    TOKEN(PUNCTUATOR,   colon, ":")
#undef TOKEN


extern const AvString keywords[];
extern const uint32 keywordCount;

extern const AvString punctuators[];
extern const uint32 punctuatorCount;

#define TOKEN(type, token, symbol) TOKEN_TYPE_SPECIFIER_##type##_##token,
typedef enum TokenTypeSpecifiers {
    TOKEN_TYPE_SPECIFIER_NONE = 0,
    LIST_OF_TOKENS
}TokenTypeSpecifiers;
#undef TOKEN
#define TOKEN(type, token, symbol) TOKEN_TYPE_##type##_##token = (TOKEN_TYPE_SPECIFIER_##type##_##token<<6) | TOKEN_TYPE_##type,
typedef enum TokenType{
    TOKEN_TYPE_NONE             = 0,
    TOKEN_TYPE_KEYWORD          = 1<<0,
    TOKEN_TYPE_STRING           = 1<<1,
    TOKEN_TYPE_TEXT             = 1<<2,
    TOKEN_TYPE_PUNCTUATOR       = 1<<3,
    TOKEN_TYPE_NUMBER           = 1<<4,
    TOKEN_TYPE_SPECIAL_STRING   = 1<<5,
    LIST_OF_TOKENS
}TokenType;
#undef TOKEN

typedef struct Token {
    TokenType type;
    AvString str;
    uint32 character;
    uint32 line;
    AvString file;
} Token;


typedef enum ProcessState{
    PROCESS_STATE_OK,
    PROCESS_STATE_SCEMANTIC_ERROR,
}ProcessState;

typedef struct LocalContext {
    AV_DS(AvDynamicArray, struct VariableDescription) variables;
    struct LocalContext* previous;
    bool32 inherit;
} LocalContext;

struct ProjectOptions {
    AvString entry;
    bool32 commandDebug;
};
typedef struct Project {
    AvString name;
    AvString projectFileContent;
    AvString projectFileName;

    AvAllocator allocator;
    
    AV_DS(AvDynamicArray, struct FunctionDescription) functions;
    AV_DS(AvDynamicArray, struct VariableDescription) variables;
    AV_DS(AvDynamicArray, struct VariableDescription) constants;
    AV_DS(AvDynamicArray, struct ImportDescription) externals;
    AV_DS(AvDynamicArray, Project*) importedProjects;
    AV_DS(AvDynamicArray, struct ConstValue*) arrays;
    uint32 statementCount;
    struct Statement_S** statements;

    LocalContext* localContext;

    ProcessState processState;
    struct ProjectOptions options;
} Project;

extern const AvString configPath;
extern const AvString templatePath;


bool32 loadProjectFile(const AvString projectFilePath, AvStringRef projectFileContent, AvStringRef projectFileName);
bool32 tokenizeProject(const AvString projectFileContent, const AvString projectFileName, AvDynamicArray tokens);
bool32 parseProject(AV_DS(AvDynamicArray, Token) tokenList, void** statements, Project* project);
bool32 processProject(void* statements, Project* project);
bool32 runProject(Project* project, AvDynamicArray arguments);


void startLocalContext(struct Project* project, bool32 inherit);
void endLocalContext(struct Project* project);
void projectCreate(struct Project* project, AvString name, AvString file, AvString content);
void projectDestroy(struct Project* project);

#endif//__AV_BUILDER__ 