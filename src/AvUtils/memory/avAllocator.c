#include <AvUtils/memory/avAllocator.h>
#include <AvUtils/avLogging.h>
#include <string.h>

#define ALLOC_FUNC(type, func, ...) av##type##Allocator##func (__VA_ARGS__  ( Av##type##Allocator* ) allocator);
#define ALLOC_FUNC_CASE(TYPE, type, func, op, ...) case AV_ALLOCATOR_TYPE_##TYPE: \
    op ALLOC_FUNC (type, func, __VA_ARGS__)\
    break;\

#define ALLOC_FUNCS(func,op, ...) switch (allocator->type) {\
        ALLOC_FUNC_CASE(DYNAMIC, Dynamic, func, op, __VA_ARGS__)\
        ALLOC_FUNC_CASE(LINEAR, Linear, func, op, __VA_ARGS__)\
        default: avAssert(0, "invalid allocator type"); break;\
    }\

void avAllocatorCreate(uint64 size, AvAllocatorType type, AvAllocator* allocator) {
    allocator->type = type;
    ALLOC_FUNCS(Create, , size, );
}
void* avAllocatorAllocate(uint64 size, AvAllocator* allocator) {
    ALLOC_FUNCS(Allocate, return, size, );
    return nullptr;
}

uint64 avAllocatorGetAllocatedSize(AvAllocator* allocator) {
    ALLOC_FUNCS(GetAllocatedSize, return, );
    return 0;
}

bool32 avAllocatorTransform(AvAllocatorType srcType, AvAllocatorType dstType, AvAllocator* allocator) {

    if (srcType != allocator->type) {
        avAssert(srcType != allocator->type, "source type must equal the type of the allocator to be transformed");
        return false;
    }

    if(srcType == AV_ALLOCATOR_TYPE_NONE || dstType == AV_ALLOCATOR_TYPE_NONE){
        avAssert(srcType == AV_ALLOCATOR_TYPE_NONE || dstType == AV_ALLOCATOR_TYPE_NONE, "allocator type must not be of type NONE");
        return false;
    }

    if (srcType == dstType) {
        return true;
    }

    AvAllocator tmpAllocator = { 0 };
    avAllocatorCreate(avAllocatorGetAllocatedSize(allocator), dstType, &tmpAllocator);
    void* data = avAllocatorAllocate(avAllocatorGetAllocatedSize(allocator), &tmpAllocator);
    
    switch(srcType){
        case AV_ALLOCATOR_TYPE_DYNAMIC:
            avDynamicArrayReadRange(data, avAllocatorGetAllocatedSize(allocator), 0, 1, 0, allocator->dynamicAllocator.memory);
        break;
        case AV_ALLOCATOR_TYPE_LINEAR:
            memcpy(data, allocator->linearAllocator.base, avAllocatorGetAllocatedSize(allocator));
        break;
        default:
            avAssert(0, "developer messed up by forgetting break statement");
            break;
    }
    return true;
}

void avAllocatorReset(AvAllocator* allocator) {
    ALLOC_FUNCS(Reset, ;, );
}
void avAllocatorDestroy(AvAllocator* allocator) {
    ALLOC_FUNCS(Destroy, ;, );
}