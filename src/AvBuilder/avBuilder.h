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
    TOKEN(KEYWORD,      global,     "global")\
    TOKEN(KEYWORD,      perform,    "perform")\
    TOKEN(KEYWORD,      import,     "import")\
    TOKEN(KEYWORD,      inherit,    "inherit")\
    TOKEN(KEYWORD,      provide,    "provide")\
    TOKEN(KEYWORD,      files,      "files")\
    TOKEN(KEYWORD,      if,         "if")\
    TOKEN(KEYWORD,      else,       "else")\
    TOKEN(KEYWORD,      from,       "from")\
    TOKEN(KEYWORD,      in,         "in")\
    TOKEN(KEYWORD,      as,         "as")\
    TOKEN(KEYWORD,      return,     "return")\
    TOKEN(KEYWORD,      recursive,  "recursive")\
    TOKEN(KEYWORD,      var,        "var")\
    TOKEN(KEYWORD,      func,       "func")\
    TOKEN(KEYWORD,      directories,"directories")\
    TOKEN(KEYWORD,      break,      "break")\
    TOKEN(KEYWORD,      continue,   "continue")\
    \
    TOKEN(PUNCTUATOR,   less_than_or_equal, "<=")\
    TOKEN(PUNCTUATOR,   greater_than_or_equal, ">=")\
    TOKEN(PUNCTUATOR,   comparison, "==")\
    TOKEN(PUNCTUATOR,   not_equals, "!=")\
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
    TOKEN(PUNCTUATOR,   not, "!")\
    TOKEN(PUNCTUATOR,   minus, "-")\
    TOKEN(PUNCTUATOR,   divide, "/")\
    TOKEN(PUNCTUATOR,   semicolon, ";")\
    TOKEN(PUNCTUATOR,   colon, ":")\
    TOKEN(PUNCTUATOR,   question, "?")\
    TOKEN(PUNCTUATOR,   pipe, "|")\
    TOKEN(PUNCTUATOR,   error_pipe, "~")\
    TOKEN(PUNCTUATOR,   less_than, "<")\
    TOKEN(PUNCTUATOR,   greater_than, ">")\
    TOKEN(PUNCTUATOR,   and, "&&")\
    TOKEN(PUNCTUATOR,   or, "||")\
    TOKEN(PUNCTUATOR,   increment_assign, "+=")\
    TOKEN(PUNCTUATOR,   decrement_assign, "-=")\
    TOKEN(PUNCTUATOR,   multiply_assign, "*=")\
    TOKEN(PUNCTUATOR,   divide_assign, "/=")\
    TOKEN(PUNCTUATOR,   hash, "#")
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
	bool32 genCompileCommands;
};

// struct PartialImport {
//     Project* project;
// };

typedef struct Function {
    union{
        struct Statement_S* definition;
        const struct BuiltInFunctionDescription* builtin; 
    }; 
} Function;

typedef struct Variable {
    struct Value constValue;
} Variable;

enum SymbolType {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    //SYMBOL_PARAMETER,
    // SYMBOL_BUILTIN_FUNCTION,
    // SYMBOL_BUILTIN_VARIABLE,
};

enum ScopeType{
    SCOPE_TYPE_TOPLEVEL,
    SCOPE_TYPE_FUNCTION,
    SCOPE_TYPE_FOREACH,
    SCOPE_TYPE_BLOCK,
    SCOPE_TYPE_IF,
    SCOPE_TYPE_ELSE,
};

typedef struct Scope {
    AvAllocator allocator;
    AvDynamicArray symbols;
    struct Scope* parent;
    struct Project* project;
    enum ScopeType type;
} Scope;

typedef struct StackFrame {
    struct Scope* scope;
    struct StackFrame* parent;
    AvAllocator allocator;

    struct Value* values;
    uint32 valueCount;
} StackFrame;

typedef struct Symbol {
    AvString identifier;
    enum SymbolType type;
    Scope* scope;
    bool8 constant;
    bool8 constValue;
    bool8 builtin;
    bool8 external;
    uint32 localIndex;
    union{
        struct Function function;
        struct Variable variable;
    };
} Symbol;

typedef struct Alias {
    AvString identifier;
    AvString alias;
} Alias;

typedef struct ProjectImportDescription{
    AvString file;
    AvDynamicArray importAliasses;
} ProjectImportDescription;

enum ControlFlowStatus{
    CONTROLFLOW_NORMAL,
    CONTROLFLOW_RETURN,
    CONTROLFLOW_BREAK,
    CONTROLFLOW_CONTINUE,
};

typedef struct Project {
    AvString name;
    AvString projectFileContent;
    AvString projectFileName;

    AvAllocator baseAllocator;
    AvAllocator* allocator;
    
    AV_DS(AvDynamicArray, struct ImportDescription) libraryAliases;
    AV_DS(AvDynamicArray, struct Project*) importedProjects;

    uint32 statementCount;
    struct Statement_S* statements;

    LocalContext* localContext;
    bool32 isLocal;

    uint64 ID;

    ProcessState processState;
    struct ProjectOptions options;

    Scope* toplevelScope;
    Scope* currentScope;
    bool32 skipScope;
    enum ControlFlowStatus controlFlow;

    StackFrame* currentStackFrame;
    //struct FunctionDefinition_S* currentFunction;

    struct Project* parent;

} Project;

extern const AvString configPath;
extern const AvString templatePath;


bool32 loadProjectFile(const AvString projectFilePath, AvStringRef projectFileContent, AvStringRef projectFileName);
bool32 tokenizeProject(const AvString projectFileContent, const AvString projectFileName, AvDynamicArray tokens);
bool32 parseProject(AV_DS(AvDynamicArray, Token) tokenList, Project* project);
bool32 processProject(Project* project);
bool32 runProject(Project* project, AvDynamicArray arguments);


void startLocalContext(struct Project* project, bool32 inherit);
void endLocalContext(struct Project* project);
void projectCreate(struct Project* project, AvString name, AvString file, AvString content, bool32 isLocal);
void projectDestroy(struct Project* project);

void enterScope(enum ScopeType type, struct Statement_S* statement, Project* ctx);
void exitScope(Project* ctx);

Symbol* resolveSymbol(AvString identifier, int32* depth, Project* ctx);
#define LOC __FILE__, __LINE__
#define LOC_PARAM const char* file, uint32 line
#define LOC_PASS file, line

#define destroyValue(valuePtr, ...) destroyValue_(valuePtr __VA_OPT__(,) __VA_ARGS__, LOC)
#define destroyConstValue(valuePtr, ...) destroyConstValue_(valuePtr __VA_OPT__(,) __VA_ARGS__, LOC)

#define cloneValue(dstPtr, src, ...) cloneValue_(dstPtr, src __VA_OPT__(,) __VA_ARGS__, LOC)
#define cloneConstValue(dstPtr, src, ...) cloneConstValue_(dstPtr, src __VA_OPT__(,) __VA_ARGS__, LOC)

void destroyValue_(Value* value, LOC_PARAM);
void destroyConstValue_(ConstValue* value, LOC_PARAM);
void cloneValue_(Value* dst, Value src, LOC_PARAM);
void cloneConstValue_(ConstValue* dst, ConstValue src, LOC_PARAM);

#endif//__AV_BUILDER__ 
