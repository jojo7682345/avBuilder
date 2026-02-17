#include "avBuilder.h"
#include <AvUtils/avMemory.h>
#include <AvUtils/logging/avAssert.h>
#include <AvUtils/dataStructures/avFMap.h>
#include <AvUtils/filesystem/avDirectory.h>
#include <AvUtils/string/avChar.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <AvUtils/avProcess.h>
#include <AvUtils/avEnvironment.h>
#include <AvUtils/process/avPipe.h>

#define AV_DYNAMIC_ARRAY_EXPOSE_MEMORY_LAYOUT
#include <AvUtils/dataStructures/avDynamicArray.h>

#include "avProjectLang.h"
#include "builtIn/avBuilderBuiltIn.h"
#include "compileCommands/compileCommands.h"

#define NULL_VALUE (struct Value){0}

void printValue(struct Value value);

void runtimeError(Project* project, const char* message, ...){
	va_list args;
	va_start(args, message);

	avStringPrintf(AV_CSTR("Runtime Error in project %S:\n\t"), project->name);
	avStringPrintfVA(AV_CSTR(message), args);

	avStringPrintf(AV_CSTR("\nVariables: [\n"));

	LocalContext* context = project->localContext;

	while(context){
		if(avDynamicArrayGetSize(context->variables) > 0){
			for(uint32 index = 0; index < avDynamicArrayGetSize(context->variables); index++) { 
				struct VariableDescription element; avDynamicArrayRead(&element, index, (context->variables)); { 
					struct VariableDescription var = element; 
					avStringPrintf(AV_CSTR("\t%S = "), var.identifier); 
					if(var.value){
						printValue(*var.value); 
					}else{
						avStringPrint(AV_CSTR("NULL"));
					}
					avStringPrint(AV_CSTR("\n")); 
				} 
			}
			if(context->previous){
				avStringPrint(AV_CSTR("], [\n"));
			}
		}
		if(!context->inherit){
			break;
		}
		context = context->previous;
	}
	avStringPrintf(AV_CSTR("]\n"));

	avStringPrintf(AV_CSTR("Globals: [\n"));
	for(uint32 index = 0; index < avDynamicArrayGetSize(project->variables); index++) {
		 struct VariableDescription element; 
		 avDynamicArrayRead(&element, index, (project->variables)); 
		 { 
			struct VariableDescription var = element; 
			avStringPrintf(((AvString){
				.chrs="\t%S = ", 
				.len=avCStringLength("\t%S = "), 
				.memory=((AvStringMemory*)0)
			}), var.identifier); 
			if(var.value){
				printValue(*var.value); 
			}
			avStringPrint(((AvString){
				.chrs="\n", .len=avCStringLength("\n"), .memory=((AvStringMemory*)0)
			})); 
		} 
	};
	avStringPrintf(AV_CSTR("]\n"));

	avStringPrintf(AV_CSTR("Constants: [\n"));
	avDynamicArrayForEachElement(struct VariableDescription, project->constants, {
		struct VariableDescription var = element;
		avStringPrintf(AV_CSTR("\t%S = "), var.identifier);
		printValue(*var.value);
		avStringPrint(AV_CSTR("\n"));
	});
	avStringPrintf(AV_CSTR("]\n"));

	avStringPrintf(AV_CSTR("Externals: [\n"));
	avDynamicArrayForEachElement(struct VariableDescription, project->externals, {
		struct VariableDescription var = element;
		avStringPrintf(AV_CSTR("\t%S\n"), var.identifier);
	});
	avStringPrintf(AV_CSTR("]\n"));

	avAssert(false, "runtime error");
}

void printConstValue(struct ConstValue value){
	if(value.type == VALUE_TYPE_NUMBER){
		avStringPrintf(AV_CSTR("%i"), value.asNumber);
		return;
	}
	if(value.type == VALUE_TYPE_STRING){
		avStringPrint(value.asString);
	}
}

void printValue(struct Value value){
	if(value.type == VALUE_TYPE_NUMBER){
		avStringPrintf(AV_CSTR("%i"), value.asNumber);
		return;
	}
	if(value.type == VALUE_TYPE_STRING){
		avStringPrint(value.asString);
		return;
	}
	if(value.type == VALUE_TYPE_ARRAY){
		avStringPrintln(AV_CSTR("["));
		struct ArrayValue arr = value.asArray;
		for(uint32 i = 0; i < arr.count; i++){
			avStringPrint(AV_CSTR("\t"));
			printConstValue(arr.values[i]);
			avStringPrint(AV_CSTR(",\n"));
		}
		avStringPrint(AV_CSTR("]"));
	}
}

static int32 parseNumber(AvString string){
	

	enum NumberType {
		NUMBER_TYPE_DECIMAL,
		NUMBER_TYPE_HEXADECIMAL,
		NUMBER_TYPE_BINARY,
		NUMBER_TYPE_OCTAL,
	};
	enum NumberType type = NUMBER_TYPE_DECIMAL;
	uint32 offset = 0;
	if(string.len > 2 && string.chrs[0] == '0'){
		switch(string.chrs[1]){
			case 'x':
			case 'X':
				type = NUMBER_TYPE_HEXADECIMAL;
				offset = 2;
				if(string.len < 3){
					avAssert(false, "invalid number");
					return -1;
				}
			break;
			case 'b':
			case 'B':
				type = NUMBER_TYPE_BINARY;
				offset = 2;
				if(string.len < 3){
					avAssert(false, "invalid number");
					return -1;
				}
			break;
			default:
				if(avCharIsNumber(string.chrs[1])){
					type = NUMBER_TYPE_OCTAL;
					offset = 1;
				}
			break;
		}
	}

	uint32 length = string.len - offset;
	const char* chr = string.chrs+offset;
	uint32 value = 0;

	for(uint32 i = 0; length != 0; length--){
		switch(type){
			case NUMBER_TYPE_DECIMAL:
				value *= 10;
				value += chr[i]-'0';
			break;
			case NUMBER_TYPE_HEXADECIMAL:
				value *= 16;
				value += (avCharIsNumber(chr[i]))?(chr[i]-'0'):(
					(avCharToUppercase(chr[i])-'A')+10
				);
			break;
			case NUMBER_TYPE_BINARY:
				value <<= 1;
				value |= chr[i]-'0';
			break;
			case NUMBER_TYPE_OCTAL:
				value *= 8;
				value += chr[i]-'0';
			break;
		}

		i++;
	}

	return value;
}

struct Value getValue(struct Expression_S* expression, Project* project);

void toConstValue(struct Value value, struct ConstValue* val, Project* project){
	val->type = value.type;
	switch(value.type){
		case VALUE_TYPE_ARRAY:
			runtimeError( project,"nested arrays are not allowed");
			return;
		case VALUE_TYPE_STRING:
			memcpy(&val->asString, &value.asString, sizeof(value.asString));
		break;
		case VALUE_TYPE_NUMBER:
			memcpy(&val->asNumber, &value.asNumber, sizeof(value.asNumber));
		break;
		case VALUE_TYPE_NONE:
			avAssert(false,"logic error");
		break;
	}
}

void toValue(struct ConstValue value, struct Value* val){
	val->type = value.type;
	switch(value.type){
		case VALUE_TYPE_STRING:
			memcpy(&val->asString, &value.asString, sizeof(value.asString));
		break;
		case VALUE_TYPE_NUMBER:
			memcpy(&val->asNumber, &value.asNumber, sizeof(value.asNumber));
		break;
		case VALUE_TYPE_ARRAY:
		case VALUE_TYPE_NONE:
			avAssert(false,"logic error");
		break;
	}
}

Symbol* findSymbol(AvString identifier, Project* project){
	return NULL;
}

void enterScope(Project* project){
	Scope* scope = avAllocatorAllocate(sizeof(Scope), project->allocator);
	avAllocatorCreate(0, AV_ALLOCATOR_TYPE_DYNAMIC, &scope->allocator);
	project->allocator = &scope->allocator;
	scope->parent = project->currentScope;
	project->currentScope = scope;
	avDynamicArrayCreate(0, sizeof(Symbol), &scope->symbols);
}

void exitScope(Project* project){
	avAssert(project->currentScope!=NULL, "scope inbalance");
	Scope* scope = project->currentScope;
	project->currentScope = scope->parent;
	project->allocator = &project->currentScope->allocator;
	avAllocatorDestroy(&scope->allocator);
	avDynamicArrayDestroy(scope->symbols);
}

void defineVariable(AvString identifier, Project* project){
	if(findSymbol(identifier, project)){
		runtimeError(project, "%S already defined", identifier);
		return;
	}
	Symbol symbol = {
		.type= SYMBOL_TYPE_VARIABLE, 
		.identifier = identifier, 
	};
	avDynamicArrayAdd(&symbol, project->currentScope->symbols);
}

void assignVariable(AvString identifier, struct Value value, Project* project){
	Symbol* symbol = findSymbol(identifier, project);
	if(!symbol){
		runtimeError(project, "unable to find %S", identifier);
		return;
	}
	if(symbol->type != SYMBOL_TYPE_VARIABLE){
		const char* type = "UNDEFINED";
		switch(symbol->type){
			case SYMBOL_TYPE_CONSTANT:
				type = "constant";
				break;
			case SYMBOL_TYPE_FUNCTION:
				type = "function";
				break;
			default:
				break;
		}
		runtimeError(project, "%S is already defined as %s", identifier, type);
		return;
	}

	avMemcpy(&symbol->variable.value, &value, sizeof(struct Value));
}

void assignConstant(AvString identifier, struct Value value, Project* project){
	if(findSymbol(identifier, project)){
		runtimeError(project, "%S already defined", identifier);
		return;
	}
	Symbol symbol = {
		.type= SYMBOL_TYPE_CONSTANT, 
		.identifier = identifier, 
		.variable = {
			.value = value,
		},
	};
	avDynamicArrayAdd(&symbol, project->currentScope->symbols);
}

uint32 runProject(Project* project, AvDynamicArray arguments){
	
	enterScope(project);

	for(uint32 i = 0; i < builtInVariableCount; i++){
		struct BuiltInVariableDescription var = builtInVariables[i];
		assignConstant(var.identifier, var.value, project);
	}
	assignConstant(AV_CSTR("PROJECT_NAME"), (struct Value){.type=VALUE_TYPE_STRING,.asString=project->name}, project);
	assignConstant(AV_CSTR("PROJECT_DIR"), currentDir(project, 0, nullptr), project);
	assignConstant(AV_CSTR("PLATFORM"), (struct Value){.type=VALUE_TYPE_STRING,.asString=AV_CSTR(
#ifdef _WIN32
		"WINDOWS"
#else
		"LINUX"
#endif
	)}, project);

	for(uint32 i = 0; i < project->statementCount; i++){
		struct Statement_S statement = (project->statements)[i];
		switch(statement.type){
			default:
				return -1;
				break;
		}

	}



	exitScope(project);


	// for(uint32 i = 0; i < project->statementCount; i++){
	// 	struct Statement_S statement = (project->statements)[i];
	// 	switch(statement.type){
	// 		case STATEMENT_TYPE_IMPORT:
	// 		case STATEMENT_TYPE_FUNCTION_DEFINITION:
	// 			break;
	// 		case STATEMENT_TYPE_INHERIT:
	// 			performInherit(statement.inheritStatement, i , project, project);
	// 			break;
	// 		case STATEMENT_TYPE_VARIABLE_ASSIGNMENT:
	// 			runVariableAssignment(statement.variableAssignment, i, project);
	// 			break;
	// 		case STATEMENT_TYPE_NONE:
	// 			return -1;
	// 			break;
	// 	}

	// }
	// AvString* entry = &project->name;
	// if(project->options.entry.len > 0 && project->options.entry.chrs){
	// 	entry = &project->options.entry;
	// }

	// struct FunctionDescription mainFunction = findFunction(*entry, project);
	// if(!mainFunction.project){
	// 	runtimeError( project,"no entry found");
	// }
	// avAssert(mainFunction.statement < project->statementCount, "invalid function location");
	// struct Statement_S* functionStatement = project->statements[mainFunction.statement];
	// avAssert(functionStatement->type == STATEMENT_TYPE_FUNCTION_DEFINITION, "malformed entry");
	
	// struct FunctionDefinition_S function = functionStatement->functionDefinition;

	// uint32 argumentCount = avDynamicArrayGetSize(arguments);
	// uint32 parameterCount = 0;
	// uint32 argumentSizes[function.parameterCount];


	// for(uint32 i = 0; i < function.parameterCount; i++){
	// 	if(function.parameters[i].unknownSize){
	// 		if(i != function.parameterCount - 1){
	// 			runtimeError(project, "Variable array (%S) can only be last parameter", function.parameters[i].name);
	// 			printHelp(function, project);
	// 			return -1;
	// 		}
	// 		if(parameterCount <= argumentCount){
	// 			parameterCount = argumentCount;
	// 		}
	// 		argumentSizes[i] = 0;
	// 	}else if(function.parameters[i].size){

	// 		struct Value size = getValue(function.parameters[i].size, project);
	// 		if(size.type != VALUE_TYPE_NUMBER){
	// 			runtimeError(project, "Size specified for array parameter is not a number");
	// 			return -1;
	// 		}
	// 		if(size.asNumber <= 0){
	// 			runtimeError(project, "Cannot have an array of negative or zero size");
	// 			return -1;
	// 		}
	// 		parameterCount += size.asNumber;
	// 		argumentSizes[i] = size.asNumber;
	// 	}else{
	// 		parameterCount++;
	// 		argumentSizes[i] = 1;
	// 	}
	// }


	// if(argumentCount != parameterCount){
	// 	printHelp(function, project);
	// 	return -1;
	// }
	
	// startLocalContext(project, false);
	// uint32 parameterIndex = 0;
	// for(uint32 i = 0; i < function.parameterCount; i++){
	// 	struct FunctionParameter_S param = function.parameters[i];
	// 	if((!param.size && !param.unknownSize) || argumentSizes[i] == 1){
	// 		AvString strValue = AV_EMPTY;
	// 		avDynamicArrayRead(&strValue, i, arguments);
	// 		parameterIndex++;
	// 		struct Value value = {
	// 			.type = VALUE_TYPE_STRING,
	// 			.asString = strValue,
	// 		};
	// 		struct VariableDescription variable = {
	// 			.identifier = function.parameters[i].name,
	// 			.project = project,
	// 			.statement = mainFunction.statement,
	// 		};
	// 		assignVariable(variable, value, project);
	// 	}else if(param.size && !param.unknownSize){
	// 		struct ConstValue* values = avAllocatorAllocate(sizeof(struct ConstValue)*argumentSizes[i], &project->allocator);
	// 		for(uint32 j = 0; j < argumentSizes[i]; j++){
	// 			avDynamicArrayRead(&values[j].asString, parameterIndex, arguments);
	// 			parameterIndex++;
	// 			values[j].type = VALUE_TYPE_STRING;
	// 		}
	// 		struct Value value = {
	// 			.type = VALUE_TYPE_ARRAY,
	// 			.asArray.count = argumentSizes[i],
	// 			.asArray.values = values,
	// 		};
	// 		struct VariableDescription variable = {
	// 			.identifier = function.parameters[i].name,
	// 			.project = project,
	// 			.statement = mainFunction.statement,
	// 		};
	// 		assignVariable(variable, value, project);
	// 	}else if(param.size==0 && param.unknownSize){
	// 		argumentSizes[i] = argumentCount - parameterIndex;
	// 		struct ConstValue* values = NULL;
	// 		if(argumentSizes[i] != 0){
	// 			values = avAllocatorAllocate(sizeof(struct ConstValue)*argumentSizes[i], &project->allocator);
	// 			for(uint32 j = 0; j < argumentSizes[i]; j++){
	// 				avDynamicArrayRead(&values[j].asString, parameterIndex, arguments);
	// 				parameterIndex++;
	// 				values[j].type = VALUE_TYPE_STRING;
	// 			}
	// 		}
	// 		struct Value value = {
	// 			.type = VALUE_TYPE_ARRAY,
	// 			.asArray.count = argumentSizes[i],
	// 			.asArray.values = values,
	// 		};
	// 		struct VariableDescription variable = {
	// 			.identifier = function.parameters[i].name,
	// 			.project = project,
	// 			.statement = mainFunction.statement,
	// 		};
	// 		assignVariable(variable, value, project);
	// 	}else{
	// 		runtimeError(project, "logic error");
	// 		return -1;
	// 	}
	// }

	// // for(uint32 i = 0; i < argumentCount; i++){
	// // 	AvString strValue = AV_EMPTY;

	// // 	avDynamicArrayRead(&strValue, i, arguments);
	// // 	struct Value value = {
	// // 		.type = VALUE_TYPE_STRING,
	// // 		.asString = strValue,
	// // 	};
	// // 	struct VariableDescription variable = {
	// // 		.identifier = function.parameters[i].name,
	// // 		.project = project,
	// // 		.statement = mainFunction.statement,
	// // 	};
	// // 	assignVariable(variable, value, project);
	// // }
	// struct Value returnValue = runFunction(function, project);
	// endLocalContext(project);
	// if(returnValue.type == VALUE_TYPE_NUMBER){
	// 	return returnValue.asNumber;
	// }
	// if(returnValue.type == VALUE_TYPE_STRING){
	// 	return returnValue.asString.len == 0;
	// }
	// if(returnValue.type == VALUE_TYPE_ARRAY){
	// 	return returnValue.asArray.count == 0;
	// }
	// return -1;
}
