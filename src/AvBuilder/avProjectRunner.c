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
#include "avBuilder.h"

#define NULL_VALUE (struct Value){0}

void printValue(struct Value value);

void runtimeError(Project* project, const char* message, ...){
	va_list args;
	va_start(args, message);

	avStringPrintf(AV_CSTR("Runtime Error in project %S:\n\t"), project->name);
	avStringPrintfVA(AV_CSTR(message), args);

	avStringPrintf(AV_CSTR("\nVariables: [\n"));

	//LocalContext* context = project->localContext;

	// while(context){
	// 	if(avDynamicArrayGetSize(context->variables) > 0){
	// 		for(uint32 index = 0; index < avDynamicArrayGetSize(context->variables); index++) { 
	// 			struct VariableDescription element; avDynamicArrayRead(&element, index, (context->variables)); { 
	// 				struct VariableDescription var = element; 
	// 				avStringPrintf(AV_CSTR("\t%S = "), var.identifier); 
	// 				if(var.value){
	// 					printValue(*var.value); 
	// 				}else{
	// 					avStringPrint(AV_CSTR("NULL"));
	// 				}
	// 				avStringPrint(AV_CSTR("\n")); 
	// 			} 
	// 		}
	// 		if(context->previous){
	// 			avStringPrint(AV_CSTR("], [\n"));
	// 		}
	// 	}
	// 	if(!context->inherit){
	// 		break;
	// 	}
	// 	context = context->previous;
	// }
	// avStringPrintf(AV_CSTR("]\n"));

	// avStringPrintf(AV_CSTR("Globals: [\n"));
	// for(uint32 index = 0; index < avDynamicArrayGetSize(project->variables); index++) {
	// 	 struct VariableDescription element; 
	// 	 avDynamicArrayRead(&element, index, (project->variables)); 
	// 	 { 
	// 		struct VariableDescription var = element; 
	// 		avStringPrintf(((AvString){
	// 			.chrs="\t%S = ", 
	// 			.len=avCStringLength("\t%S = "), 
	// 			.memory=((AvStringMemory*)0)
	// 		}), var.identifier); 
	// 		if(var.value){
	// 			printValue(*var.value); 
	// 		}
	// 		avStringPrint(((AvString){
	// 			.chrs="\n", .len=avCStringLength("\n"), .memory=((AvStringMemory*)0)
	// 		})); 
	// 	} 
	// };
	// avStringPrintf(AV_CSTR("]\n"));

	// avStringPrintf(AV_CSTR("Constants: [\n"));
	// avDynamicArrayForEachElement(struct VariableDescription, project->constants, {
	// 	struct VariableDescription var = element;
	// 	avStringPrintf(AV_CSTR("\t%S = "), var.identifier);
	// 	printValue(*var.value);
	// 	avStringPrint(AV_CSTR("\n"));
	// });
	// avStringPrintf(AV_CSTR("]\n"));

	// avStringPrintf(AV_CSTR("Externals: [\n"));
	// avDynamicArrayForEachElement(struct VariableDescription, project->externals, {
	// 	struct VariableDescription var = element;
	// 	avStringPrintf(AV_CSTR("\t%S\n"), var.identifier);
	// });
	// avStringPrintf(AV_CSTR("]\n"));

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

static int64 parseNumber(AvString string){
	

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
	int64 value = 0;

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

void destroyValue(Value* value){
	avAssert(value != NULL, "dst must be valid");
	Value src = *value;
	avMemset(value, 0, sizeof(Value));
	if(src.type == VALUE_TYPE_NUMBER){
		return;
	}
	if(src.type == VALUE_TYPE_STRING){
		avStringFree(&src.asString);
		return;
	}
	if(src.type == VALUE_TYPE_ARRAY){
		for(uint32 i = 0; i < src.asArray.count; i++){
			ConstValue val = src.asArray.values[i];
			if(val.type==VALUE_TYPE_NUMBER){
				continue;
			}
			if(val.type==VALUE_TYPE_STRING){
				avStringFree(&val.asString);
				continue;
			}
			avAssert(0, "Invalid constValue type");
		}
		if(src.asArray.values) avFree(src.asArray.values);
		return;
	}
	if(src.type == VALUE_TYPE_NONE){
		return;
	}
	avAssert(0, "Invalid type");
}

void cloneValue(Value* dst, Value src){
	avAssert(dst != NULL, "dst must be valid");
	avAssert(src.type!=VALUE_TYPE_NONE, "value must be valid");
	if(dst->type!=VALUE_TYPE_NONE){
		destroyValue(dst);
	}
	dst->type = src.type;
	if(src.type == VALUE_TYPE_NUMBER){
		dst->asNumber = src.asNumber;
		return;
	}
	if(src.type == VALUE_TYPE_STRING){
		avStringClone(&dst->asString, src.asString);
		return;
	}
	if(src.type == VALUE_TYPE_ARRAY){
		ConstValue* values;
		if(src.asArray.count){
			values = avAllocate(sizeof(ConstValue)*src.asArray.count, "");
		}
		for(uint32 i = 0; i < src.asArray.count; i++){
			values[i].type = src.asArray.values[i].type;
			if(values[i].type == VALUE_TYPE_NUMBER){
				values[i].asNumber = src.asArray.values[i].asNumber;
				continue;
			}
			if(values[i].type == VALUE_TYPE_STRING){
				avStringClone(&values[i].asString, src.asArray.values[i].asString);
				continue;;
			}
			avAssert(values[i].type!=VALUE_TYPE_NONE, "value must be valid");
		}
		return;
	}
	avAssert(0, "Invalid type");
}

void enterStackFrame(Scope* scope, Project* ctx){
	// bootstrap frame & allocator
	AvAllocator allocator;
	avAllocatorCreate(0, AV_ALLOCATOR_TYPE_DYNAMIC, &allocator);
	StackFrame* frame = avAllocatorAllocate(sizeof(StackFrame), &allocator);
	avMemcpy(&frame->allocator, &allocator, sizeof(AvAllocator));
	frame->valueCount = avDynamicArrayGetSize(scope->symbols);
	frame->values = avAllocatorAllocate(sizeof(struct Value) * frame->valueCount, &frame->allocator);

    frame->scope = scope;
	frame->parent = ctx->currentStackFrame;
	ctx->currentScope = scope;
	ctx->currentStackFrame = frame;
	ctx->allocator = &frame->allocator;
}

void exitStackFrame(Project* ctx){
	StackFrame* frame = ctx->currentStackFrame;
	avAssert(frame!=NULL, "stackframe inbalance");

	StackFrame* parent = frame->parent;

	for(uint32 i = 0; i < frame->valueCount; i++){
		destroyValue(frame->values + i);
	}

	avAllocatorDestroy(&frame->allocator);

	ctx->currentStackFrame = parent;
	if(parent){
		ctx->currentScope = parent->scope;
		ctx->allocator = &parent->allocator;
	}else{
		ctx->currentScope = ctx->toplevelScope;
		ctx->allocator = &ctx->baseAllocator;
	}
}

bool32 assignReturnValue(Symbol* symbol, int32 localDepth, Value value, Project* ctx){
	avAssert(symbol!=NULL, "symbol must be valid");
	avAssert(value.type!=VALUE_TYPE_NONE, "value must be valid");
	avAssert(symbol->type==-1, "can only return to the return slot");
	
	if(!symbol->builtin){
		return false;
	}
	if(!symbol->constant){
		return false;
	}

	StackFrame* frame = ctx->currentStackFrame;
	Scope* targetScope = frame->scope;
	for(uint32 i = 0; i < localDepth; i++){
		if(targetScope == NULL){
			return false;
		}
		targetScope = targetScope->parent;
	}

	while (frame && frame->scope != targetScope){
    	frame = frame->parent;
	}

	if(frame == NULL){
		return false;
	}
	if(symbol->localIndex >= frame->valueCount){
		return false;
	}
	
	Value* ptr = frame->values + symbol->localIndex;
	cloneValue(ptr, value);
	return true;
}

bool32 assingSymbol(Symbol* symbol, int32 localDepth, Value value, Project* ctx){
	avAssert(symbol!=NULL, "symbol must be valid");
	avAssert(value.type!=VALUE_TYPE_NONE, "value must be valid");
	
	if(symbol->builtin){
		return false;
	}
	if(symbol->constant){
		return false;
	}

	StackFrame* frame = ctx->currentStackFrame;
	Scope* targetScope = frame->scope;
	for(uint32 i = 0; i < localDepth; i++){
		if(targetScope == NULL){
			return false;
		}
		targetScope = targetScope->parent;
	}

	while (frame && frame->scope != targetScope){
    	frame = frame->parent;
	}

	if(frame == NULL){
		return false;
	}
	if(symbol->localIndex >= frame->valueCount){
		return false;
	}
	
	Value* ptr = frame->values + symbol->localIndex;
	cloneValue(ptr, value);
	return true;
}

bool32 retrieveSymbol(Symbol* symbol, int32 localDepth, Value* value, Project* ctx){
	avAssert(symbol!=NULL, "symbol must be valid");
	avAssert(value!=NULL, "value must be valid");

	if(symbol->builtin){
		cloneValue(value, symbol->variable.constValue);
		return true;
	}
	StackFrame* frame = ctx->currentStackFrame;
	Scope* targetScope = frame->scope;
	for(uint32 i = 0; i < localDepth; i++){
		if(targetScope == NULL){
			return false;
		}
		targetScope = targetScope->parent;
	}

	while (frame && frame->scope != targetScope){
    	frame = frame->parent;
	}

	if(frame == NULL){
		return false;
	}
	if(symbol->localIndex >= frame->valueCount){
		return false;
	}
	avMemcpy(value, &frame->values[symbol->localIndex], sizeof(Value));
	return true;
}

bool32 checkValueTrue(Value val){
	if(val.type == VALUE_TYPE_NONE){
		return false;
	}
	if(val.type == VALUE_TYPE_ARRAY){
		return val.asArray.count != 0;
	}
	if(val.type == VALUE_TYPE_NUMBER){
		return val.asNumber != 0;
	}
	if(val.type == VALUE_TYPE_STRING){
		return !avStringIsEmpty(val.asString);
	}
	return false;
}

bool32 evaluateExpression(Value* value, struct Expression_S expression, Project* ctx);

bool32 evaluateStatement(struct Statement_S* statement, Project* ctx);

bool32 evaluateFunctionCall(Value* value, struct Expression_S expression, Project* ctx){
	Value* values = 0;
	if(expression.call.argumentCount){
		values = avAllocate(sizeof(Value) * expression.call.argumentCount, "");
	}
	for(uint32 i = 0; i < expression.call.argumentCount; i++){
		if(!evaluateExpression(values + i, expression.call.arguments[i], ctx)){
			return false;
		}
	}

	Symbol* sym = expression.call.resolvedSymbol;
	if(sym->builtin){
		Value retVal = callBuiltInFunction(*sym->function.builtin, expression.call.argumentCount, values, ctx);
		cloneValue(value, retVal);
		if(values) avFree(values);
		return true;
	}

	Project* proj = ctx;
	if(sym->external){
		proj = sym->scope->project;
	}
	
	enterStackFrame(sym->scope, proj);

	struct FunctionDefinition_S func = sym->function.definition->functionDefinition;
	for(uint32 i = 0; i < func.parameterCount; i++){
		uint32 size = 1;
		if(func.parameters[i].size.type!=EXPRESSION_TYPE_NONE){
			if(func.parameters[i].resolvedSymbol->variable.constValue.type != VALUE_TYPE_NONE){
				size = func.parameters[i].resolvedSymbol->variable.constValue.asNumber;
			}else{
				Value* value = &func.parameters[i].resolvedSymbol->variable.constValue;
				if(!evaluateExpression(value, func.parameters[i].size, proj)){
					exitStackFrame(proj);
					if(values) avFree(values);
					return false;
				}
				if(value->type != VALUE_TYPE_NUMBER || value->asNumber <= 0){
					runtimeError(proj, "Parameter %S size does not resolve to valid number", func.parameters[i].name);
					exitStackFrame(proj);
					if(values) avFree(values);
					return false;
				}
			}
		}else if(func.parameters[i].unknownSize){
			size = expression.call.argumentCount - func.parameterCount + 1;
		}

		if(values[i].type == VALUE_TYPE_ARRAY){
			if(size != values[i].asArray.count){
				runtimeError(proj, "Parameter %S, was not provided with right size of %u", func.parameters[i].name, size);
				exitStackFrame(proj);
				if(values) avFree(values);
				return false;
			}
		}else{
			if(size != 1){
				runtimeError(proj, "Parameter %S, was not provided with right size of 1", func.parameters[i].name);
				exitStackFrame(proj);
				if(values) avFree(values);
				return false;
			}
		}

		if(!assingSymbol(func.parameters[i].resolvedSymbol, 0, values[i], proj)){
			exitStackFrame(proj);
			if(values) avFree(values);
			return false;
		}
	}

	if(!evaluateStatement(func.body, proj)){
		exitStackFrame(proj);
		if(values) avFree(values);
		return false;
	}

	struct Value returnValue = proj->currentStackFrame->values[0];
	//avMemcpy(value, &returnValue, sizeof(Value));
	cloneValue(value, returnValue);

	exitStackFrame(proj);

	if(values) avFree(values);

	if(proj->currentStackFrame->scope->type != SCOPE_TYPE_TOPLEVEL){
		runtimeError(ctx, "Stack inbalance");
		return false;
	}

	return true;
}

bool32 evaluateExpression(Value* value, struct Expression_S expression, Project* ctx){
	Value tmp = {0};
	if(value==NULL) value = &tmp;
	avMemset(value, 0, sizeof(Value));
	
	switch (expression.type) {
	case EXPRESSION_TYPE_IDENTIFIER:
		if(!retrieveSymbol(
			expression.identifier.resolvedSymbol, 
			expression.identifier.depth, 
			value, 
			expression.identifier.resolvedSymbol->external ? expression.identifier.resolvedSymbol->scope->project : ctx
		)){
			return false;
		}
		return true;
	case EXPRESSION_TYPE_LITERAL:{
		Value val = {.type = VALUE_TYPE_STRING,.asString = expression.literal.value};
		avMemcpy(value, &val, sizeof(Value));
		return true;
	}
	case EXPRESSION_TYPE_NUMBER:{
		Value val = {.type = VALUE_TYPE_NUMBER, .asNumber = parseNumber(expression.number.value)};
		avMemcpy(value, &val, sizeof(Value));
		return true;
	}
	case EXPRESSION_TYPE_CALL:{
		return evaluateFunctionCall(value, expression, ctx);
	}
	case EXPRESSION_TYPE_ARRAY:{
		ConstValue* values = avAllocatorAllocate(sizeof(ConstValue)*expression.array.length, ctx->allocator);
		for(uint32 i = 0; i < expression.array.length; i++){
			Value tmp;
			if(!evaluateExpression(&tmp, expression.array.elements[i], ctx)){
				return false;
			}
			toConstValue(tmp, values + i, ctx);
		}
		Value val = {
			.type = VALUE_TYPE_ARRAY,
			.asArray = {
				.count = expression.array.length,
				.values = values,
			},
		};
		avMemcpy(value, &val, sizeof(Value));
		return true;
	}
	case EXPRESSION_TYPE_ASSIGNMENT:{
		Value val = {0};
		if(!evaluateExpression(&val, *expression.assignment.value, ctx)){
			return false;
		}
		//TODO: continue here
	}
	default:
		runtimeError(ctx, "Not implemented yet");
		return false;
	}



	return true;
}

bool32 evaluateForeach(struct Statement_S statement, Project* ctx){
	Value collection;
	if(!evaluateExpression(&collection, *statement.foreachStatement.collection, ctx)){
		return false;
	}
	ConstValue tmp;
	ConstValue* values = &tmp;
	uint32 count = 1;
	switch(collection.type){
		case VALUE_TYPE_ARRAY:
			values = collection.asArray.values;
			count = collection.asArray.count;
			break;
		case VALUE_TYPE_STRING:
		case VALUE_TYPE_NUMBER:
			toConstValue(collection, &tmp, ctx);
			break;
		default:
			runtimeError(ctx, "Invalid collection type");
			return false;
	}

	enterStackFrame(statement.attachedScope, ctx);

	for(uint32 i = 0; i < count; i++){
		Value value;
		toValue(values[i], &value);
		if(!assingSymbol(statement.foreachStatement.resolvedVarSymbol, 0, value, ctx)){
			exitStackFrame(ctx);
			return false;
		}
		if(statement.foreachStatement.resolvedIndexSymbol){
			if(!assingSymbol(statement.foreachStatement.resolvedIndexSymbol, 0, (Value){.type=VALUE_TYPE_NUMBER, .asNumber=i}, ctx)){
				exitStackFrame(ctx);
				return false;
			}
		}
		if(!evaluateStatement(statement.foreachStatement.statement, ctx)){
			return false;
		}
		if(ctx->controlFlow==CONTROLFLOW_BREAK){
			ctx->controlFlow = CONTROLFLOW_NORMAL;
			break;
		}
		if(ctx->controlFlow==CONTROLFLOW_CONTINUE){
			ctx->controlFlow = CONTROLFLOW_NORMAL;
			continue;
		}
		if(ctx->controlFlow==CONTROLFLOW_RETURN){
			break;
		}
	}

	exitStackFrame(ctx);
	return true;
}

bool32 evaluateImport(struct Statement_S statement, Project* ctx){

	struct ImportStatement_S import = statement.importStatement;
	for(uint32 i = 0; i < import.mappingCount; i++){
		struct ImportMapping_S mapping = import.mappings[i];
		if(mapping.type & DEFINITION_MAPPING_PROVIDE){
			continue;
		}

		if(mapping.resolvedExternal->type!=SYMBOL_VARIABLE){
			continue;
		}
		Value value;
		if(!retrieveSymbol(mapping.resolvedExternal, 0, &value, import.project)){
			return false;
		}
		if(!assingSymbol(mapping.resolvedSymbol, 0, value, ctx)){
			return false;
		}
	}
	return true;
}

bool32 evaluateStatement(struct Statement_S* statement, Project* ctx){
	switch(statement->type){
		case STATEMENT_TYPE_BLOCK:
			if(statement->attachedScope){
				enterStackFrame(statement->attachedScope, ctx);
			}
			for(uint32 i = 0; i < statement->block.statementCount; i++){
				if(!evaluateStatement(&statement->block.statements[i], ctx)){
					exitStackFrame(ctx);
					return false;
				}
				if(ctx->controlFlow!=CONTROLFLOW_NORMAL){
					break;
				}
			}
			if(statement->attachedScope){
				exitStackFrame(ctx);
			}
			return true;
		case STATEMENT_TYPE_EXPRESSION:
			return evaluateExpression(NULL, statement->expression, ctx);
		case STATEMENT_TYPE_IF:{
			Value value;
			if(!evaluateExpression(&value, *statement->ifStatement.check, ctx)){
				return false;
			}
			if(checkValueTrue(value)){
				if(!evaluateStatement(statement->ifStatement.branch, ctx)){
					return false;
				}
			}else{
				if(!evaluateStatement(statement->ifStatement.alternativeBranch, ctx)){
					return false;
				}
			}
			return true;
		}
		case STATEMENT_TYPE_FOREACH:
			return evaluateForeach(*statement, ctx);
		case STATEMENT_TYPE_BREAK:
			ctx->controlFlow = CONTROLFLOW_BREAK;
			return true;
		case STATEMENT_TYPE_CONTINUE:
			ctx->controlFlow = CONTROLFLOW_CONTINUE;
			return true;
		case STATEMENT_TYPE_RETURN:{
			if(statement->returnStatement.value){
				Value value;
				if(!evaluateExpression(&value, *statement->returnStatement.value, ctx)){
					return false;
				}
				if(!assignReturnValue(statement->returnStatement.resolvedVirtualSymbol, statement->returnStatement.returnDepth, value, ctx)){
					return false;
				}
			}
			ctx->controlFlow = CONTROLFLOW_RETURN;
			return true;
		}
		case STATEMENT_TYPE_VARIABLE_DEFINITION:
			if(statement->variableDefinition.initialValue.type != EXPRESSION_TYPE_NONE){
				Value value;
				if(!evaluateExpression(&value, statement->variableDefinition.initialValue, ctx)){
					return false;
				}
				if(!assingSymbol(statement->variableDefinition.resolvedSymbol, 0, value, ctx)){
					return false;
				}
			}
			return true;
		case STATEMENT_TYPE_INHERIT:
			if(!statement->inheritStatement.resolvedSymbol->external && statement->inheritStatement.defaultValue){
				Value val;
				if(!evaluateExpression(&val, *statement->inheritStatement.defaultValue, ctx)){
					return false;
				}
				if(!assingSymbol(statement->inheritStatement.resolvedSymbol, 0, val, ctx)){
					return false;
				}
			}else if(!statement->inheritStatement.resolvedSymbol->external && !statement->inheritStatement.defaultValue){
				runtimeError(ctx, "inherit  default valuenot %S not specified", statement->inheritStatement.variable);
			}else if(statement->inheritStatement.resolvedSymbol->external){
				// statement->inheritStatement.resolvedSymbol = statement->inheritStatement.externalSymbol;
				return true;
			}else{
				runtimeError(ctx, "Logic Error");
				return false;
			}
		case STATEMENT_TYPE_IMPORT:
			return evaluateImport(*statement, ctx);
		default:	
			runtimeError(ctx, "Not implemented yet");
			return false;
	}



	return true;
}

void exitAllImportedProjectStackFrames(Project* project){
	for(uint32 index = 0; index < avDynamicArrayGetSize(project->importedProjects); index++) { 
		Project** element = avDynamicArrayGetPtr(index, project->importedProjects); 
		while((*element)->currentStackFrame){
			exitStackFrame(*element);
		}
	};
}

uint32 runProject(Project* project, AvDynamicArray arguments){
	project->controlFlow = CONTROLFLOW_NORMAL;
	AvString* entry = &project->name;
	if(project->options.entry.len > 0 && project->options.entry.chrs){
		entry = &project->options.entry;
	}

	Symbol* funcSym =  resolveSymbol(*entry, 0, project);
	if(funcSym->type != SYMBOL_FUNCTION){
		runtimeError(project, "Entry %S is not a function", *entry);
		return -1;
	}
	if(funcSym->builtin){
		runtimeError(project, "Cannot call builtin function as entry", *entry);
		return -1;
	}
	
	enterStackFrame(project->toplevelScope, project);
	for(uint32 index = 0; index < avDynamicArrayGetSize(project->importedProjects); index++) { 
		Project** element = avDynamicArrayGetPtr(index, project->importedProjects); 
		enterStackFrame((*element)->toplevelScope, *element);

		bool32 ret = true;
		for(uint32 i = 0; i < (*element)->statementCount; i++){
			struct Statement_S* statement = (*element)->statements+i;
			switch(statement->type){
				case STATEMENT_TYPE_VARIABLE_DEFINITION:
				case STATEMENT_TYPE_INHERIT:
				case STATEMENT_TYPE_IMPORT:
					if(!evaluateStatement(statement, (*element))){
						ret = false;
					}
					break;
				default:
					break;
			}
			if(ret == false){
				break;
			}
		}
		if(ret==false){
			while(index-- != 0){
				exitStackFrame(*element);
				element = avDynamicArrayGetPtr(index, project->importedProjects); 
			}
			exitStackFrame(*element);
			exitStackFrame(project);
			return -1;
		}
	};
	bool32 ret = true;
	// fill the stack frame with values
	for(uint32 i = 0; i < project->statementCount; i++){
		struct Statement_S* statement = project->statements +i;
		switch(statement->type){
			case STATEMENT_TYPE_VARIABLE_DEFINITION:
			case STATEMENT_TYPE_INHERIT:
			case STATEMENT_TYPE_IMPORT:
				if(!evaluateStatement(statement, project)){
					ret = false;
				}
				break;
			default:
				break;
		}
		if(ret == false){
			break;
		}
	}

	if(ret == false){
		exitStackFrame(project);
		exitAllImportedProjectStackFrames(project);
		return -1;
	}



	struct Statement_S functionStatement = *funcSym->function.definition;
	struct FunctionDefinition_S func = functionStatement.functionDefinition;
	avAssert(functionStatement.attachedScope!=NULL, "function must have attached scope");

	enterStackFrame(functionStatement.attachedScope, project);

	// fill arguments
	uint32 argumentIndex = 0;
	uint32 argumentCount = avDynamicArrayGetSize(arguments);
	for(uint32 i = 0; i < func.parameterCount; i++){
		struct FunctionParameter_S param = func.parameters[i];
		Symbol* sym = param.resolvedSymbol;
		
		uint32 size = 1;
		if(param.size.type != EXPRESSION_TYPE_NONE){
			Value sizeVal = {0};
			if(!evaluateExpression(&sizeVal, param.size, project)){
				ret = false;
				break;
			}
			if(sizeVal.type!=VALUE_TYPE_NUMBER){
				runtimeError(project, "parameter size %S does not resolve to a number", param.name);
				ret = false;
				break;
			}
		}
		if(param.unknownSize){
			size = argumentCount - argumentIndex;
		}

		if(size > (argumentCount - argumentIndex) || size == 0){
			runtimeError(project, "Not enough arguments provided to function %S", func.functionName);
			ret = false;
			break;
		}

		if(size == 1){
			Value val = (Value){.type= VALUE_TYPE_STRING,};
			avDynamicArrayRead(&val.asString, argumentIndex++, arguments);
			if(!assingSymbol(sym, 0, val, project)){
				ret = false;
				break;
			}
		}else{
			Value val = (Value){
				.type=VALUE_TYPE_ARRAY,
				.asArray ={
					.count = size,
					.values = avAllocate(sizeof(Value)*size, ""),
				},
			};
			for(uint32 j = 0; j < size; j++){
				val.asArray.values[j].type = VALUE_TYPE_STRING;
				avDynamicArrayRead(&val.asArray.values[j].asString, argumentIndex++, arguments);
			}
			if(!assingSymbol(sym, 0, val, project)){
				avFree(val.asArray.values);
				ret = false;
				break;
			}
			avFree(val.asArray.values);
		}

	}
	if(ret == false){
		exitStackFrame(project);
		exitStackFrame(project);

		exitAllImportedProjectStackFrames(project);
		return -1;
	}

	// we are now ready to rumble
	evaluateStatement(func.body, project);
	// we are now done with rumbling

	int32 retVal = -1;
	avAssert(project->currentStackFrame!=NULL, "scope inbalance");
	struct Value returnValue = project->currentStackFrame->values[0];
	// retrieve return value
	if(returnValue.type == VALUE_TYPE_NUMBER){
		retVal = returnValue.asNumber;
	}
	if(returnValue.type == VALUE_TYPE_STRING){
		retVal = returnValue.asString.len == 0;
	}
	if(returnValue.type == VALUE_TYPE_ARRAY){
		retVal = returnValue.asArray.count == 0;
	}
	if(returnValue.type==VALUE_TYPE_NONE){
		retVal = 0;
	}
	
	exitStackFrame(project);
	
	exitStackFrame(project);
	
	exitAllImportedProjectStackFrames(project);
	
	return retVal;
}
