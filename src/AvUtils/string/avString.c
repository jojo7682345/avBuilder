#include <AvUtils/avString.h>
#include <AvUtils/avMemory.h>
#include <AvUtils/avMath.h>
#define AV_DYNAMIC_ARRAY_EXPOSE_MEMORY_LAYOUT
#include <AvUtils/dataStructures/avDynamicArray.h>
#include <AvUtils/string/avChar.h>
#include <AvUtils/avLogging.h>

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#define NULL_TERMINATOR_SIZE 1

typedef struct StringDebugContext_T* StringDebugContext;
typedef struct StringDebugContext_T {
	StringDebugContext prev;
	AvDynamicArray allocatedMemory;
} StringDebugContext_T;
static StringDebugContext debugContext;

uint64 avCStringLength(const char* str) {
	uint64 length = 0;
	while (str[length++]) {} // loop until the termination character
	return length - 1; // do not include the termination character
}

void avStringClone(AvStringRef dst, AvString src) {
	AvStringHeapMemory memory;
	avStringMemoryHeapAllocate(src.len, &memory);
	avStringMemoryStore(src, 0, AV_STRING_FULL_LENGTH, memory);
	if(dst->memory){
		avStringFree(dst);	
	}
	avStringFromMemory(dst, 0, AV_STRING_FULL_LENGTH, memory);
}

void avStringCopy(AvStringRef dst, AvString src) {
	avStringCopySection(dst, 0, AV_STRING_FULL_LENGTH, src);
}

void avStringCopySection(AvStringRef dst, uint64 offset, uint64 length, AvString src) {
	avAssert(dst != nullptr, "dst must be a valid string reference");

	if (offset >= src.len) {
		avAssert(offset >= src.len, "offset is greater than source length resulting in an empty string");
		return;
	}

	if (length == AV_STRING_FULL_LENGTH) {
		length = src.len;
	}
	length = AV_MIN(src.len, length + offset) - offset;

	if (src.memory) {
		uint64 off = src.chrs - src.memory->data + offset;
		avStringFromMemory(dst, off, length, src.memory);
		return;
	}

	AvString tmp = {
		.chrs = src.chrs + offset,
		.len = length,
		.memory = nullptr
	};
	memcpy(dst, &tmp, sizeof(AvString));

}

void avStringMove(AvStringRef dst, AvStringRef src) {
	avAssert(dst != nullptr, "destination must be a valid reference");
	avAssert(src != nullptr, "source must be a valid reference");

	if (dst->memory == nullptr) {
		avStringClone(dst, *src);
	} else {
		avStringMemoryStore(*src, dst->chrs - dst->memory->data, dst->len, dst->memory);
	}
	avStringFree(src);
}

void avStringResize(AvStringRef str, uint64 newLength) {
	avAssert(str != nullptr, "str must be a valid reference");
	avAssert(str->memory != nullptr, "a constant string cannot be resized");

	if (newLength == AV_STRING_FULL_LENGTH) {
		newLength = str->memory->capacity;
	}

	uint64 offset = str->chrs - str->memory->data;
	newLength = AV_MIN(newLength, str->memory->capacity - offset);
	memcpy((uint64*)&str->len, &newLength, sizeof(str->len));
}

void avStringShift(AvStringRef str, uint64 newOffset) {
	avAssert(str != nullptr, "str must be a valid reference");
	avAssert(str->memory != nullptr, "a constant string cannot be resized");
	if (newOffset < str->memory->capacity) {
		avAssert(newOffset < str->memory->capacity, "offset must not fall outside the capacity");
		return;
	}

	uint64 newLength = AV_MIN(newOffset + str->len, str->memory->capacity) - newOffset;
	avStringResize(str, newLength);
	char* newChrs = str->memory->data + newOffset;
	memcpy((char*)&str->chrs, &newChrs, sizeof(str->chrs));
}

void avStringFromMemory(AvStringRef dst, uint64 offset, uint64 length, AvStringMemoryRef memory) {
	avAssert(memory != 0, "memory must be valid");
	avAssert(dst != 0, "destination must be a valid reference");

	if (dst->memory && dst->memory != memory) {
		avAssert(dst->memory == nullptr, "destination already has backed memory. This will cause undefined behaviour. If this is not the case, zero out the string first");
		return;
	}

	if (offset >= memory->capacity) {
		avAssert(offset >= memory->capacity, "offset is greater than capacity resulting in an empty string");
		return;
	}

	if (length == AV_STRING_FULL_LENGTH) {
		length = memory->capacity;
	}
	length = AV_MIN(memory->capacity, length + offset) - offset;

	AvString result = {
		.chrs = memory->data + offset,
		.len = length,
		.memory = memory
	};

	memory->referenceCount++;

	memcpy(dst, &result, sizeof(AvString));
}

void avStringMemoryClone(AvStringMemoryRef* dst, AvStringMemory src) {
	avAssert(dst != nullptr, "destination must be a valid handle");
	avAssert(*dst == nullptr, "destination must be empty memory");

	avStringMemoryHeapAllocate(src.capacity, dst);
	avStringMemoryStore(AV_STR(src.data, src.capacity), 0, src.capacity, *dst);
}

static void stringMemoryRemoveReference(AvStringMemoryRef memory) {
	avAssert(memory != nullptr, "memory must be a valid reference");
	avAssert(memory->referenceCount > 0, "freeing while no more references should remain");

	memory->referenceCount--;

	if (memory->referenceCount == 0) {
		avStringMemoryFree(memory);
	}
}

void avStringFree(AvStringRef str) {

	avAssert(str != nullptr, "string must be a valid reference");
	if (str->memory == nullptr) {
		return;
	}
	//printf("string { chrs = %.*s, len = %lu, memory = %p }\n", str->len, str->chrs, str->len, str->memory);
	AvStringMemoryRef memory = str->memory;
	avAssert(memory->referenceCount > 0, "string memory references are corrupt");

	stringMemoryRemoveReference(memory);
	memset(str, 0, sizeof(AvString));
}

void avStringMemoryHeapAllocate(uint64 capacity, AvStringHeapMemory* memory) {
	avAssert(memory != nullptr, "invalid heap memory pointer");
	(*memory) = avCallocate(1, sizeof(AvStringMemory), "allocating string memory on heap");
	(*memory)->properties.heapAllocated = true;
	avStringMemoryAllocate(capacity, *memory);
}

void avStringMemoryAllocate(uint64 capacity, AvStringMemoryRef memory) {
	avAssert(memory != nullptr, "invalid memory reference");
	avAssert(memory->data == nullptr, "string memory already allocated");
	avAssert(memory->capacity == 0, "string memory already allocated");
	avAssert(memory->referenceCount == 0, "string memory already allocated");
	avAssert(capacity != 0, "capacity must be >1");

	memory->capacity = capacity;
	memory->data = avCallocate(capacity + NULL_TERMINATOR_SIZE, 1, "allocating string data");
	memory->referenceCount = 0;

#ifndef NDEBUG
	if (debugContext) {
		if(!memory->properties.heapAllocated){
			memory->properties.contextAllocationIndex = avDynamicArrayAdd(&memory, debugContext->allocatedMemory);
			memory->properties.debugContext = debugContext;
		}else if (debugContext->prev) {
			memory->properties.contextAllocationIndex = avDynamicArrayAdd(&memory, debugContext->prev->allocatedMemory);
			memory->properties.debugContext = debugContext->prev;
		}
	}
#endif

}

// void avStringMemoryResize(uint64 capacity, AvStringMemoryRef memory) {
// 	avAssert(memory != nullptr, "invalid memory reference");
// 	avAssert(capacity, "resize to 0 size is not allowed");
// 	avAssert(memory->data != nullptr, "resizing unallocated memory is not allowed");
// 	avAssert(memory->capacity, "resizing unallocated memory is not allowed");

// 	char* prevData = memory->data;
// 	uint64 prevCapacity = memory->capacity;
// 	memory->data = avReallocate(memory->data, capacity + NULL_TERMINATOR_SIZE, "reallocating string data");
// 	if (capacity > prevCapacity) {
// 		uint64 diffCapacity = capacity - prevCapacity;
// 		memset(memory->data + prevCapacity, 0, diffCapacity + NULL_TERMINATOR_SIZE);
// 	}
// 	memory->capacity = capacity;

// 	// update references to new data
// 	for (uint32 i = 0; i < memory->referenceCount; i++) {
// 		AvStringRef str = memory->references[i];
// 		avAssert(str != nullptr, "references are corrupted");
// 		avAssert(str->memory == memory, "string must be allocated from this memory");

// 		uint64 offset = str->chrs - prevData;
// 		uint64 length = AV_MIN(str->len + offset, capacity) - offset;

// 		if (offset >= capacity || length == 0) {
// 			stringMemoryRemoveReference(i, memory);
// 			memset(str, 0, sizeof(AvString));
// 			i--;
// 			continue;
// 		}

// 		AvString tmpStr = {
// 			.chrs = memory->data + offset,
// 			.len = length,
// 			.memory = memory
// 		};
// 		memcpy(str, &tmpStr, sizeof(AvString));
// 	}

// }

// void avStringMemoryResizeSection(uint64 offset, uint64 length, AvStringMemoryRef memory) {
// 	avAssert(memory != nullptr, "invalid memory reference");

// 	uint64 newSize = AV_MAX(offset + length, memory->capacity);
// 	avStringMemoryResize(newSize, memory);
// }

void avStringMemoryFree(AvStringMemoryRef memory) {
	avAssert(memory != nullptr, "invalid memory reference");
	avAssert(memory->referenceCount == 0, "freeing string memory still in use");

#ifndef NDEBUG
	if (memory->properties.debugContext) {
		avDynamicArrayRemove(memory->properties.contextAllocationIndex, ((StringDebugContext)memory->properties.debugContext)->allocatedMemory);
		for (
			uint32 i = memory->properties.contextAllocationIndex; 
			i < avDynamicArrayGetSize(((StringDebugContext)memory->properties.debugContext)->allocatedMemory); 
			i++
		) {
			AvStringMemoryRef tmp;
			avDynamicArrayRead(&tmp, i, ((StringDebugContext)memory->properties.debugContext)->allocatedMemory);
			tmp->properties.contextAllocationIndex--;
			avDynamicArrayWrite(&tmp, i, ((StringDebugContext)memory->properties.debugContext)->allocatedMemory);
		}
	}
#endif

	if (memory->data) {
		avFree(memory->data);
		memory->data = nullptr;
	}

	if (memory->properties.heapAllocated) {
		avFree(memory);
	}
}

void avStringMemoryStore(AvString str, uint64 offset, uint64 length, AvStringMemoryRef memory) {
	avAssert(memory != nullptr, "memory must be a valid reference");
	avAssert(memory->data != nullptr, "memory has not been allocated");
	avAssert(memory->capacity != 0, "memory has not been allocated");
	if (offset >= memory->capacity) {
		avAssert(offset >= memory->capacity, "offset is greater than capacity resulting in an empty string");
		return;
	}
	if (str.len == 0) {
		return;
	}
	if (length == AV_STRING_FULL_LENGTH) {
		length = str.len;
	}
	length = AV_MIN(memory->capacity, length + offset) - offset; // cap length to memory capacity
	length = AV_MIN(length, str.len);
	avAssert(str.chrs != nullptr, "string must be valid if str.len > 0");
	memcpy(memory->data + offset, str.chrs, length);
}

void avStringMemoryAllocStore(AvString str, AvStringMemoryRef memory) {
	avAssert(memory != nullptr, "memory must be a valid reference");
	avStringMemoryAllocate(str.len, memory);
	avStringMemoryStore(str, 0, AV_STRING_FULL_LENGTH, memory);
}

void avStringDebugContextStart_() {

	StringDebugContext context = avCallocate(1, sizeof(StringDebugContext_T), "allocating string debug context");
	context->prev = debugContext;
	avDynamicArrayCreate(0, sizeof(AvStringMemoryRef), &context->allocatedMemory);
	avDynamicArraySetAllowRelocation(true, context->allocatedMemory);
	debugContext = context;

}

void avStringDebugContextEnd_() {
	uint unfreedMemory = avDynamicArrayGetSize(debugContext->allocatedMemory);
	for (uint i = 0; i < unfreedMemory; i++) {
		AvStringMemoryRef stringMemory;
		avDynamicArrayRead(&stringMemory, 0, debugContext->allocatedMemory);

		//TODO: better log
		avStringPrintf(AV_CSTR("allocated string memory containing \"%s\" has not been freed: %u remaining references\n"), AV_STR(stringMemory->data, stringMemory->capacity), stringMemory->referenceCount);
	}
	avDynamicArrayDestroy(debugContext->allocatedMemory);


	StringDebugContext nextContext = debugContext->prev;
	avFree(debugContext);
	debugContext = nextContext;
}

strOffset avStringFindLastOccuranceOfChar(AvString str, char chr) {
	for (uint64 i = str.len; i > 0; i--) {
		if (str.chrs[i - 1] == chr) {
			return i - 1;
		}
	}
	return AV_STRING_NULL;
}

strOffset avStringFindFirstOccranceOfChar(AvString str, char chr) {
	for (uint64 i = 0; i < str.len; i++) {
		if (str.chrs[i] == chr) {
			return i;
		}
	}
	return AV_STRING_NULL;
}

bool32 avStringContainsChar(AvString str, char chr) {
	return (avStringFindCharCount(str, chr) != 0);
}

bool32 avStringContains(AvString str, AvString sequence) {
	return avStringFindCount(str, sequence) != 0;
}

uint64 avStringFindCharCount(AvString str, char chr) {
	uint64 count = 0;
	for (uint64 i = 0; i < str.len; i++) {
		if (str.chrs[i] == chr) {
			count++;
		}
	}
	return count;
}

strOffset avStringFindLastOccuranceOf(AvString str, AvString find) {
	if (str.len < find.len) {
		return AV_STRING_NULL;
	}

	for (uint64 i = str.len - find.len; i > 0; i--) {
		uint64 j = 0;
		for (; j < find.len; j++) {
			if (str.chrs[i - 1 + j] != find.chrs[j]) {
				break;
			}
		}
		if (j == find.len) {
			return i;
		}
	}
	return AV_STRING_NULL;
}

strOffset avStringFindFirstOccuranceOf(AvString str, AvString find) {
	if (str.len < find.len) {
		return AV_STRING_NULL;
	}
	for (uint64 i = 0; i < str.len; i++) {
		bool32 match = true;
		for(uint64 j = i; j < AV_MIN(str.len, i+find.len); j++){
			if(str.chrs[j]!=find.chrs[j-i]){
				match = false;
				break;
			}
		}
		if(match){
			return i;
		}
	}
	return AV_STRING_NULL;
}

uint64 avStringFindCount(AvString str, AvString find) {
	uint64 strOffset = 0;
	if (str.len < find.len) {
		return 0;
	}
	uint64 count = 0;
	for (uint64 i = 0; i < str.len; i++) {
		if (str.chrs[i] == find.chrs[strOffset]) {
			strOffset++;
			if (strOffset == find.len) {
				count++;
				strOffset = 0;
			}
		} else {
			strOffset = 0;
		}
	}
	return count;
}

bool32 avStringEndsWith(AvString str, AvString sequence) {
	if(str.len < sequence.len){
		return false;
	}
	uint64 offset = str.len - sequence.len;
	for(uint64 i = offset; i < str.len; i++){
		if(str.chrs[i] != sequence.chrs[i-offset]){
			return false;
		}
	}
	return true;
}

bool32 avStringStartsWith(AvString str, AvString sequence) {
	if(str.len < sequence.len){
		return false;
	}
	for(uint64 i = 0; i < sequence.len; i++){
		if(str.chrs[i] != sequence.chrs[i]){
			return false;
		}
	}
	return true;
}

bool32 avStringEndsWithChar(AvString str, char chr) {
	if (str.len == 0 || str.chrs == nullptr) {
		return false;
	}
	return str.chrs[str.len - 1] == chr;
}

bool32 avStringStartsWithChar(AvString str, char chr) {
	if (str.len == 0 || str.chrs == nullptr) {
		return false;
	}
	return str.chrs[0] == chr;
}

bool32 avStringWrite(AvStringRef str, strOffset offset, char chr) {
	avAssert(str != nullptr, "string must be a valid reference");
	if (offset >= str->len) {
		return false;
	}
	if (str->memory == nullptr) {
		avStringClone(str, *str);
	}
	str->memory->data[offset] = chr;
	return true;
}


char avStringRead(AvString str, strOffset offset) {
	if (str.len == 0) {
		return '\0';
	}
	avAssert(str.chrs != nullptr, "string is malformed");
	if (offset >= str.len) {
		return '\0';
	}
	return str.chrs[offset];
}


void avStringToUppercase(AvStringRef str) {
	avAssert(str != nullptr, "string must be a valid reference");
	if (str->memory == nullptr) {
		avStringClone(str, *str);
	}
	uint64 offset = str->chrs - str->memory->data;
	for (uint i = offset; i < AV_MIN(str->len + offset, str->memory->capacity); i++) {
		str->memory->data[i] = avCharToUppercase(str->memory->data[i]);
	}
}
void avStringToLowercase(AvStringRef str) {
	avAssert(str != nullptr, "string must be a valid reference");
	if (str->memory == nullptr) {
		avStringClone(str, *str);
	}
	uint64 offset = str->chrs - str->memory->data;
	for (uint i = offset; i < AV_MIN(str->len + offset, str->memory->capacity); i++) {
		str->memory->data[i] = avCharToLowercase(str->memory->data[i]);
	}
}

bool32 avStringEquals(AvString strA, AvString strB) {
	if (strA.len != strB.len) {
		return false;
	}
	for (uint64 i = 0; i < strA.len; i++) {
		if (strA.chrs[i] != strB.chrs[i]) {
			return false;
		}
	}
	return true;
}

bool32 avStringEqualsCaseInsensitive(AvString strA, AvString strB) {
	if (strA.len != strB.len) {
		return false;
	}
	for (uint64 i = 0; i < strA.len; i++) {
		if (!avCharEqualsCaseInsensitive(strA.chrs[i], strB.chrs[i])) {
			return false;
		}
	}
	return true;
}

int32 avStringCompare(AvString strA, AvString strB) {

	for (uint64 i = 0; i < AV_MIN(strA.len, strB.len); i++) {
		if (strA.chrs[i] == strB.chrs[i]) {
			continue;
		}
		return (strA.chrs[i] < strB.chrs[i]) ? -1 : 1;
	}

	if (strA.len == strB.len) {
		return 0;
	}

	return strA.len < strB.len ? 0 : 1;

}

void avStringReplaceChar(AvStringRef str, char original, char replacement) {
	if (str->memory == nullptr) {
		avStringClone(str, *str);
	}

	for (uint64 i = 0; i < str->len; i++) {
		if (str->memory->data[i] == original) {
			str->memory->data[i] = replacement;
		}
	}
}

uint64 avStringReplaceAll(AvStringRef dst, AvString str, uint32 count, uint64 stride, AvString* sequences, AvString* replacements){
	avAssert(dst != nullptr, "destination must be a valid reference");
	avAssert(sequences != nullptr, "sequences must be a valid array");
	avAssert(replacements != nullptr, "replacements must be a valid array");

	uint64* counts = avCallocate(count, sizeof(uint64), "counts");
	uint64 totalCount = 0;
	for(uint32 i = 0; i < count; i++){
		counts[i] = avStringFindCount(str, *((AvString*)((byte*)sequences+(i*stride))));
		totalCount += counts[i];
	}

	if(totalCount==0){
		avStringClone(dst, str);
		return 0;
	}

	uint64 remainingLength = str.len;
	uint64 replacementLength = 0;
	for(uint32 i = 0; i < count; i++){
		remainingLength -= counts[i] * ((AvString*)((byte*)sequences+(i*stride)))->len;
		replacementLength += counts[i] * ((AvString*)((byte*)replacements+(i*stride)))->len;
	}
	uint64 newLength = remainingLength + replacementLength;
	AvStringHeapMemory memory;
	avStringMemoryHeapAllocate(newLength, &memory);

	uint64 writeIndex = 0;
	uint64 readIndex = 0;
	uint64 countReplaced = 0;

	while (countReplaced != totalCount) {
		AvString remainingStr = AV_STR(
			str.chrs + readIndex,
			str.len - readIndex
		);
		uint32 closestSequence = -1;
		strOffset closestOffset = -1;
		for(uint32 i = 0; i < count; i++){
			strOffset offset = avStringFindFirstOccuranceOf(
				remainingStr,
				*((AvString*)((byte*)sequences+(i*stride)))
			);
			if(offset==AV_STRING_NULL){
				continue;
			}
			if(offset < closestOffset){
				closestSequence = i;
				closestOffset = offset;
			}
		}

		avAssert(closestSequence!=-1, "error");
		
		strOffset offset = closestOffset;
		avStringMemoryStore(remainingStr, writeIndex, offset, memory);
		readIndex += offset;
		
		
		writeIndex += offset;
		avStringMemoryStore(*((AvString*)((byte*)replacements+(closestSequence*stride))), writeIndex, ((AvString*)((byte*)replacements+(closestSequence*stride)))->len, memory);
		readIndex += ((AvString*)((byte*)sequences+(closestSequence*stride)))->len;
		writeIndex += ((AvString*)((byte*)replacements+(closestSequence*stride)))->len;
		countReplaced++;
	}
	AvString remainingStr = AV_STR(
		str.chrs + readIndex,
		str.len - readIndex
	);
	avStringMemoryStore(remainingStr, writeIndex, remainingStr.len, memory);

	avStringFromMemory(dst, AV_STRING_WHOLE_MEMORY, memory);
	return totalCount;

}

uint64 avStringReplace(AvStringRef dst, AvString str, AvString sequence, AvString replacement) {
	avAssert(dst != nullptr, "destination must be a valid reference");

	uint64 count = avStringFindCount(str, sequence);

	if (count == 0) {
		avStringClone(dst, str);
		return 0;
	}

	uint64 remainingLength = str.len - count * sequence.len;
	uint64 newLength = remainingLength + count * replacement.len;
	AvStringHeapMemory memory;
	avStringMemoryHeapAllocate(newLength, &memory);


	uint64 writeIndex = 0;
	uint64 readIndex = 0;
	uint64 countReplaced = 0;

	while (countReplaced != count) {
		AvString remainingStr = AV_STR(
			str.chrs + readIndex,
			str.len - readIndex
		);
		strOffset offset = avStringFindFirstOccuranceOf(
			remainingStr,
			sequence
		);
		avStringMemoryStore(remainingStr, writeIndex, offset, memory);
		readIndex += offset;
		writeIndex += offset;
		avStringMemoryStore(replacement, writeIndex, replacement.len, memory);
		readIndex += sequence.len;
		writeIndex += replacement.len;
		countReplaced++;
	}
	AvString remainingStr = AV_STR(
		str.chrs + readIndex,
		str.len - readIndex
	);
	avStringMemoryStore(remainingStr, writeIndex, remainingStr.len, memory);

	avStringFromMemory(dst, AV_STRING_WHOLE_MEMORY, memory);
	return count;
}

void avStringJoin_(AvStringRef dst, ...) {
	va_list strs;
	va_start(strs, dst);
	AvDynamicArray arr;
	avDynamicArrayCreate(0, sizeof(AvString), &arr);
	uint64 length = 0;
	while (true) {
		AvString str = va_arg(strs, AvString);
		if (str.len == AV_STRING_NULL) {
			break;
		}
		length += str.len;
		avDynamicArrayAdd(&str, arr);
	}

	avAssert(dst != nullptr, "destination must be a valid reference");

	AvStringHeapMemory memory;
	avStringMemoryHeapAllocate(length, &memory);

	uint64 writeIndex = 0;
	avDynamicArrayForEachElement(AvString, arr, {
		avStringMemoryStore(element, writeIndex, element.len, memory);
		writeIndex += element.len;
		});
	avDynamicArrayDestroy(arr);

	avStringFromMemory(dst, AV_STRING_WHOLE_MEMORY, memory);
}

// void avStringAppend(AvStringRef dst, AvString src) {
// 	avAssert(dst != nullptr, "destination must be a valid reference");
// 	if (dst->memory == nullptr) {
// 		avStringMemoryAllocStore(src, dst->memory);
// 		avStringFromMemory(dst, 0, AV_STRING_FULL_LENGTH, dst->memory);
// 		return;
// 	}
// 	AvStringMemoryRef memory = dst->memory;
// 	if ((dst->chrs - dst->memory->data) + dst->len < dst->memory->capacity) {
// 		avStringMemoryHeapAllocate(dst->len + src.len, &memory);
// 		avStringMemoryStore(*dst, 0, dst->len, memory);
// 		avStringMemoryStore(src, dst->len, src.len, memory);
// 		avFree(dst);
// 		avStringFromMemory(dst, AV_STRING_WHOLE_MEMORY, memory);
// 	} else {
// 		avStringMemoryResizeSection(dst->len, src.len, memory);
// 		avStringMemoryStore(src, dst->len, src.len, memory);
// 		avStringResize(dst, dst->len + src.len);
// 	}
// }

void avStringMemoryStoreCharArraysVA_(AvStringMemoryRef memory, ...) {

	AvDynamicArray arr;
	avDynamicArrayCreate(0, sizeof(const char*), &arr);



	va_list list;
	va_start(list, memory);
	const char* arg = NULL;
	while ((arg = va_arg(list, const char*)) != nullptr) {
		avDynamicArrayAdd(&arg, arr);
	}
	va_end(list);

	uint32 count = avDynamicArrayGetSize(arr);
	const char** strs = avCallocate(count, sizeof(const char*), "allocating strings");
	avDynamicArrayReadRange(strs, count, 0, sizeof(const char*), 0, arr);
	avDynamicArrayDestroy(arr);

	avStringMemoryStoreCharArrays(memory, count, strs);

	avFree(strs);
}

void avStringMemoryStoreCharArrays(AvStringMemoryRef memory, uint32 count, const char* strs[]) {

	uint64 offset = 0;
	for (uint i = 0; i < count; i++) {
		uint64 size = avCStringLength(strs[i]);
		avStringMemoryStore(AV_STR(strs[i], size), offset, size, memory);
		offset += size;
	}
}

void avStringPrint(AvString str) {
	printf("%.*s", (uint32)str.len, str.chrs);
	fflush(stdout);
}

void avStringPrintData(AvString str) {
	printf("{ .chrs=%.*s, .len=%"PRIu64" .memory=0x%p }\n", (uint32)str.len, str.chrs, str.len, str.memory);
	fflush(stdout);
}

void avStringPrintLn(AvString str) {
	avStringPrint(str);
	printf("\n");
}

void avStringPrintln(AvString str) {
	avStringPrintLn(str);
}



//only %i, %u, %c supported
uint32 avStringScanf(AvString format, AvString str, ...) {
	va_list elems;
	va_start(elems, str);
	uint64 readIndex = 0;
	uint32 elementsFilled = 0;
	bool8 parse = 0;
	for (uint64 i = 0; i < format.len && readIndex < str.len; i++) {
		char c = format.chrs[i];
		if (c == '%') {
			parse = true;
			continue;
		}
		if (parse) {
			switch (c) {
			case 'i':
			{
				bool32 negative = -1;
				uint32 value = 0;
				for (; readIndex < str.len; readIndex++) {
					char chr = str.chrs[readIndex];
					if (negative == -1) {
						negative = chr == '-' ? 1 : 0;
						continue;
					}
					if (avCharIsNumber(chr)) {
						value *= 10;
						value += chr - '0';
					}
				}
				int* ptr = va_arg(elems, int*);
				(*ptr) = ((int32)value) * (negative == 1 ? -1 : 1);}
			break;
			case 'u':
			{
				uint32 value = 0;
				for (; readIndex < str.len; readIndex++) {
					char chr = str.chrs[readIndex];
					if (avCharIsNumber(chr)) {
						value *= 10;
						value += chr - '0';
					}
				}
				uint* ptr = va_arg(elems, uint*);
				*ptr = value;
			}
			break;
			case 'c':
			{
				for (; readIndex < str.len; readIndex++) {
					char chr = str.chrs[readIndex];
					char* ptr = va_arg(elems, char*);
					*ptr = chr;
					break;
				}
			}
			break;
			case '%':
				continue;
				break;
			default:
				avAssert(c, "unsupported character");
				va_end(elems);
				return elementsFilled;
			}
			parse = false;
			elementsFilled++;
			continue;
		}
		if (c != format.chrs[readIndex++]) {
			break;
		}
	}

	va_end(elems);
	return elementsFilled;
}

uint32 avStringSplitOnChar(AV_DS(AvArrayRef, AvString) substrings, char split, AvString str) {
	avAssert(substrings != nullptr, "substrings must be a valid array");
	avAssert(substrings->allocatedSize == 0, "substring must be an unallocated array");
	avAssert(str.len > 0, "split must be a valid string");
	avAssert(str.chrs != nullptr, "split must be a valid string");

	uint splitCount = split == '\0' ? str.len : avStringFindCharCount(str, split);
	avArrayAllocateWithFreeCallback(splitCount + 1, sizeof(AvString), substrings, false, nullptr, (AvDestroyElementCallback)&avStringFree);
	if (splitCount == 0) {
		avStringCopy(avArrayGetPtr(0, substrings), str);
		return 1;
	}

	uint64 substrStart = 0;
	uint64 subStrEnd = 0;
	uint32 index = 0;
	for (uint64 i = 0; i < str.len; i++) {
		if (str.chrs[i] == split) {
			uint64 len = subStrEnd - substrStart;
			avStringCopySection(avArrayGetPtr(index++, substrings), substrStart, len, str);
			substrStart = subStrEnd + 1;
		}
		subStrEnd++;
	}
	uint64 len = subStrEnd - substrStart;
	avStringCopySection(avArrayGetPtr(index++, substrings), substrStart, len, str);
	return splitCount;
}

uint32 avStringSplit(AV_DS(AvArrayRef, AvString) substrings, AvString split, AvString str) {
	avAssert(substrings != nullptr, "substrings must be a valid array");
	avAssert(substrings->allocatedSize == 0, "substring must be an unallocated array");
	avAssert(str.len > 0, "split must be a valid string");
	avAssert(str.chrs != nullptr, "split must be a valid string");

	uint splitCount = split.len == 0 ? str.len : avStringFindCount(str, split);
	avArrayAllocateWithFreeCallback(splitCount + 1, sizeof(AvString), substrings, false, nullptr, (AvDestroyElementCallback)&avStringFree);
	if (splitCount == 0) {
		avStringCopy(avArrayGetPtr(0, substrings), str);
		return 1;
	}

	uint32 offset = 0;
	uint32 index = 0;

	uint32 count = 0;
	while (count != splitCount) {
		strOffset sectionLength = avStringFindFirstOccuranceOf(AV_STR(str.chrs + offset, str.len - offset), split) - 1;
		avStringCopySection(avArrayGetPtr(index++, substrings), offset, sectionLength, str);
		offset += sectionLength + split.len;
		count++;
	}
	avStringCopySection(avArrayGetPtr(index++, substrings), offset, str.len - offset, str);

	return splitCount;
}

void avStringFlip(AvStringRef str) {
	avAssert(str != nullptr, "string must be a valid reference");
	AvString refString = AV_EMPTY;
	if (str->memory == nullptr) {
		avStringClone(str, *str);
	}
	avStringClone(&refString, *str);

	for (uint64 i = 0; i < str->len; i++) {
		str->memory->data[i] = refString.chrs[str->len - i - 1];
	}

	avStringFree(&refString);
}

void avStringCopyToAllocator(AvString src, AvStringRef dst, AvAllocator* allocator){
	avAssert(src.len != 0, "string length cannot be 0");
	avAssert(src.chrs != nullptr, "string must have data");
	avAssert(dst!=nullptr, "destination must be a valid reference");
	avAssert(dst->memory==nullptr, "destination must be empty");
	avAssert(dst->chrs==nullptr, "destination must be empty");
	avAssert(dst->len==0, "destination must be empty");

	char* buffer = avAllocatorAllocate(src.len+1, allocator);
	memcpy(buffer, src.chrs, src.len);
	AvString str = {
		.chrs = buffer,
		.len = src.len,
		.memory = nullptr,
	};
	memcpy(dst, &str, sizeof(AvString));
}



