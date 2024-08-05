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

struct Primary {
    enum PrimaryType type;
    union {
        AvString literal;
        AvString number;
        struct Expression* grouping;
        AvString identifier;
    };
};

struct Array {
    struct Summation* expression;
    struct Array* next;
};

struct ArrayList{ 
    struct Array* array;
    struct ArrayList* next;
};
struct Filter{
    struct Call* call;
    struct ArrayList* filter;
};

struct Argument {
    struct Expression* expression;
    struct Argument* next;
};

struct Call{
    struct Primary* function;
    struct Argument* argument;
};

enum UnaryOperator {
    UNARY_OPERATOR_NONE = 0,
    UNARY_OPERATOR_MINUS,
};
enum UnaryType {
    UNARY_TYPE_NONE = 0,
    UNARY_TYPE_UNARY,
};

struct Unary{
    enum UnaryOperator operator;
    enum UnaryType type;
    union{
        struct Unary* unary;
        struct Filter* filter;
    };
};

enum EnumerationType {
    ENUMERATION_TYPE_NONE = 0,
    ENUMERATION_TYPE_ENUMERATION,
};
struct Enumeration {
    enum EnumerationType type;
    bool8 recursive;
    struct Unary* unary;
};

enum SummationOperator {
    SUMMATION_OPERATOR_NONE = 0,
    SUMMATION_OPERATOR_ADD,
    SUMMATION_OPERATOR_SUBTRACT,
};

struct Summation{
    struct Multiplication* left;
    enum SummationOperator operator;
    struct Summation* right;
};

enum MultiplicationOperator {
    MULTIPLICATION_OPERATOR_NONE = 0,
    MULTIPLICATION_OPERATOR_MULTIPLY,
    MULTIPLICATION_OPERATOR_DIVIDE,
};

struct Multiplication{
    struct Enumeration* left;
    enum MultiplicationOperator operator;
    struct Multiplication* right;
};

struct Grouping {
    struct Expression* expression;
};
struct Expression {
    struct Array* array;
};

enum VariableAccessModifier{
    VARIABLE_ACCESS_MODIFIER_NONE = 0,
    VARIABLE_ACCESS_MODIFIER_ARRAY,
};
struct Variable {
    AvString name;
    enum VariableAccessModifier modifier;
    struct Expression* index;
};

struct VariableAssignment {
    struct Variable* variable;
    struct Expression* expression;  
};

struct FunctionCallStatement {
    struct Call* call;
};

struct DefinitionMapping{
    AvString symbol;
    AvString symbolAlias;
};

struct DefinitionMappingList{
    struct DefinitionMapping* definitionMapping;
    struct DefinitionMappingList* next;
};

struct ImportStatement {
    AvString file;
    struct DefinitionMappingList* definitionMappingList;
};

enum CommandStatementType {
    COMMAND_STATEMENT_NONE = 0,
    COMMAND_STATEMENT_VARIABLE_ASSIGNMENT,
    COMMAND_STATEMENT_FUNCTION_CALL,
};
struct CommandStatement {
    enum CommandStatementType type;
    union {
        struct VariableAssignment* variableAssignment;
        struct FunctionCallStatement* functionCall;
    };
};

struct CommandStatementList {
    struct CommandStatement* commandStatement;
    struct CommandStatementList* next;
};

struct VariableDefinitionStatement {
    AvString identifier;
    struct Expression* size;
};

enum PerformOperationType {
    PERFORM_OPERATION_TYPE_NONE= 0,
    PERFORM_OPERATION_TYPE_COMMAND, 
    PERFORM_OPERATION_TYPE_VARIABLE_ASSIGNMENT,
    PERFORM_OPERATION_TYPE_FUNCTION_CALL,
    PERFORM_OPERATION_TYPE_VARIABLE_DEFINITION,
};
struct PerformOperation{
    enum PerformOperationType type;
    union {
        struct CommandStatementList* commandStatementList;
        struct VariableAssignment* variableAssignment;
        struct FunctionCallStatement* functionCall;
        struct VariableDefinitionStatement* varStatement;
    };
};

struct PerformOperationList {
    struct PerformOperation* performOperation;
    struct PerformOperationList* next;
};

struct PerformStatement {
    struct PerformOperationList* performOperationList;
};

struct ForeachStatement{
    AvString variable;
    struct Expression* collection;
    AvString index;
    struct PerformStatement* performStatement;
};

struct ReturnStatement{
    struct Expression* value;
};

struct Parameter{
    AvString name;
};

struct ParameterList{
    struct Parameter* parameter;
    struct ParameterList* next;
};

enum FunctionStatementType{
    FUNCTION_STATEMENT_TYPE_NONE = 0,
    FUNCTION_STATEMENT_TYPE_FOREACH,
    FUNCTION_STATEMENT_TYPE_PERFORM,
    FUNCTION_STATEMENT_TYPE_RETURN,
    FUNCTION_STATEMENT_TYPE_VAR_DEFINITION,
};
struct FunctionStatement{
    enum FunctionStatementType type;
    union {
        struct ForeachStatement* foreachStatement;
        struct PerformStatement* performStatement;
        struct ReturnStatement* returnStatement;
        struct VariableDefinitionStatement* varStatement;
    };
};

struct FunctionStatementList{
    struct FunctionStatement* functionStatement;
    struct FunctionStatementList* next;
};

struct FunctionDefinition{
    AvString name;
    struct ParameterList* parameterList;
    struct FunctionStatementList* functionStatementList;
};
enum ProjectStatementType {
    PROJECT_STATEMENT_TYPE_NONE=  0,
    PROJECT_STATEMENT_TYPE_INCLUDE,
    PROJECT_STATEMENT_TYPE_VARIABLE_ASSIGNMENT,
    PROJECT_STATEMENT_TYPE_FUNCTION_DEFINITION,
};
struct ProjectStatement{
    enum ProjectStatementType type;
    union {
        struct ImportStatement* importStatement;
        struct VariableAssignment* variableAssignment;
        struct FunctionDefinition* functionDefinition;
    };
};
struct ProjectStatementList{
    struct ProjectStatement* statement;
    struct ProjectStatementList* next;
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
};


struct UnaryExpression_S{
    enum UnaryOperator operator;
    struct Expression_S* expression;
};

struct FilterExpression_S{
    struct Expression_S* expression;
    uint32 filterCount;
    struct Expression_S* filters;
};

struct CallExpression_S{
    AvString function;
    uint32 argumentCount;
    struct Expression_S* arguments;
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

enum ExpressionType {
    EXPRESSION_TYPE_NONE = 0,
    EXPRESSION_TYPE_ARRAY,
    EXPRESSION_TYPE_SUMMATION,
    EXPRESSION_TYPE_MULTIPLICATION,
    EXPRESSION_TYPE_ENUMERATION,
    EXPRESSION_TYPE_UNARY,
    EXPRESSION_TYPE_FILTER,
    EXPRESSION_TYPE_CALL,
    EXPRESSION_TYPE_GROUPING,
    EXPRESSION_TYPE_IDENTIFIER,
    EXPRESSION_TYPE_LITERAL,
    EXPRESSION_TYPE_NUMBER,
};

struct Expression_S {
    enum ExpressionType type;
    union {
        struct ArrayExpression_S array;
        struct SummationExpression_S summation;
        struct MultiplicationExpression_S multiplication;
        struct EnumerationExpression_S enumeration;
        struct UnaryExpression_S unary;
        struct FilterExpression_S filter;
        struct CallExpression_S call;
        struct GroupExpression_S grouping;
        struct IdentifierExpression_S identifier;
        struct LiteralExpression_S literal;
        struct NumberExpression_S number;
    };
};

struct VariableAssignment_S {
    AvString variableName;
    enum VariableAccessModifier modifier;
    struct Expression_S* index;
    struct Expression_S* value;
};

struct CommandStatement_S{
    enum CommandStatementType type;
    union {
        struct VariableAssignment_S variableAssignment;
        struct CallExpression_S functionCall;
    };
};

struct CommandStatementBody_S{
    uint32 statementCount;
    struct CommandStatement_S* statements;
};

struct VariableDefinition_S{
    AvString identifier;
    struct Expression_S* size;
};

enum PerformStatementType {
    PERFORM_STATEMENT_TYPE_NONE = 0,
    PERFORM_STATEMENT_TYPE_COMMAND,
    PERFORM_STATEMENT_TYPE_VARIABLE_ASSIGNMENT,
    PERFORM_STATEMENT_TYPE_FUNCTION_CALL,
    PERFORM_STATEMENT_TYPE_VARIABLE_DEFINITION,
};

struct PerformStatement_S {
    enum PerformStatementType type;
    union{
        struct CommandStatementBody_S commandStatement;
        struct VariableAssignment_S variableAssignment;
        struct CallExpression_S functionCall;
        struct VariableDefinition_S variableDefinition;
    };
};

struct PerformStatementBody_S{
    uint32 statementCount;
    struct PerformStatement_S* statements;
};

struct ForeachStatement_S{
    AvString variable;
    struct Expression_S* collection;
    AvString index;
    struct PerformStatementBody_S performStatement;
};

struct ReturnStatement_S{
    struct Expression_S* value;
};


struct FunctionStatement_S{
    enum FunctionStatementType type;
    union{
        struct PerformStatementBody_S performStatement;
        struct ForeachStatement_S foreachStatement;
        struct ReturnStatement_S returnStatement;
        struct VariableDefinition_S variableDefinition;
    };
};


struct FunctionDefinition_S{
    AvString functionName;
    uint32 parameterCount;
    AvString* parameters;
    uint32 statementCount;
    struct FunctionStatement_S* statements;
};

struct ImportMapping_S{
    AvString symbol;
    AvString alias;
};

struct ImportStatement_S{
    AvString importFile;
    uint32 mappingCount;
    struct ImportMapping_S* mappings;
};

enum StatementType {
    STATEMENT_TYPE_NONE = 0,
    STATEMENT_TYPE_VARIABLE_ASSIGNMENT,
    STATEMENT_TYPE_FUNCTION_DEFINITION,
    STATEMENT_TYPE_IMPORT,
};

struct Statement_S{
    enum StatementType type;
    union{
        struct VariableAssignment_S variableAssignment;
        struct FunctionDefinition_S functionDefinition;
        struct ImportStatement_S importStatement;
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
        uint32 asNumber;
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
        uint32 asNumber;
        struct ArrayValue asArray;
    };
};

struct FunctionDescription {
    AvString identifier;
    uint32 statement;
    struct Project* project;
};

struct ImportDescription{
    AvString identifier;
    uint32 importLocation;
};

struct VariableDescription {
    AvString identifier;
    uint32 statement;
    struct Value* value;
    struct Project* project;
};


#endif//__AV_PROJECT_LANG__