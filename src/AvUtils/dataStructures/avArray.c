#include <AvUtils/dataStructures/avArray.h>
#include <AvUtils/avMemory.h>
#include <AvUtils/avLogging.h>
#include <string.h>

void avArrayAllocate(uint32 count, uint64 elementSize, AvArrayRef array){
    avArrayAllocateWithFreeCallback(count, elementSize, array, 0, nullptr, nullptr);
}

void avArrayAllocateWithFreeCallback(uint32 count, uint64 elementSize, AvArrayRef array, bool32 dereferenceElementBeforeFree, AV_NULL_OPTION AvDeallocateElementCallback deallocateElementCallback, AV_NULL_OPTION AvDestroyElementCallback destroyElementCallback) {
    avAssert(count != 0, "cannot allocate array of size 0");
    avAssert(elementSize != 0, "cannot allocate elements of size 0");
    avAssert(array != nullptr, "array must be a valid reference");
    avAssert(!((deallocateElementCallback != (AvDeallocateElementCallback){0}) && (destroyElementCallback != (AvDestroyElementCallback){0})), "only one destroycallback can be defined");

    AvArray tmpArray = {
        .count = count,
        .elementSize = elementSize,
        .allocatedSize = count * elementSize,
        .data = avCallocate(count, elementSize, "allocating array data"),
        .freeCallbackOptions.deallocateElementCallback = deallocateElementCallback,
        .freeCallbackOptions.destroyElementCallback = destroyElementCallback,
        .freeCallbackOptions.dereferenceElement = dereferenceElementBeforeFree
    };
    memcpy(array, &tmpArray, sizeof(AvArray));

}

static void* getPtr(uint32 index, AvArrayRef array){
    avAssert(array->data!=nullptr, "array must first be allocated");
    avAssert(array->elementSize != 0, "elements must not be of size 0");
    if(index >= array->count){
        return nullptr;
    }
    return (byte*)array->data + ((uint64)index * array->elementSize);
}

bool32 avArrayWrite(void* data, uint32 index, AvArrayRef array){
    avAssert(data != nullptr, "data must be a valid pointer");
    avAssert(array->allocatedSize != 0, "array must be allocated first");
    if(index >= array->count){
        return 0;
    }
    memcpy(getPtr(index, array), data, array->elementSize);
    return true;
}
bool32 avArrayRead(void* data, uint32 index, AvArrayRef array){
    avAssert(data != nullptr, "data must be a valid pointer");
    avAssert(array->allocatedSize != 0, "array must be allocated first");
    if (index >= array->count) {
        return 0;
    }
    memcpy(data, getPtr(index, array), array->elementSize);
    return true;
}
void* avArrayGetPtr(uint32 index, AvArrayRef array){
    avAssert(array->allocatedSize != 0, "array must be allocated first");
    if (index >= array->count) {
        return nullptr;
    }
    return getPtr(index, array);
}

void avArrayDeallocateElements(AvArrayRef array, AvDeallocateElementCallback deallocElement){
    avAssert(array != nullptr, "array must be a valid reference");
    avAssert(array->allocatedSize != 0, "array must be allocated first");
    for (uint32 i = 0; i < array->count; i++) {
        void* data = array->freeCallbackOptions.dereferenceElement ? *(void**)getPtr(i, array) : getPtr(i, array);
        deallocElement(data, array->elementSize);
    }
}

static void arrayFree(AvArrayRef array){
    avFree((void*)array->data);
    memset(array, 0, sizeof(AvArray));
}

void avArrayDestroyElements(AvArrayRef array, AvDestroyElementCallback destroyCallback){
    avAssert(array != nullptr, "array must be a valid reference");
    avAssert(array->allocatedSize != 0, "array must be allocated first");
    if (array->freeCallbackOptions.dereferenceElement) {
        avAssert(array->elementSize == sizeof(void*), "cannot call destroy on non pointer types");
    }
    for (uint32 i = 0; i < array->count; i++) {
        void* data = array->freeCallbackOptions.dereferenceElement ? *(void**)getPtr(i, array) : getPtr(i, array);
        //printf("destroying %p\n", data);
        destroyCallback(data);
    }
}

void avArrayFreeAndDeallocate(AvArrayRef array, AvDeallocateElementCallback deallocElement){
    avArrayDeallocateElements(array, deallocElement);
    arrayFree(array);
}
void avArrayFreeAndDestroy(AvArrayRef array, AvDestroyElementCallback destroyHandle){
    avArrayDestroyElements(array, destroyHandle);
    arrayFree(array);
}
void avArrayFree(AvArrayRef array){
    avAssert(array != nullptr, "array must be a valid reference");
    avAssert(!((array->freeCallbackOptions.deallocateElementCallback != nullptr) && (array->freeCallbackOptions.destroyElementCallback != nullptr)), "only one destroycallback can be defined");
    //printf("asserted\n");
    if (array->freeCallbackOptions.deallocateElementCallback) {
        avArrayDeallocateElements(array, array->freeCallbackOptions.deallocateElementCallback);
        return;
    } else if (array->freeCallbackOptions.destroyElementCallback) {
        avArrayDestroyElements(array, array->freeCallbackOptions.destroyElementCallback);
    }
    arrayFree(array);
}