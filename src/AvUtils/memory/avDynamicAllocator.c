#include <AvUtils/memory/avDynamicAllocator.h>
#include <AvUtils/avMemory.h>
#include <AvUtils/avLogging.h>
#define AV_DYNAMIC_ARRAY_EXPOSE_MEMORY_LAYOUT
#include <AvUtils/dataStructures/avDynamicArray.h>

void avDynamicAllocatorCreate(uint64 size, AvDynamicAllocator* allocator) {
    (void)size;
    avDynamicArrayCreate(0, 1, &allocator->memory);
    avDynamicArraySetAllowRelocation(false, allocator->memory);
}

void* avDynamicAllocatorAllocate(uint64 size, AvDynamicAllocator* allocator) {
    avAssert(size < (1ULL<<32), "single allocations can not exeed 32 bit size");
    avAssert(size != 0, "cannot allocate of size 0");
    avDynamicArraySetGrowSize((uint32)size, allocator->memory);
    byte null = 0;
    uint32 index = avDynamicArrayAdd(&null, allocator->memory);
    for(uint64 i = 1; i < size; i++){
        avDynamicArrayAdd(&null, allocator->memory);
    }
    uint32 pageIndex = avDynamicArrayGetIndexPage(&index, allocator->memory);
    avAssert(index == 0, "memory corrupted");
    return avDynamicArrayGetPageDataPtr(pageIndex, allocator->memory);
}

uint64 avDynamicAllocatorGetAllocatedSize(AvDynamicAllocator* allocator){
    return avDynamicArrayGetSize(allocator->memory);
}

void avDynamicAllocatorReset(AvDynamicAllocator* allocator) {
    avDynamicArrayClear(nullptr, allocator->memory);
    avDynamicArraySetAllowRelocation(true, allocator->memory);
    avDynamicArrayTrim(allocator->memory);
    avDynamicArraySetAllowRelocation(false, allocator->memory);
}

void avDynamicAllocatorDestroy(AvDynamicAllocator* allocator) {
    avDynamicArrayDestroy(allocator->memory);
    allocator->memory = nullptr;
}
