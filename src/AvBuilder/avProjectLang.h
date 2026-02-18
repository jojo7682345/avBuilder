#ifndef __AV_PROJECT_LANG__
#define __AV_PROJECT_LANG__

#include <AvUtils/avString.h>

enum PrimaryType {
    PRIMARY_TYPE_NONE = 0,
    PRIMARY_TYPE_LITERAL,
    PRIMARY_TYPE_NUMBER,
    PRIMARY_TYPE_GROUPING,
    PRIMARY_TYPE_IDENTIFIER,
};

enum ComparisonOperator {
    COMPARISON_OPERATOR_NONE = 0,
    COMPARISON_OPERATOR_EQUALS,
    COMPARISON_OPERATOR_NOT_EQUALS,
    COMPARISON_OPERATOR_LESS_THAN,
    COMPARISON_OPERATOR_LESS_THAN_OR_EQUAL,
    COMPARISON_OPERATOR_GREATER_THAN,
    COMPARISON_OPERATOR_GREATER_THAN_OR_EQUAL,
};

enum CombinationOperator{
    COMBINATION_OPERATOR_NONE = 0,
    COMBINATION_OPERATOR_AND,
    COMBINATION_OPERATOR_OR,
};

enum UnaryOperator {
    UNARY_OPERATOR_NONE = 0,
    UNARY_OPERATOR_MINUS,
    UNARY_OPERATOR_NOT,
    UNARY_OPERATOR_PLUS,
};

enum SummationOperator {
    SUMMATION_OPERATOR_NONE = 0,
    SUMMATION_OPERATOR_ADD,
    SUMMATION_OPERATOR_SUBTRACT,
};

enum MultiplicationOperator {
    MULTIPLICATION_OPERATOR_NONE = 0,
    MULTIPLICATION_OPERATOR_MULTIPLY,
    MULTIPLICATION_OPERATOR_DIVIDE,
};

enum DefinitionMappingType {
    DEFINITION_MAPPING_DEFAULT          = 0b00,
    DEFINITION_MAPPING_PROVIDE          = 0b10,
    DEFINITION_MAPPING_GLOBAL           = 0b01,
};

struct ArrayExpression_S {
    uint32 length;
    struct Expression_S* elements;
};

struct SummationExpression_S{
    struct Expression_S* left;
    enum SummationOperator operator;
    struct Expression_S* right;
};

struct MultiplicationExpression_S{
    struct Expression_S* left;
    enum MultiplicationOperator operator;
    struct Expression_S* right;
};

struct EnumerationExpression_S{
    struct Expression_S* directory;
    bool8 recursive;
    bool8 dirs;
};

struct UnaryExpression_S{
    enum UnaryOperator operator;
    struct Expression_S* expression;
};

struct IndexExpression_S{
    struct Expression_S* expression;
    struct Expression_S* index;
};

struct CallExpression_S{
    AvString function;
    uint32 argumentCount;
    struct Expression_S* arguments;
};

struct ComparisonExpression_S{
    struct Expression_S* left;
    enum ComparisonOperator operator;
    struct Expression_S* right;
};

struct CombinationExpression_S{
    struct Expression_S* left;
    enum CombinationOperator operator;
    struct Expression_S* right;
};

struct GroupExpression_S {
    struct Expression_S* expression;
};

struct IdentifierExpression_S{
    AvString identifier;
};

struct LiteralExpression_S{
    AvString value;
};

struct NumberExpression_S{
    AvString value;
};

enum CommandOutputType{
    COMMAND_OUTPUT_TYPE_NONE,
    COMMAND_OUTPUT_TYPE_RETCODE,
    COMMAND_OUTPUT_TYPE_LINES,
};

struct CommandExpression_S{
    enum CommandOutputType outputType;
    struct Expression_S* command;
    struct Expression_S* pipeOutput;
};

enum AssignmentOperator{
    ASSIGNMENT_OPERATOR_NONE,
    ASSIGNMENT_OPERATOR_ASSIGN,
    ASSIGNMENT_OPERATOR_INCREMENT_ASSIGN,
    ASSIGNMENT_OPERATOR_DECREMENT_ASSIGN,
    ASSIGNMENT_OPERATOR_MULTIPLY_ASSIGN,
    ASSIGNMENT_OPERATOR_DIVIDE_ASSIGN,
};

struct AssignmentExpression_S{
    struct Expression_S* variable;
    enum AssignmentOperator operator;
    struct Expression_S* value;
};

enum ExpressionType {
    EXPRESSION_TYPE_NONE = 0,
    EXPRESSION_TYPE_ARRAY,
    EXPRESSION_TYPE_SUMMATION,
    EXPRESSION_TYPE_MULTIPLICATION,
    EXPRESSION_TYPE_ENUMERATION,
    EXPRESSION_TYPE_UNARY,
    EXPRESSION_TYPE_INDEX,
    EXPRESSION_TYPE_CALL,
    EXPRESSION_TYPE_IDENTIFIER,
    EXPRESSION_TYPE_LITERAL,
    EXPRESSION_TYPE_NUMBER,
    EXPRESSION_TYPE_COMPARISON,
    EXPRESSION_TYPE_COMBINATION,
    EXPRESSION_TYPE_COMMAND,
    EXPRESSION_TYPE_ASSIGNMENT,
};

struct Expression_S {
    enum ExpressionType type;
    union {
        struct ArrayExpression_S array;
        struct SummationExpression_S summation;
        struct MultiplicationExpression_S multiplication;
        struct EnumerationExpression_S enumeration;
        struct UnaryExpression_S unary;
        struct IndexExpression_S index;
        struct CallExpression_S call;
        struct IdentifierExpression_S identifier;
        struct LiteralExpression_S literal;
        struct NumberExpression_S number;
        struct ComparisonExpression_S comparison;
        struct CombinationExpression_S combination;
        struct AssignmentExpression_S assignment;
        struct CommandExpression_S command;
    };
};

struct VariableDefinition_S{
    AvString identifier;
    struct Expression_S size;
    struct Expression_S initialValue;
};

struct ForeachStatement_S{
    AvString variable;
    struct Expression_S* collection;
    AvString index;
    struct Statement_S* statement;
};

struct ReturnStatement_S{
    struct Expression_S* value;
};

struct IfStatement_S{
    struct Expression_S* check;
    struct Statement_S* branch;
    struct Statement_S* alternativeBranch;
};

struct BlockStatement_S{
    uint32 statementCount;
    struct Statement_S* statements;
};

struct FunctionParameter_S {
    AvString name;
    bool8 unknownSize;
    struct Expression_S size;
};
struct FunctionDefinition_S{
    AvString functionName;
    uint32 parameterCount;
    struct FunctionParameter_S* parameters;
    struct Statement_S* body;
};

struct ImportMapping_S{
    enum DefinitionMappingType type;
    AvString symbol;
    AvString alias;
};

struct ImportStatement_S{
    AvString importFile;
    bool32 local;
    uint32 mappingCount;
    struct ImportMapping_S* mappings;
};

struct InheritStatement_S{
    AvString variable;
    struct Expression_S* defaultValue;
};

enum StatementType {
    STATEMENT_TYPE_NONE = 0,
    STATEMENT_TYPE_FUNCTION_DEFINITION,
    STATEMENT_TYPE_IMPORT,
    STATEMENT_TYPE_INHERIT,
    STATEMENT_TYPE_FOREACH,
    STATEMENT_TYPE_RETURN,
    STATEMENT_TYPE_VARIABLE_DEFINITION,
    STATEMENT_TYPE_IF,
    STATEMENT_TYPE_EXPRESSION,
    STATEMENT_TYPE_BLOCK,
    STATEMENT_TYPE_CONTINUE,
    STATEMENT_TYPE_BREAK,
};

struct Statement_S{
    enum StatementType type;
    uint32 line;
    union{
        struct FunctionDefinition_S functionDefinition;
        struct ImportStatement_S importStatement;
        struct InheritStatement_S inheritStatement;
        struct ForeachStatement_S foreachStatement;
        struct ReturnStatement_S returnStatement;
        struct VariableDefinition_S variableDefinition;
        struct IfStatement_S ifStatement;
        struct Expression_S expression;
        struct BlockStatement_S block;
    };
};

enum ValueType {
    VALUE_TYPE_NONE = 0,
    VALUE_TYPE_STRING = 1<<0,
    VALUE_TYPE_NUMBER = 1<<1,
    VALUE_TYPE_ARRAY = 1<<2,
};
struct ConstValue {
    enum ValueType type;
    union {
        AvString asString;
        int64 asNumber;
    };
};

struct ArrayValue {
    uint32 count;
    struct ConstValue* values;
};

struct Value {
    enum ValueType type;
    union {
        AvString asString;
        int64 asNumber;
        struct ArrayValue asArray;
    };
};



#endif//__AV_PROJECT_LANG__