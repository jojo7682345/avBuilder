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

static uint64 getLineOffset(AvString string, uint64 startingOffset, uint32 line){
    for(uint64 i = startingOffset; i < string.len; i++){
        char c = string.chrs[i];
        if(c=='\n'){
            line--;
            if(line == 0){
                return i+1;
            }
        }
    }
    return (uint64)-1;
}

void runtimeError(Project* project, const char* message, ...){
	va_list args;
	va_start(args, message);

	avStringPrintf(AV_CSTR("Runtime Error in project %S:\n\t"), project->name);
	avStringPrintfVA(AV_CSTR(message), args);

	avStringPrintf(AV_CSTR("\nFunction: %s:%u\n"), project->currentScope->functionName, project->currentLine);

    
    for(uint32 i = project->currentLine - 3; i < project->currentLine + 3; i++){
        char buffer[4096];
        uint64 start = getLineOffset(project->projectFileContent, 0, i);
        uint64 end = getLineOffset(project->projectFileContent, start, 1);
        if(start == (uint64)-1) continue;
        if(end == (uint64)-1) end = start + sizeof(buffer)-1;
        if(end - start > sizeof(buffer)-1){
            end = start + sizeof(buffer)-1;
        }
        avMemcpy(buffer, project->projectFileContent.chrs + start, end-start);
        buffer[4095] = '\0';
        buffer[end-start] = '\0';
        if(buffer[end-start-1] =='\n') buffer[end-start-1] = '\0';
        avStringPrintf(AV_CSTRA("%u: %s\n"), i, buffer);
    }

    
   

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
    //project.currentScope
    

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

void destroyValue_(Value* value, LOC_PARAM){
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
		}
		if(src.asArray.values) avFree(src.asArray.values);
		return;
	}
	if(src.type == VALUE_TYPE_NONE){
		return;
	}
	avAssert(0, "Invalid type");
}

void destroyConstValue_(ConstValue* value, LOC_PARAM){
	avAssert(value != NULL, "dst must be valid");
	ConstValue src = *value;
	avMemset(value, 0, sizeof(ConstValue));
	if(src.type == VALUE_TYPE_NUMBER){
		return;
	}
	if(src.type == VALUE_TYPE_STRING){
		avStringFree(&src.asString);
		return;
	}
	if(src.type == VALUE_TYPE_NONE){
		return;
	}
	avAssert(0, "Invalid type");
}
void cloneConstValue_(ConstValue* dst, ConstValue src, LOC_PARAM);
void cloneValue_(Value* dst, Value src, LOC_PARAM){
	avAssert(dst != NULL, "dst must be valid");
	if(src.type==VALUE_TYPE_NONE) return;
	//avAssert(src.type!=VALUE_TYPE_NONE, "value must be valid");
	if(dst->type!=VALUE_TYPE_NONE){
		destroyValue_(dst, LOC_PASS);
	}
	dst->type = src.type;
	if(src.type == VALUE_TYPE_NUMBER){
		dst->asNumber = src.asNumber;
		return;
	}
	if(src.type == VALUE_TYPE_STRING){
		if(avStringIsEmpty(src.asString)){
			avMemset(&dst->asString, 0, sizeof(AvString));
		}else{
			avStringClone_(&dst->asString, src.asString, LOC_PASS);
		}
		return;
	}
	if(src.type == VALUE_TYPE_ARRAY){
		ConstValue* values = 0;
		if(src.asArray.count){
			values = avAllocate(sizeof(ConstValue)*src.asArray.count, "");
			avMemset(values, 0, sizeof(ConstValue)*src.asArray.count);
		}
		for(uint32 i = 0; i < src.asArray.count; i++){
			cloneConstValue_(values+i, src.asArray.values[i], LOC_PASS);
			//avAssert(values[i].type!=VALUE_TYPE_NONE, "value must be valid");
		}
		dst->asArray.values = values;
		dst->asArray.count = src.asArray.count;
		return;
	}
	avAssert(0, "Invalid type");
}


void cloneConstValue_(ConstValue* dst, ConstValue src, LOC_PARAM){
	avAssert(dst != NULL, "dst must be valid");
	if(dst->type!=VALUE_TYPE_NONE){
		destroyConstValue_(dst, LOC_PASS);
	}
	dst->type = src.type;
	if(src.type == VALUE_TYPE_NUMBER){
		dst->asNumber = src.asNumber;
		return;
	}
	if(src.type == VALUE_TYPE_STRING){
		if(avStringIsEmpty(src.asString)){
			avMemset(&dst->asString, 0, sizeof(AvString));
		}else{
			avStringClone_(&dst->asString, src.asString, LOC_PASS);
		}
		return;
	}
	if(src.type==VALUE_TYPE_NONE){
		avMemset(dst, 0, sizeof(ConstValue));
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
	if(frame->valueCount) frame->values = avAllocatorAllocate(sizeof(struct Value) * frame->valueCount, &frame->allocator);

	if(ctx->currentStackFrame==0){
		if(scope->type!=SCOPE_TYPE_TOPLEVEL){
			runtimeError(ctx, "Scope unbalanced");
		}
	}

    frame->scope = scope;
	frame->parent = ctx->currentStackFrame;
	ctx->currentScope = scope;
	ctx->currentStackFrame = frame;
	ctx->allocator = &frame->allocator;
	//{
		//StackFrame* frame = ctx->currentStackFrame;
		//printf(AV_STRING_PRINTF_CODE, (int32)ctx->name.len, ctx->name.chrs);
		//while(frame){
		//	printf("\t");
		//	frame = frame->parent;
		//}
		//printf("Enter StackFrame\n");
	//}

	if(!frame->parent){
		if(scope->type!=SCOPE_TYPE_TOPLEVEL){
			runtimeError(ctx, "Scope unbalanced");
		}
	}
}

void exitStackFrame(Project* ctx){

	// {
	// 	StackFrame* frame = ctx->currentStackFrame;
	// 	printf(AV_STRING_PRINTF_CODE, (int32)ctx->name.len, ctx->name.chrs);
	// 	while(frame){
	// 		printf("\t");
	// 		frame = frame->parent;
	// 	}
	// 	printf("Exit StackFrame\n");
	// }
	
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

bool32 assignSymbol(Symbol* symbol, int32 localDepth, Value value, uint32 index, Project* ctx){
	avAssert(symbol!=NULL, "symbol must be valid");
	//avAssert(value.type!=VALUE_TYPE_NONE, "value must be valid");
	
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

	if(ptr->type==VALUE_TYPE_ARRAY && index != -1){
		if(index >= ptr->asArray.count){
			runtimeError(ctx, "Assigning symbol %S out of bounds", symbol->identifier);
			return false;
		}
		ConstValue tmp;
		toConstValue(value, &tmp, ctx);
		cloneConstValue(ptr->asArray.values + index, tmp);
	}else{
		if(index == -1){
			index = 0;
		}
		if(index > 0){
			runtimeError(ctx, "Assigning symbol %S out of bounds", symbol->identifier);
			return false;
		}
		cloneValue(ptr, value);
	}
	

	

	return true;
}

#define retrieveSymbol(symbol, localDepth, value, ctx, ...) retrieveSymbol_(symbol, localDepth, value, ctx __VA_OPT__(,) __VA_ARGS__, LOC)
bool32 retrieveSymbol_(Symbol* symbol, int32 localDepth, Value* value, Project* ctx, LOC_PARAM){
	avAssert(symbol!=NULL, "symbol must be valid");
	avAssert(value!=NULL, "value must be valid");

	if(symbol->builtin){
		if(symbol->type!=SYMBOL_VARIABLE) return false;
		cloneValue(value, symbol->variable.constValue);
		return true;
	}

	StackFrame* frame = ctx->currentStackFrame;
	if(symbol->external){
		frame = symbol->scope->project->currentStackFrame;
	}
	
	Scope* targetScope = symbol->scope;
	// for(uint32 i = 0; i < localDepth; i++){
	// 	if(targetScope == NULL){
	// 		return false;
	// 	}
	// 	targetScope = targetScope->parent;
	// }

	while (frame && frame->scope != targetScope){
    	frame = frame->parent;
	}

	if(frame == NULL){
		return false;
	}
	// if(avStringEquals(symbol->identifier, AV_CSTRA("sources"))){
	// 	avStringPrintln(AV_CSTRA("ACCESS"));
	// }
	if(symbol->localIndex >= frame->valueCount){
		return false;
	}
	//avMemcpy(value, &frame->values[symbol->localIndex], sizeof(Value));
	Value val = frame->values[symbol->localIndex];
	if(val.type==VALUE_TYPE_NONE){
		runtimeError(ctx, "Reading undefined variable %S", symbol->identifier);
		return false;
	}
	cloneValue_(value, val, LOC_PASS);
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

bool32 performFunctionCall(Symbol* fn, Value* returnValue, uint32 argumentCount, Value* values, Project* ctx){
	Symbol* sym = fn;
	avAssert(fn != NULL, "Function must have valid symbol");
	if(sym->builtin){
		Value retVal = callBuiltInFunction(*sym->function.builtin, argumentCount, values, ctx);
		cloneValue(returnValue, retVal);
		destroyValue(&retVal);
		return true;
	}
	
	Project* proj = ctx;
	if(sym->external){
		proj = sym->scope->project;
	}
	//printf("Function Call: " AV_STRING_PRINTF_CODE ":\n", (int)sym->function.definition->functionDefinition.functionName.len,sym->function.definition->functionDefinition.functionName.chrs);
	enterStackFrame(sym->function.definition->attachedScope, proj);

	struct FunctionDefinition_S func = sym->function.definition->functionDefinition;
	for(uint32 i = 0; i < func.parameterCount; i++){
		uint32 size = 1;
		bool32 sizeSpecified = false;
		if(func.parameters[i].size.type!=EXPRESSION_TYPE_NONE){
			sizeSpecified = true;
			if(func.parameters[i].resolvedSymbol->variable.constValue.type != VALUE_TYPE_NONE){
				size = func.parameters[i].resolvedSymbol->variable.constValue.asNumber;
			}else{
				Value* value = &func.parameters[i].resolvedSymbol->variable.constValue;
				if(!evaluateExpression(value, func.parameters[i].size, proj)){
					exitStackFrame(proj);
					return false;
				}
				if(value->type != VALUE_TYPE_NUMBER || value->asNumber <= 0){
					runtimeError(proj, "Parameter %S size does not resolve to valid number", func.parameters[i].name);
					exitStackFrame(proj);
					return false;
				}
			}
		}else if(func.parameters[i].unknownSize){
			size = argumentCount - func.parameterCount + 1;
		}

		if(values[i].type == VALUE_TYPE_ARRAY){
			if(sizeSpecified && size != values[i].asArray.count){
				runtimeError(proj, "Parameter %S, was not provided with right size of %u", func.parameters[i].name, size);
				exitStackFrame(proj);
				return false;
			}
		}else{
			if(size != 1){
				runtimeError(proj, "Parameter %S, was not provided with right size of 1", func.parameters[i].name);
				exitStackFrame(proj);
				return false;
			}
		}

		if(!assignSymbol(func.parameters[i].resolvedSymbol, 0, values[i], 0, proj)){
			exitStackFrame(proj);
			return false;
		}
	}

	if(!evaluateStatement(func.body, proj)){
		exitStackFrame(proj);
		runtimeError(proj, "Error while evaluating function call");
		return false;
	}

	// restore controlflow
	proj->controlFlow = CONTROLFLOW_NORMAL;

	struct Value retValue = proj->currentStackFrame->values[0];
	if(retValue.type==VALUE_TYPE_NONE){
		struct Value none = {.type=VALUE_TYPE_NUMBER,.asNumber= 0};
		avMemcpy(returnValue, &none, sizeof(Value));
	}else{
		cloneValue(returnValue, retValue);
	}



	exitStackFrame(proj);
	return true;
}

bool32 evaluateFunctionCall(Value* value, struct Expression_S expression, Project* ctx){
	Value* values = 0;
	if(expression.call.argumentCount){
		values = avCallocate(expression.call.argumentCount, sizeof(Value), "");
	}
	for(uint32 i = 0; i < expression.call.argumentCount; i++){
		if(!evaluateExpression(values + i, expression.call.arguments[i], ctx)){
			return false;
		}
	}

	if(!performFunctionCall(expression.call.resolvedSymbol, value, expression.call.argumentCount, values, ctx)){
		return false;
	}

	for(uint32 i = 0; i < expression.call.argumentCount; i++){
		destroyValue(&values[i]);
	}
	if(values) avFree(values);

	return true;
}

struct Value compareArrays(Project* project, struct Value left, enum ComparisonOperator operator, struct Value right){
	bool32 trueValue = 1;
	bool32 falseValue = 0;
	switch(operator){
		case COMPARISON_OPERATOR_NOT_EQUALS:
			trueValue = 0;
			falseValue = 1;
		case COMPARISON_OPERATOR_EQUALS:
			if(left.asArray.count != right.asArray.count){
				return (struct Value){ .type = VALUE_TYPE_NUMBER, .asNumber = falseValue };
			}
			for(uint32 i = 0; i < left.asArray.count; i++){
				struct ConstValue leftVal = left.asArray.values[i];
				struct ConstValue rightVal = right.asArray.values[i];
				if(leftVal.type != rightVal.type){
					return (struct Value){ .type = VALUE_TYPE_NUMBER, .asNumber = falseValue };
				}
				if(leftVal.type == VALUE_TYPE_NUMBER && leftVal.asNumber!=rightVal.asNumber) {
					return (struct Value){ .type = VALUE_TYPE_NUMBER, .asNumber = falseValue }; 
				}else if(leftVal.type == VALUE_TYPE_STRING && avStringEquals(leftVal.asString, rightVal.asString)) {
					return (struct Value){ .type = VALUE_TYPE_NUMBER, .asNumber = falseValue };
				}else{
					runtimeError(project, "Invalid array content for comparrison");
					return (struct Value){ .type = VALUE_TYPE_NUMBER, .asNumber = falseValue }; 
				}
			}
			return (struct Value){ .type = VALUE_TYPE_NUMBER, .asNumber = trueValue };
		case COMPARISON_OPERATOR_LESS_THAN_OR_EQUAL:
			trueValue = 0;
			falseValue = 1;
		case COMPARISON_OPERATOR_GREATER_THAN:
			return (struct Value) {.type = VALUE_TYPE_NUMBER, .asNumber = (left.asArray.count > right.asArray.count)?trueValue : falseValue};
		case COMPARISON_OPERATOR_GREATER_THAN_OR_EQUAL:
			trueValue = 0;
			falseValue = 1;
		case COMPARISON_OPERATOR_LESS_THAN:
			return (struct Value) {.type = VALUE_TYPE_NUMBER, .asNumber = (left.asArray.count < right.asArray.count)?trueValue : falseValue};
		default:
			runtimeError(project, "invalid comparison operator");
	}
	return (struct Value){ .type = VALUE_TYPE_NUMBER, .asNumber = 0 };
}

struct Value compareArrayString(Project* project,struct Value left, enum ComparisonOperator operator, struct Value right){
	bool32 trueValue = 1;
	bool32 falseValue = 0;
	if(avStringIsEmpty(right.asString)){
		switch(operator){
			case COMPARISON_OPERATOR_NOT_EQUALS:
				trueValue = 0;
				falseValue = 1;
			case COMPARISON_OPERATOR_EQUALS:
				return (struct Value) {.type = VALUE_TYPE_NUMBER, .asNumber = (left.asArray.count == 0)? trueValue : falseValue};
			case COMPARISON_OPERATOR_LESS_THAN_OR_EQUAL:
				trueValue = 0;
				falseValue = 1;
			case COMPARISON_OPERATOR_GREATER_THAN:
				return (struct Value) {.type = VALUE_TYPE_NUMBER, .asNumber = (left.asArray.count > 0)? trueValue : falseValue}; 
			case COMPARISON_OPERATOR_GREATER_THAN_OR_EQUAL:
			   return (struct Value) {.type = VALUE_TYPE_NUMBER, .asNumber = trueValue};
			case COMPARISON_OPERATOR_LESS_THAN:
				return (struct Value) {.type = VALUE_TYPE_NUMBER, .asNumber = falseValue}; 
			default:
				runtimeError(project, "invalid comparison operator");
				return (struct Value){ .type = VALUE_TYPE_NUMBER, .asNumber = 0 };
		}
	}


	struct Value tmpValue = {
		.type = VALUE_TYPE_ARRAY,
		.asArray.count = 1,
		.asArray.values = (struct ConstValue*)&right, // this is ok here as the tmp value is only compared agains, and not used further.
	};
	return compareArrays(project, left, operator, tmpValue);
}

static enum ComparisonOperator swapCompareOperator(enum ComparisonOperator operator){
	switch(operator){
  
		case COMPARISON_OPERATOR_EQUALS:
		case COMPARISON_OPERATOR_NOT_EQUALS:
		break;
		case COMPARISON_OPERATOR_GREATER_THAN:
			operator = COMPARISON_OPERATOR_LESS_THAN_OR_EQUAL;
			break;
		case COMPARISON_OPERATOR_GREATER_THAN_OR_EQUAL:
			operator = COMPARISON_OPERATOR_LESS_THAN;
			break;
		case COMPARISON_OPERATOR_LESS_THAN:
			operator = COMPARISON_OPERATOR_GREATER_THAN_OR_EQUAL;
			break;
		case COMPARISON_OPERATOR_LESS_THAN_OR_EQUAL:
			operator = COMPARISON_OPERATOR_GREATER_THAN;
			break;
		default:
			break;
	}
	return operator;
}
struct Value compareStringArray(Project* project, struct Value left, enum ComparisonOperator operator, struct Value right){
	operator = swapCompareOperator(operator);
	return compareArrayString(project, right, operator, left);
}

struct Value compareArrayNumber(Project* project, struct Value left, enum ComparisonOperator operator, struct Value right){
	bool32 trueValue = 1;
	bool32 falseValue = 0;
	if(right.asNumber == 0){
		if(left.asArray.count == 0){
			switch(operator){
				case COMPARISON_OPERATOR_NOT_EQUALS:
					trueValue = 0;
					falseValue = 1;
				case COMPARISON_OPERATOR_EQUALS:
					return (struct Value) {.type = VALUE_TYPE_NUMBER, .asNumber = (left.asArray.count == 0)? trueValue : falseValue}; 
				default:
					runtimeError(project, "invalid comparison operator");
					return (struct Value){ .type = VALUE_TYPE_NUMBER, .asNumber = 0 };
			}
		}
	}

	struct Value tmpValue = {
		.type = VALUE_TYPE_ARRAY,
		.asArray.count = 1,
		.asArray.values = (struct ConstValue*)&right, // this is ok here as the tmp value is only compared agains, and not used further.
	};
	return compareArrays(project, left, operator, tmpValue);
}

struct Value compareNumberArray(Project* project, struct Value left, enum ComparisonOperator operator, struct Value right){
	operator = swapCompareOperator(operator);
	return compareArrayNumber(project, right, operator, left);
}

struct Value compareNumbers(Project* project, struct Value left, enum ComparisonOperator operator, struct Value right){
	bool32 value = 0;
	switch(operator){
		case COMPARISON_OPERATOR_EQUALS:
			value = left.asNumber==right.asNumber;
			break;
		case COMPARISON_OPERATOR_NOT_EQUALS:
			value = left.asNumber!=right.asNumber;
			break;
		case COMPARISON_OPERATOR_LESS_THAN:
			value = left.asNumber<right.asNumber;
			break;
		case COMPARISON_OPERATOR_GREATER_THAN:
			value = left.asNumber>right.asNumber;
			break;
		case COMPARISON_OPERATOR_LESS_THAN_OR_EQUAL:
			value = left.asNumber<=right.asNumber;
			break;
		case COMPARISON_OPERATOR_GREATER_THAN_OR_EQUAL:
			value = left.asNumber>=right.asNumber;
			break;
		default:
			runtimeError(project, "invalid comparison operator");
			break;
	}
	return (struct Value){
		.type = VALUE_TYPE_NUMBER,
		.asNumber = value,
	};
}

struct Value compareNumberString(Project* project, struct Value left, enum ComparisonOperator operator, struct Value right){
	runtimeError(project, "invalid comparison types, (string ? number) not allowed");
	return (struct Value){
		.type = VALUE_TYPE_NUMBER,
		.asNumber = 0,
	}; 
}

struct Value compareStringNumber(Project* project, struct Value left, enum ComparisonOperator operator, struct Value right){
	return compareNumberString(project, left, operator, right);
}

struct Value compareStrings(Project* project, struct Value left, enum ComparisonOperator operator, struct Value right){
	if(operator != COMPARISON_OPERATOR_EQUALS && operator != COMPARISON_OPERATOR_NOT_EQUALS){
		runtimeError(project, "invalid comparison operator");
		return (struct Value){
			.type = VALUE_TYPE_NUMBER,
			.asNumber = 0,
		}; 
	}
	bool32 equals = avStringEquals(left.asString, right.asString);
	return (struct Value){
		.type = VALUE_TYPE_NUMBER,
		.asNumber = ((operator == COMPARISON_OPERATOR_EQUALS) ? equals : !equals),
	};
}

bool32 evaluateComparison(Value* value, struct Expression_S expression, Project* ctx){
	struct Value left = {0};
	struct Value right = {0};

	if(!evaluateExpression(&left, *expression.comparison.left, ctx)){
		return false;
	}
	if(!evaluateExpression(&right, *expression.comparison.right, ctx)){
		destroyValue(&left);
		return false;
	}

	if(left.type == VALUE_TYPE_NONE || right.type == VALUE_TYPE_NONE){
		runtimeError(ctx, "comparing null value");
		return false;
	}
	
	if(left.type == VALUE_TYPE_ARRAY){
		switch(right.type){
			case VALUE_TYPE_ARRAY:
				cloneValue(value, compareArrays(ctx, left, expression.comparison.operator, right));
				destroyValue(&left);
				destroyValue(&right);
				return true;
			case VALUE_TYPE_NUMBER:
				cloneValue(value, compareArrayNumber(ctx, left, expression.comparison.operator, right));
				destroyValue(&left);
				destroyValue(&right);
				return true;
			case VALUE_TYPE_STRING:
				cloneValue(value, compareArrayString(ctx, left, expression.comparison.operator, right));
				destroyValue(&left);
				destroyValue(&right);
				return true;
			default: break;
		}
	}
	if(left.type == VALUE_TYPE_NUMBER){
		switch(right.type){
			case VALUE_TYPE_ARRAY:
				cloneValue(value, compareNumberArray(ctx, left, expression.comparison.operator, right));
				destroyValue(&left);
				destroyValue(&right);
				return true;
			case VALUE_TYPE_NUMBER:
				cloneValue(value, compareNumbers(ctx, left, expression.comparison.operator, right));
				destroyValue(&left);
				destroyValue(&right);
				return true;
			case VALUE_TYPE_STRING:
				cloneValue(value, compareNumberString(ctx, left, expression.comparison.operator, right));
				destroyValue(&left);
				destroyValue(&right);
				return true;
			default: break;
		}
	}
	if(left.type == VALUE_TYPE_STRING){
		switch(right.type){
			case VALUE_TYPE_ARRAY:
				cloneValue(value, compareStringArray(ctx, left, expression.comparison.operator, right));
				destroyValue(&left);
				destroyValue(&right);
				return true;
			case VALUE_TYPE_NUMBER:
				cloneValue(value, compareStringNumber(ctx, left, expression.comparison.operator, right));
				destroyValue(&left);
				destroyValue(&right);
				return true;
			case VALUE_TYPE_STRING:
				cloneValue(value, compareStrings(ctx, left, expression.comparison.operator, right));
				destroyValue(&left);
				destroyValue(&right);
				return true;
			default: break;
		}
	}
	runtimeError(ctx, "logic error");
	destroyValue(&left);
	destroyValue(&right);
	return false; 
}

struct Value concatenateStrings(struct Value left, struct Value right, Project* project){
	char buffer[4096] = {0};
	AvString lstr = {0};
	AvString rstr = {0};
	if(left.type == VALUE_TYPE_NUMBER){
		avStringPrintfToBuffer(buffer, 4095, AV_CSTR("%i"), left.asNumber);
		AvString str = AV_CSTR(buffer);
		memcpy(&lstr, &str, sizeof(AvString));
	}else{
		memcpy(&lstr, &left.asString, sizeof(AvString));
	}
	if(right.type == VALUE_TYPE_NUMBER){
		avStringPrintfToBuffer(buffer, 4095, AV_CSTR("%i"), right.asNumber);
		AvString str = AV_CSTR(buffer);
		memcpy(&rstr, &str, sizeof(AvString));
	}else{
		memcpy(&rstr, &right.asString, sizeof(AvString));
	}
	uint64 len = lstr.len + rstr.len;
	AvStringHeapMemory memory;
	avStringMemoryHeapAllocate(len, &memory);
	avStringMemoryStore(lstr, 0, lstr.len, memory);
	avStringMemoryStore(rstr, lstr.len, rstr.len, memory);
	AvString str ={0};
	avStringFromMemory(&str, AV_STRING_WHOLE_MEMORY, memory);
	return (struct Value){
		.type= VALUE_TYPE_STRING,
		.asString = str,
	};
}

struct Value concatenateArray(struct Value left, struct Value right, Project* project){
	uint32 size = (left.type==VALUE_TYPE_ARRAY?left.asArray.count:1) + (right.type==VALUE_TYPE_ARRAY?right.asArray.count:1);
	if(size == 0){
		return (struct Value){
			.type=VALUE_TYPE_NONE,
		};
	}
	struct ConstValue* results = avCallocate(size, sizeof(struct ConstValue), "");
	uint32 index = 0;
	if(left.type == VALUE_TYPE_ARRAY){
		// avMemcpy(results, left.asArray.values, sizeof(struct ConstValue)*left.asArray.count);
		for(uint32 i = 0; i < left.asArray.count; i++){
			cloneConstValue(results + i + index, left.asArray.values[i]);
		}
		index += left.asArray.count;
	}else{
		struct ConstValue res = {0};
		toConstValue(left, &res, project);
		cloneConstValue(results + index, res);
		// memcpy(results, &res, sizeof(struct ConstValue));
		index += 1;
	}
	if(right.type==VALUE_TYPE_ARRAY){
		// memcpy(results+index, right.asArray.values, sizeof(struct ConstValue)*right.asArray.count);
		for(uint32 i = 0; i < right.asArray.count; i++){
			cloneConstValue(results + i + index, right.asArray.values[i]);
		}
		index += right.asArray.count;
	}else{
		struct ConstValue res = {0};
		toConstValue(right, &res, project);
		cloneConstValue(results + index, res);
		//memcpy(results+index, &res, sizeof(struct ConstValue));
		index += 1;
	}
	return (struct Value){
		.type=VALUE_TYPE_ARRAY,
		.asArray = {
			.count = size,
			.values = results,
		},
	};

}

Value performSummation(Value left, enum SummationOperator operator, Value right, Project* project){
	int32 value = 0;
	
	if(left.type != VALUE_TYPE_NUMBER && left.type != VALUE_TYPE_STRING && left.type != VALUE_TYPE_ARRAY){
		runtimeError( project,"add operator not defined for types other than number or string");
		return (struct Value){0};
	}
	if(right.type != VALUE_TYPE_NUMBER && right.type != VALUE_TYPE_STRING && right.type != VALUE_TYPE_ARRAY){
		runtimeError( project,"add operator not defined for types other than number or string");
		return (struct Value){0};
	}

	switch(operator){
		case SUMMATION_OPERATOR_ADD:
			if(left.type == VALUE_TYPE_ARRAY || right.type == VALUE_TYPE_ARRAY){
				return concatenateArray(left, right, project);
			}
			if(left.type == VALUE_TYPE_STRING || right.type == VALUE_TYPE_STRING){
				return concatenateStrings(left, right, project);
			}
			value = left.asNumber + right.asNumber;
		break;
		case SUMMATION_OPERATOR_SUBTRACT:
			if(left.type == VALUE_TYPE_STRING || right.type == VALUE_TYPE_STRING){
				runtimeError(project, "cannot subtract strings");
				return NULL_VALUE;
			}
			if(left.type == VALUE_TYPE_ARRAY || right.type==VALUE_TYPE_ARRAY){
				runtimeError(project, "cannot subtract arrays");
				return NULL_VALUE;
			}
			value = left.asNumber - right.asNumber;
		break;
		case SUMMATION_OPERATOR_NONE:
			avAssert(false, "should not reach here");
			break;
	}
	return (struct Value){
		.type = VALUE_TYPE_NUMBER,
		.asNumber = value,
	};

}

Value performMultiplication(Value left, enum MultiplicationOperator operator, Value right, Project* project){
	uint32 value = 0;
	if(left.type != VALUE_TYPE_NUMBER){
		runtimeError( project,"unary minus operator not defined for types other than number");
		return (struct Value){0};
	}
	if(right.type != VALUE_TYPE_NUMBER){
		runtimeError( project,"unary minus operator not defined for types other than number");
		return (struct Value){0};
	}
	switch(operator){
		case MULTIPLICATION_OPERATOR_MULTIPLY:
			value = left.asNumber * right.asNumber;
		break;
		case MULTIPLICATION_OPERATOR_DIVIDE:
			value = left.asNumber / right.asNumber;
		break;
		case MULTIPLICATION_OPERATOR_NONE:
			avAssert(false, "should not reach here");
			break;
	}
	return (struct Value){
		.type = VALUE_TYPE_NUMBER,
		.asNumber = value,
	};
}

ConstValue* toIterableArray(Value* value, enum ValueType type, uint32* count){
	if(value->type==VALUE_TYPE_ARRAY){
		if(type != VALUE_TYPE_ALL){
			for(uint32 i = 0; i < value->asArray.count; i++){
				if(value->asArray.values[i].type!=type){
					*count = 0;
					return 0;
				}
			}
		}
		*count = value->asArray.count;
		return value->asArray.values;
	}
	if(type != VALUE_TYPE_ALL){
		if(value->type!=type){
			*count = 0;
			return 0; 
		}
	}
	*count = 1;
	return (ConstValue*) value;
}

static void addFilesInPath(AvString directory, AvPathRef root, bool32 recursive, bool32 dirs, AvDynamicArray files, Project* project){
	AvPath path = AV_EMPTY;
   if(!avDirectoryOpen(directory, root, &path)){
		runtimeError(project, "unable to open directory %S", directory);
		return;
	}
	
	for(uint32 i = 0; i <path.contentCount; i++){
		AvPathNode node = path.content[i];
		
		switch(node.type){
			case AV_PATH_NODE_TYPE_FILE:{
				if(dirs){
					break;
				}
				AvString str = {0};
				avStringClone(&str, node.fullName);
				avDynamicArrayAdd(&str, files);
				break;
			}
			case AV_PATH_NODE_TYPE_DIRECTORY:
				if(recursive){
					addFilesInPath(node.name, &path, recursive, dirs, files, project);
				}
				if(dirs){
					AvString str = {0};
					avStringClone(&str, node.fullName);
					avDynamicArrayAdd(&str, files); 
				}
				break;
			case AV_PATH_NODE_TYPE_NONE:
				runtimeError(project, "unknown type file %S", node.name);
				break;
		}
	}
	
	avDirectoryClose(&path);
}

struct Value enumerateFiles(struct EnumerationExpression_S enumeration, Project* project){

	struct Value value = {
		.type = VALUE_TYPE_NONE,
		.asString = AV_EMPTY
	};


	struct Value directory = {0};
	if(!evaluateExpression(&directory, *enumeration.directory, project)){
		return value;
	} 
	struct ConstValue constDirectory = (struct ConstValue){0};

	uint32 directoryCount = 0;
	struct ConstValue* directories = nullptr;
	if(directory.type == VALUE_TYPE_ARRAY){
		directoryCount = directory.asArray.count;
		directories = directory.asArray.values;
	}else{
		struct ConstValue constDir = {0};
		toConstValue(directory, &constDir, project);
		directoryCount = 1;
		memcpy(&constDirectory, &constDir, sizeof(struct ConstValue));
		directories = &constDirectory;
	}
	

	AvDynamicArray files = AV_EMPTY;
	avDynamicArrayCreate(0, sizeof(AvString), &files);

	for(uint32 i = 0; i < directoryCount; i++){
		struct ConstValue dirValue = directories[i];
		if(dirValue.type!=VALUE_TYPE_STRING){
			runtimeError(project, "invalid directory");
			continue;
		}
		AvString dir = dirValue.asString;
		addFilesInPath(dir, nullptr, enumeration.recursive, enumeration.dirs, files, project);
	}

	uint32 fileCount = avDynamicArrayGetSize(files);

	destroyValue(&directory);
	
	if(fileCount == 0){
		goto end;
	}
	if(fileCount == 1){
		value.type = VALUE_TYPE_STRING;
		avDynamicArrayRead(&value.asString, 0, files);
		goto end;
	}
	value.type = VALUE_TYPE_ARRAY;
	value.asArray.count = fileCount;
	value.asArray.values = avAllocate(sizeof(struct ConstValue)*fileCount, "");
	avDynamicArrayReadRange(value.asArray.values, fileCount, offsetof(struct ConstValue,asString), sizeof(struct ConstValue), 0, files);
	avDynamicArrayForEachElement(AvString, files, {
		value.asArray.values[index].type = VALUE_TYPE_STRING;
	});
end:
	avDynamicArrayDestroy(files);
	return value;
}

void avSplitCommandArgs(AvString commandStr, AvDynamicArray args) {
    uint32 i = 0;

    while (i < commandStr.len) {
        // Skip leading whitespace
        while (i < commandStr.len && avCharIsWhiteSpace(commandStr.chrs[i])) i++;

        if (i >= commandStr.len) break;

        // Temporary buffer (worst case: entire remaining string)
        char *bufferStart = (char*)&commandStr.chrs[i];
        char *writePtr = bufferStart;

        bool32 inQuote = false;
        char quoteChar = 0;

        //uint32 startIndex = i;

        while (i < commandStr.len){
            char c = commandStr.chrs[i];

            // Handle escapes
            if (c == '\\') {
                if (i + 1 < commandStr.len) {
                    char next = commandStr.chrs[i + 1];
                    // Allow escaping quotes, backslash, whitespace
                    if (next == '"' || next == '\'' || next == '\\' || avCharIsWhiteSpace(next)) {
                        *writePtr++ = next;
                        i += 2;
                        continue;
                    }
                }
            }

            if (inQuote) {
                if (c == quoteChar) {
                    inQuote = false;
                    i++;
                    continue;
                }

                *writePtr++ = c;
                i++;
            }else {
                if (c == '"' || c == '\''){
                    inQuote = true;
                    quoteChar = c;
                    i++;
                    continue;
                }

                if (avCharIsWhiteSpace(c)) break;

                *writePtr++ = c;
                i++;
            }
        }

        uint32 length = (uint32)(writePtr - bufferStart);

        avDynamicArrayAdd(
            &(AvString){
                .chrs = bufferStart,
                .len  = length,
            },
            args
        );
    }
}

bool32 performAssignment(struct Expression_S expression, Value* value, Project* ctx){
	if(expression.assignment.operator == ASSIGNMENT_OPERATOR_NONE){
		runtimeError(ctx, "invalid operator");
		return false;
	}
	if(expression.assignment.operator != ASSIGNMENT_OPERATOR_ASSIGN){
		Value init = {0};
		if(!retrieveSymbol(expression.assignment.resolvedSymbol, expression.assignment.depth, &init, ctx)){
			return false;
		}
		switch(expression.assignment.operator){
			case ASSIGNMENT_OPERATOR_INCREMENT_ASSIGN:{
				Value tmp = performSummation(init, SUMMATION_OPERATOR_ADD, *value, ctx);
				destroyValue(value);
				cloneValue(value, tmp);
				destroyValue(&tmp);
				break;
			}
			case ASSIGNMENT_OPERATOR_DECREMENT_ASSIGN:{
				Value tmp = performSummation(init, SUMMATION_OPERATOR_SUBTRACT, *value, ctx);
				destroyValue(value);
				cloneValue(value, tmp);
				destroyValue(&tmp);
				break;
			}
			case ASSIGNMENT_OPERATOR_MULTIPLY_ASSIGN:{
				Value tmp = performMultiplication(init, MULTIPLICATION_OPERATOR_MULTIPLY, *value, ctx);
				destroyValue(value);
				cloneValue(value, tmp);
				destroyValue(&tmp);
				break;
			}
			case ASSIGNMENT_OPERATOR_DIVIDE_ASSIGN:{
				Value tmp = performMultiplication(init, MULTIPLICATION_OPERATOR_DIVIDE, *value, ctx);
				destroyValue(value);
				cloneValue(value, tmp);
				destroyValue(&tmp);
				break;
			}
			default:
				break;
		}
		destroyValue(&init);
	}
	uint32 index = -1;
	if(expression.assignment.index){
		Value indexVal = {0};
		if(!evaluateExpression(&indexVal, *expression.assignment.index, ctx)){
			return false;
		}
		if(indexVal.type!=VALUE_TYPE_NUMBER){
			destroyValue(&indexVal);
			runtimeError(ctx, "Index expression does not resolve to number");
			return false;
		}
		if(indexVal.asNumber < 0){
			runtimeError(ctx, "Invalid index value %lli accessing %S", indexVal.asNumber, expression.assignment.variable);
			return false;
		}
		index = indexVal.asNumber;
	}
	return assignSymbol(expression.assignment.resolvedSymbol, expression.assignment.depth, *value, index, ctx);
}

static bool32 getCommandString(struct CommandExpression_S command, Project* ctx, AvStringRef outStr){
	Value commandString = {0};
	if(!evaluateExpression(&commandString, *command.command, ctx)){
		return false;
	}
	if(commandString.type != VALUE_TYPE_STRING){
		runtimeError(ctx, "Command must be of type string");
		return false;
	}
	AvString str = compileString(ctx, 1, &commandString).asString;
	destroyValue(&commandString);
	avStringClone(outStr, str);
	avStringFree(&str);
	return true;
}

static bool32 buildProcessStartInfo(AvString commandStr, AvProcessStartInfo* info){
	AvDynamicArray args;
	avDynamicArrayCreate(0, sizeof(AvString), &args);
	avSplitCommandArgs(commandStr, args);
	uint32 argCount = avDynamicArrayGetSize(args);
	avDynamicArrayMakeContiguous(args);
	AvString* strings = avDynamicArrayGetPageDataPtr(0, args);
	avProcessStartInfoPopulateARR(info, strings[0], (AvString)AV_EMPTY, argCount-1, strings+1);
	avDynamicArrayDestroy(args);
	return true;
}

typedef enum {
    CMD_OUT_RETURN,
    CMD_OUT_PIPE_TO_COMMAND,
    CMD_OUT_FILE_WRITE,
    CMD_OUT_FILE_APPEND
} CommandOutputMode;

static bool32 routeOutput(struct CommandExpression_S command, CommandOutputMode* mode, AvPipe* pipe, AvFile* outputFile, AvFileDescriptor* outFileDescriptor, AvProcessStartInfo* info, Project* ctx){
	if(!command.pipeOutput) {
		*mode = CMD_OUT_RETURN;
	} else if(command.outputType == COMMAND_OUTPUT_TYPE_PIPE) {
		*mode = CMD_OUT_PIPE_TO_COMMAND;
	} else if(command.outputType == COMMAND_OUTPUT_TYPE_APPEND) {
		*mode = CMD_OUT_FILE_APPEND;
	} else {
		*mode = CMD_OUT_FILE_WRITE;
	}

	switch(*mode){
		case CMD_OUT_RETURN:
		case CMD_OUT_PIPE_TO_COMMAND:
			avPipeCreate(pipe);
			info->output = &pipe->write;
			break;
		case CMD_OUT_FILE_WRITE:
		case CMD_OUT_FILE_APPEND:{
			Value outputFileVal = {0};
			if(!evaluateExpression(&outputFileVal, *command.pipeOutput, ctx)){
				return false;
			}
			if(outputFileVal.type!=VALUE_TYPE_STRING){
				runtimeError(ctx, "Output file does not resolve to string");
				return false;
			}
			*outputFile = avFileHandleCreate(outputFileVal.asString);
			if(!avFileOpen(*outputFile, (AvFileOpenOptions){
				.binary = true,
				.openMode = (*mode)==CMD_OUT_FILE_APPEND ? AV_FILE_OPEN_APPEND : AV_FILE_OPEN_WRITE,
				.update = false,
			})){
				runtimeError(ctx, "Failed to open file for writing");
				avFileHandleDestroy(*outputFile);
				return false;
			}
			*outFileDescriptor = avFileGetDescriptor(*outputFile);
			info->output = outFileDescriptor;
			break;
		}
	}
	return true;
}

static void extractLine(AvDynamicArray currentLine, AvDynamicArray strings, AvAllocator* allocator){
	uint32 lineSize = avDynamicArrayGetSize(currentLine);
	if(lineSize==0){
		return;
	}
	char* str = avAllocatorAllocate(lineSize + 1, allocator);
	avDynamicArrayReadRange(str, AV_DYNAMIC_ARRAY_FULL_RANGE, currentLine);
	str[lineSize] = '\0';
	AvString tmp = {
		.chrs = str,
		.len = lineSize,
		.memory = NULL,
	};
	avDynamicArrayAdd(&tmp, strings);
	char c = 0;
	avDynamicArrayClear(&c, currentLine);
}

static Value extractValueFromPipe(AvPipe* pipe, Project* ctx) {
    char buffer[1024];
	const uint32 bufferSize = sizeof(buffer);
	uint32 readSize = 0;
	AvAllocator allocator;
	avAllocatorCreate(0, AV_ALLOCATOR_TYPE_DYNAMIC, &allocator);

	AvDynamicArray currentLine;
	avDynamicArrayCreate(1024, 1, &currentLine);
	avDynamicArraySetGrowSize(1024, currentLine);

	AvDynamicArray strings;
	avDynamicArrayCreate(1, sizeof(AvString), &strings);

	while((readSize = read(pipe->read, buffer, bufferSize))){
		for(uint32 i = 0; i < readSize; i++){
			char c = buffer[i];
			if(avCharIsNewline(c)){
				extractLine(currentLine, strings, &allocator);
			}else{
				avDynamicArrayAdd(&c, currentLine);
			}
		}
	}
	if(avDynamicArrayGetSize(currentLine)!=0){
		extractLine(currentLine, strings, &allocator);
	}

	uint32 lineCount = avDynamicArrayGetSize(strings);
	struct Value returnValue = {0};
	if(lineCount == 0){
		returnValue.type = VALUE_TYPE_STRING;
	}else if(lineCount == 1){
		struct Value val = {.type =VALUE_TYPE_STRING};
		avDynamicArrayRead(&val.asString, 0, strings);
		cloneValue(&returnValue, val);
	}else{
		ConstValue* values = avCallocate(lineCount, sizeof(ConstValue), "");
		for(uint32 i = 0; i < lineCount; i++){
			ConstValue tmp = {.type=VALUE_TYPE_STRING};
			avDynamicArrayRead(&tmp.asString, i, strings);
			cloneConstValue(values + i, tmp);
		}
		returnValue.type = VALUE_TYPE_ARRAY;
		returnValue.asArray = (struct ArrayValue){
			.count = lineCount,
			.values = values,
		};
	}
	avDynamicArrayDestroy(currentLine);
	avDynamicArrayDestroy(strings);
	avAllocatorDestroy(&allocator);
	return returnValue;
}


bool32 extractRetCode(struct CommandExpression_S command, int32 exitCode, Project* ctx){
	if(command.retCode){
		uint32 index = 0;
		struct Expression_S* expr = command.retCode;
		if(expr->type==EXPRESSION_TYPE_INDEX){
			Value indexVal = {0};
			if(!evaluateExpression(&indexVal, *expr->index.index, ctx)){
				return false;
			}
			if(indexVal.type != VALUE_TYPE_NUMBER){
				runtimeError(ctx, "Index does not resolve to number");
				return false;
			}
			index = indexVal.asNumber;
			expr = expr->index.expression;
		}
		if(expr->type==EXPRESSION_TYPE_IDENTIFIER){
			if(!assignSymbol(expr->identifier.resolvedSymbol, expr->identifier.depth, (Value){.type=VALUE_TYPE_NUMBER,.asNumber=exitCode}, index, ctx)){
				return false;
			}
		}else{
			runtimeError(ctx, "Expceted writable value");
			return false;
		}
	}
	return true;
}

Value performCommand(struct CommandExpression_S command, AvPipe* input, Project* ctx){
	Value ret = (Value){0};
	
	
	AvString commandStr = {0};
	if(!getCommandString(command, ctx, &commandStr)){
		return ret;
	}

	if(ctx->options.genCompileCommands){
		addCommandToCompileCommands(commandStr.chrs);
	}
	
	AvProcessStartInfo info = AV_EMPTY;
	if(!buildProcessStartInfo(commandStr, &info)){
		return ret;
	}
	

	if(input) {
		info.input = &input->read;
	}

	CommandOutputMode mode;
	AvFile outputFile = {0};
	AvPipe pipe = {0};
	AvFileDescriptor descriptor;
	if(!routeOutput(command, &mode, &pipe, &outputFile, &descriptor, &info, ctx)){
		goto cleanup;
	}

	AvProcess process = {0};
	if(!avProcessStart(info, &process)){
		runtimeError(ctx, "Failed to start process %S", info.executable);
		goto cleanup;
	}
	if(mode == CMD_OUT_PIPE_TO_COMMAND || mode == CMD_OUT_RETURN) avPipeConsumeWriteChannel(&pipe);
	if(input) avPipeConsumeReadChannel(input);

	switch(mode) {
		case CMD_OUT_PIPE_TO_COMMAND:{
			Value tmp = performCommand(command.pipeOutput->command, &pipe, ctx);
			avMemcpy(&ret, &tmp, sizeof(Value));
			break;
		}
		case CMD_OUT_RETURN:{
			Value tmp = extractValueFromPipe(&pipe, ctx);
			avMemcpy(&ret, &tmp, sizeof(Value));
			break;
		}
		case CMD_OUT_FILE_WRITE:
		case CMD_OUT_FILE_APPEND:	
			break;// Nothing to extract
	}

	int32 exitCode = avProcessWaitExit(process);
	extractRetCode(command, exitCode, ctx);

	if(ctx->options.commandDebug){
		avStringPrintf(AV_CSTRA("%i = %S\n"), exitCode, commandStr);
	}
	
cleanup:
	avStringFree(&commandStr);
	switch(mode){
		case CMD_OUT_PIPE_TO_COMMAND:
		case CMD_OUT_RETURN:
			avPipeDestroy(&pipe);
			break;
		case CMD_OUT_FILE_APPEND:
		case CMD_OUT_FILE_WRITE:
			avFileClose(outputFile);
			avFileHandleDestroy(outputFile);
			break;
	}
	avProcessStartInfoDestroy(&info);
	if(process) avProcessDiscard(process);
	return ret;
	
}



bool32 evaluateCommand(Value* value, struct Expression_S expression, Project* ctx){
	struct CommandExpression_S command = expression.command;
	Value val = performCommand(command, NULL, ctx);
	if(val.type==VALUE_TYPE_NONE){
		return false;
	}
	if(value) avMemcpy(value, &val, sizeof(Value));
	return true;
}

bool32 evaluateExpression(Value* value, struct Expression_S expression, Project* ctx){
	if(value==NULL) {
		runtimeError(ctx, "NULL value specified for expression not of type command");
		return false;
	}
	if(value) avMemset(value, 0, sizeof(Value));
	
	switch (expression.type) {
	case EXPRESSION_TYPE_IDENTIFIER:
		if(!retrieveSymbol(
			expression.identifier.resolvedSymbol, 
			expression.identifier.depth, 
			value, 
			expression.identifier.resolvedSymbol->external ? expression.identifier.resolvedSymbol->scope->project : ctx
		)){
			runtimeError(ctx, "Failed retrieve symbol %S", expression.identifier.resolvedSymbol ? expression.identifier.resolvedSymbol->identifier : AV_CSTRA("<UNDEFINED>"));
			return false;
		}
		// if(value->type == VALUE_TYPE_ARRAY){
		// 	for(uint32 i = 0; i < value->asArray.count; i++){
		// 		if(value->asArray.values[i].type==VALUE_TYPE_STRING && strcmp(value->asArray.values[i].asString.chrs, "build/engine/src/containers/darray.o")==0){
		// 			return true;
		// 		}
		// 	}
		// }
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
	case EXPRESSION_TYPE_COMPARISON:{
		return evaluateComparison(value, expression, ctx);
	}
	case EXPRESSION_TYPE_ARRAY:{
		ConstValue* values = NULL;
		if(expression.array.length) values = avCallocate(expression.array.length, sizeof(ConstValue), "");
		for(uint32 i = 0; i < expression.array.length; i++){
			Value tmp = {0};
			if(!evaluateExpression(&tmp, expression.array.elements[i], ctx)){
				return false;
			}
			ConstValue constTmp;
			toConstValue(tmp, &constTmp, ctx);
			cloneConstValue(values + i, constTmp);
			destroyValue(&tmp);
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
	case EXPRESSION_TYPE_SUMMATION:{
		Value left = {0};
		if(!evaluateExpression(&left, *expression.summation.left, ctx)){
			return false;
		}
		Value right = {0};
		if(!evaluateExpression(&right, *expression.summation.right, ctx)){
			destroyValue(&left);
			return false;
		}
		Value ret = performSummation(left, expression.summation.operator, right, ctx);
		
		destroyValue(&left);
		destroyValue(&right);

		if(ret.type==VALUE_TYPE_NONE){
			return false;
		}
		cloneValue(value, ret);
		destroyValue(&ret);
		return true;
	}
	case EXPRESSION_TYPE_MULTIPLICATION:{
		Value left = {0};
		if(!evaluateExpression(&left, *expression.multiplication.left, ctx)){
			return false;
		}
		Value right = {0};
		if(!evaluateExpression(&right, *expression.multiplication.right, ctx)){
			destroyValue(&left);
			return false;
		}
		Value ret = performMultiplication(left, expression.multiplication.operator, right, ctx);
		destroyValue(&left);
		destroyValue(&right);
		if(ret.type==VALUE_TYPE_NONE){
			return false;
		}
		cloneValue(value, ret);
		destroyValue(&ret);
		return true;
	}
	case EXPRESSION_TYPE_ASSIGNMENT:{
		if(!evaluateExpression(value, *expression.assignment.value, ctx)){
			return false;
		}
		return (performAssignment(expression, value, ctx));
	}
	case EXPRESSION_TYPE_INDEX:{
		Value left = {0};
		if(!evaluateExpression(&left, *expression.index.expression, ctx)){
			return false;
		}
		Value index = {0};
		if(!evaluateExpression(&index, *expression.index.index, ctx)){
			destroyValue(&left);
			return false;
		}
		uint32 size = 1;
		ConstValue* values = toIterableArray(&left, VALUE_TYPE_ALL, &size);
	
		uint32 indexSize = 1;
		ConstValue* indices = toIterableArray(&index, VALUE_TYPE_NUMBER, &indexSize);
		if(indices==0){
			runtimeError(ctx, "index with invalid value type");
			destroyValue(&left);
			destroyValue(&index);
			return false;
		}

		ConstValue* newValues = avCallocate(indexSize, sizeof(ConstValue), "");
		for(uint32 i = 0; i < indexSize; i++){
			int64 indexNr = indices[i].asNumber;
			if(indexNr < 0 || indexNr >= size){
				runtimeError(ctx, "array index out of bounds");
				destroyValue(&left);
				destroyValue(&index);
				return false;
			}
			cloneConstValue(newValues + i, values[indexNr]);
		}
		if(indexSize == 1){
			Value tmp = {0};
			toValue(newValues[0], &tmp);
			avFree(newValues);
			cloneValue(value, tmp);
			destroyValue(&tmp);
		}else{
			Value val = {
				.type= VALUE_TYPE_ARRAY,
				.asArray.count = indexSize,
				.asArray.values = newValues,
			};
			cloneValue(value, val);
			destroyValue(&val);
		}
		destroyValue(&left);
		destroyValue(&index);

		return true;
	}
	case EXPRESSION_TYPE_UNARY:{
		Value val = {0};
		if(!evaluateExpression(&val, *expression.unary.expression, ctx)){
			return false;
		}
		int64 numericVal = 0;
		enum ValueType type = val.type;
		if(val.type!=VALUE_TYPE_NUMBER){
			numericVal = checkValueTrue(val);
		}else{
			numericVal = val.asNumber;
		}
		destroyValue(&val);
		
		int64 newVal = 0;
		switch(expression.unary.operator){
			case UNARY_OPERATOR_MINUS:
				if(type != VALUE_TYPE_NUMBER){
					runtimeError(ctx, "Invalid type for '-' operator");
					return false;
				}
				newVal = -numericVal;
				break;
			case UNARY_OPERATOR_PLUS:
				if(type != VALUE_TYPE_NUMBER){
					runtimeError(ctx, "Invalid type for '-' operator");
					return false;
				}
				newVal = numericVal;
				break;
			case UNARY_OPERATOR_NOT:
				newVal = !numericVal;
				break;
			default:
				runtimeError(ctx, "Invalid operator");
				break;
		}
		Value tmp = {
			.type = VALUE_TYPE_NUMBER,
			.asNumber = newVal,
		};
		cloneValue(value, tmp);
		return true;
	}
	case EXPRESSION_TYPE_COMBINATION:{
		Value left = {0};
		if(!evaluateExpression(&left, *expression.combination.left, ctx)){
			return false;
		}
		bool32 leftVal = checkValueTrue(left);
		switch(expression.combination.operator){
			case COMBINATION_OPERATOR_AND:
				if(!leftVal){
					cloneValue(value, (Value){.type=VALUE_TYPE_NUMBER, .asNumber = 0});
					return true;
				}
				break;
			case COMBINATION_OPERATOR_OR:
				if(leftVal){
					cloneValue(value, (Value){.type=VALUE_TYPE_NUMBER, .asNumber = 1});
					return true;
				}
				break;
			default:
				runtimeError(ctx, "Invalid operator");
				return false;
		}
		destroyValue(&left);
		Value right = {0};
		if(!evaluateExpression(&right, *expression.combination.right, ctx)){
			return false;
		}
		bool32 rightVal = checkValueTrue(right);
		destroyValue(&right);
		cloneValue(value, (Value){.type=VALUE_TYPE_NUMBER, .asNumber = rightVal});
		return true;
	}
	case EXPRESSION_TYPE_ENUMERATION:{
		Value val = enumerateFiles(expression.enumeration, ctx);
		if(val.type == VALUE_TYPE_NONE){
			destroyValue(&val);
			return false;
		}
		cloneValue(value, val);
		destroyValue(&val);
		return true;
	}
	case EXPRESSION_TYPE_COMMAND:
		return evaluateCommand(value, expression, ctx);
	case EXPRESSION_TYPE_TERNARY:{
		Value testVal = {0};
		if(!evaluateExpression(&testVal, *expression.ternary.expr, ctx)){
			return false;
		}
		bool32 path = checkValueTrue(testVal);
		destroyValue(&testVal);
		if(!evaluateExpression(value, (path) ? (*expression.ternary.truePath) : (*expression.ternary.falsePath), ctx)){
			return false;
		}
		return true;
	}
		
	default:
		runtimeError(ctx, "Expression type not implemented yet");
		return false;
	}



	return true;
}

bool32 evaluateForeach(struct Statement_S statement, Project* ctx){
	Value collection = {0};
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
		case VALUE_TYPE_NONE:
			destroyValue(&collection);
			return true; // Do nothing and exit loop
		default:
			runtimeError(ctx, "Invalid collection type");
			destroyValue(&collection);
			return false;
	}

	enterStackFrame(statement.attachedScope, ctx);

	for(uint32 i = 0; i < count; i++){
		Value value = {0};
		toValue(values[i], &value);
		if(!assignSymbol(statement.foreachStatement.resolvedVarSymbol, 0, value, 0, ctx)){
			exitStackFrame(ctx);
			destroyValue(&collection);
			return false;
		}
		if(statement.foreachStatement.resolvedIndexSymbol){
			if(!assignSymbol(statement.foreachStatement.resolvedIndexSymbol, 0, (Value){.type=VALUE_TYPE_NUMBER, .asNumber=i}, 0, ctx)){
				exitStackFrame(ctx);
				destroyValue(&collection);
				return false;
			}
		}
		if(!evaluateStatement(statement.foreachStatement.statement, ctx)){
			destroyValue(&collection);
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
	destroyValue(&collection);
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
		if(mapping.resolvedExternal==NULL){
			avStringPrintln(AV_CSTRA("Error"));
			avStringPrintln(mapping.alias);
			avStringPrintln(mapping.symbol);
		}
		if(mapping.resolvedExternal->type!=SYMBOL_VARIABLE){
			continue;
		}
		Value value = {0};
		if(!retrieveSymbol(mapping.resolvedExternal, 0, &value, import.project)){
			return false;
		}
		if(!assignSymbol(mapping.resolvedSymbol, 0, value, 0, ctx)){
			return false;
		}
	}
	return true;
}

bool32 evaluateStatement(struct Statement_S* statement, Project* ctx){
	bool32 ret = true;
    ctx->currentLine = statement->line;
	if(statement->attachedScope) enterStackFrame(statement->attachedScope, ctx);
	switch(statement->type){
		case STATEMENT_TYPE_BLOCK:
			
			for(uint32 i = 0; i < statement->block.statementCount; i++){
				if(!evaluateStatement(&statement->block.statements[i], ctx)){
					exitStackFrame(ctx);
					ret = false;
					break;
				}
				if(ctx->controlFlow!=CONTROLFLOW_NORMAL){
					break;
				}
			}
			break;
		case STATEMENT_TYPE_EXPRESSION:{
			Value tmp ={0};
			ret = evaluateExpression(&tmp, statement->expression, ctx);
			destroyValue(&tmp);
			break;
		}
		case STATEMENT_TYPE_IF:{
			Value value = {0};
			if(!evaluateExpression(&value, *statement->ifStatement.check, ctx)){
				ret = false;
				break;
			}
			if(checkValueTrue(value)){
				if(!evaluateStatement(statement->ifStatement.branch, ctx)){
					destroyValue(&value);
					ret = false;
					break;
				}
			}else if(statement->ifStatement.alternativeBranch) {
				if(!evaluateStatement(statement->ifStatement.alternativeBranch, ctx)){
					destroyValue(&value);
					ret = false;
					break;
				}
			}
			destroyValue(&value);
			break;
		}
		case STATEMENT_TYPE_FOREACH:
			ret = evaluateForeach(*statement, ctx);
			break;
		case STATEMENT_TYPE_BREAK:
			ctx->controlFlow = CONTROLFLOW_BREAK;
			break;
		case STATEMENT_TYPE_CONTINUE:
			ctx->controlFlow = CONTROLFLOW_CONTINUE;
			break;
		case STATEMENT_TYPE_RETURN:{
			if(statement->returnStatement.value){
				Value value = {0};
				if(!evaluateExpression(&value, *statement->returnStatement.value, ctx)){
					ret = false;
					break;
				}
				if(!assignReturnValue(statement->returnStatement.resolvedVirtualSymbol, statement->returnStatement.returnDepth, value, ctx)){
					destroyValue(&value);
					ret = false;
					break;
				}
				destroyValue(&value);
			}
			ctx->controlFlow = CONTROLFLOW_RETURN;
			break;
		}
		case STATEMENT_TYPE_VARIABLE_DEFINITION:{
			uint32 size = 1;
			bool32 isArray = false;
			if(statement->variableDefinition.size &&  statement->variableDefinition.size->type != EXPRESSION_TYPE_NONE){
				Value value = {0};
				if(!evaluateExpression(&value, *statement->variableDefinition.size, ctx)){
					ret = false;
					break;
				}
				if(value.type != VALUE_TYPE_NUMBER){
					destroyValue(&value);
					runtimeError(ctx, "Variable %S's size does not resolve to number", statement->variableDefinition.identifier);
					ret = false;
					break;
				}
				if(value.asNumber < 1){
					runtimeError(ctx, "Variable %S's size is not a valid number", statement->variableDefinition.identifier);
					ret = false;
					break;
				}
				size = value.asNumber;
				isArray = true;
				
			}else if(statement->variableDefinition.size && statement->variableDefinition.size->type==EXPRESSION_TYPE_NONE){
				size = 0;
				isArray = true;
			}

			if(statement->variableDefinition.initialValue.type != EXPRESSION_TYPE_NONE){
				Value value = {0};
				if(!evaluateExpression(&value, statement->variableDefinition.initialValue, ctx)){
					ret = false;
					break;
				}

				if(isArray){
					if(value.type!=VALUE_TYPE_ARRAY){
						ConstValue* values;
						if(size <= 1){
							values = avAllocate(sizeof(ConstValue), "");
							toConstValue(value, values, ctx);
							Value tmp = {.type=VALUE_TYPE_ARRAY, .asArray.count = 1,.asArray.values = values};
							destroyValue(&value);
							avMemcpy(&value, &tmp, sizeof(Value));
						}else{
							runtimeError(ctx, "Initial value is not correct size");
							ret = false;
							break;
						}
					}else{
						if(size != 0){
							if(value.asArray.count != size){
								runtimeError(ctx, "Initial value is not correct size");
								ret = false;
								break;
							}
						}
					}
				}

				if(!assignSymbol(statement->variableDefinition.resolvedSymbol, 0, value, -1, ctx)){
					destroyValue(&value);
					ret = false;
					break;
				}
				destroyValue(&value);
			}else{
				// do size
				if(isArray){
					Value value = {.type=VALUE_TYPE_ARRAY,.asArray.count = size, .asArray.values = NULL};
					if(size){
						value.asArray.values = avAllocate(sizeof(ConstValue)*size, "");
						avMemset(value.asArray.values, 0, sizeof(ConstValue)*size);
					}
					if(!assignSymbol(statement->variableDefinition.resolvedSymbol, 0, value, 0, ctx)){
						ret = false;
						break;
					}
					destroyValue(&value);
				}
			}
			break;
		}
		case STATEMENT_TYPE_INHERIT:
			if(!statement->inheritStatement.resolvedSymbol->external && statement->inheritStatement.defaultValue){
				Value val = {0};
				if(!evaluateExpression(&val, *statement->inheritStatement.defaultValue, ctx)){
					ret = false;
					break;
				}
				if(!assignSymbol(statement->inheritStatement.resolvedSymbol, 0, val, 0, ctx)){
					destroyValue(&val);
					ret = false;
					break;
				}
				destroyValue(&val);
				break;
			}else if(!statement->inheritStatement.resolvedSymbol->external && !statement->inheritStatement.defaultValue){
				runtimeError(ctx, "inherit  default valuenot %S not specified", statement->inheritStatement.variable);
				ret = false;
				break;
			}else if(statement->inheritStatement.resolvedSymbol->external){
				// statement->inheritStatement.resolvedSymbol = statement->inheritStatement.externalSymbol;
				break;
			}else{
				runtimeError(ctx, "Logic Error");
				ret = false;
				break;
			}
			runtimeError(ctx, "Logic Error");
			ret = false;
			break;
		case STATEMENT_TYPE_IMPORT:
			ret = evaluateImport(*statement, ctx);
			break;
		default:	
			runtimeError(ctx, "Not implemented yet");
			ret = false;
			break;
	}
	if(statement->attachedScope) exitStackFrame(ctx);
	return ret;
}


void exitAllImportedProjectStackFrames(Project* project);
void exitProject(Project* project){
	while(project->currentStackFrame){
		exitStackFrame(project);
	}
	exitAllImportedProjectStackFrames(project);
}

void exitAllImportedProjectStackFrames(Project* project){
	for(uint32 index = 0; index < avDynamicArrayGetSize(project->importedProjects); index++) { 
		Project** element = avDynamicArrayGetPtr(index, project->importedProjects); 
		exitProject(*element);
	};
}
bool32 initAllImportedProjects(Project* project);
bool32 initProject(Project* project){
	enterStackFrame(project->toplevelScope, project);
	bool32 ret = true;
	for(uint32 i = 0; i < project->statementCount; i++){
		struct Statement_S* statement = project->statements+i;
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
	if(ret==false){
		exitStackFrame(project);
		return false;
	}

	if(!initAllImportedProjects(project)){
		return false;
	}
	return true;
}

bool32 initAllImportedProjects(Project* project){
	for(uint32 index = 0; index < avDynamicArrayGetSize(project->importedProjects); index++) { 
		Project** element = avDynamicArrayGetPtr(index, project->importedProjects); 
		if(!initProject(*element)){
			while(index-- != 0){
				exitStackFrame(*element);
				element = avDynamicArrayGetPtr(index, project->importedProjects); 
			}
			return false;
		}
	};
	return true;
}

uint32 runProject(Project* project, AvDynamicArray arguments){
	project->controlFlow = CONTROLFLOW_NORMAL;
	AvString functionEntryName = AV_CSTRA("main");
	AvString* entry = &functionEntryName;
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
	
	if(!initProject(project)){
		runtimeError(project, "Initializing imported projects failed");
		return -1;
	}

	bool32 ret = true;

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
			if(!assignSymbol(sym, 0, val, 0, project)){
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
			if(!assignSymbol(sym, 0, val, 0, project)){
				avFree(val.asArray.values);
				ret = false;
				break;
			}
			avFree(val.asArray.values);
		}

	}
	if(ret == false){
		exitProject(project);
		return -1;
	}

	// we are now ready to rumble
	if(!evaluateStatement(func.body, project)){
		exitProject(project);
		runtimeError(project, "Error occured during execution!");
		return false;
	}
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
	
	exitProject(project);
	
	return retVal;
}
