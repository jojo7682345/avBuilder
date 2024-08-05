#ifndef __AV_DYNAMIC_ALLOCATOR__
#define __AV_DYNAMIC_ALLOCATOR__
#include "../avDefinitions.h"
C_SYMBOLS_START
#include <AvUtils/avTypes.h>
#include <AvUtils/dataStructures/avDynamicArray.h>

typedef struct AvDynamicAllocator {
    AvDynamicArray memory;
} AvDynamicAllocator;

void avDynamicAllocatorCreate(uint64 size, AvDynamicAllocator* allocator);

void* avDynamicAllocatorAllocate(uint64 size, AvDynamicAllocator* allocator);

uint64 avDynamicAllocatorGetAllocatedSize(AvDynamicAllocator* allocator);

void avDynamicAllocatorReset(AvDynamicAllocator* allocator);

void avDynamicAllocatorDestroy(AvDynamicAllocator* allocator);


C_SYMBOLS_END
#endif//__AV_DYNAMIC_ALLOCATOR__