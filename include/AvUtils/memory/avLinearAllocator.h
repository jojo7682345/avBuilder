#ifndef __AV_LINEAR_ALLOCATOR__
#define __AV_LINEAR_ALLOCATOR__
#include "../avDefinitions.h"
C_SYMBOLS_START
#include <AvUtils/avTypes.h>

#ifndef AV_DEFAULT_ALIGNMENT
#define AV_DEFAULT_ALIGNMENT (sizeof(void*))
#endif

typedef struct AvLinearAllocator {
    const void* base;
    uint64 allocatedSize;
    uint64 current;
} AvLinearAllocator;

void avLinearAllocatorCreate(uint64 size, AvLinearAllocator* allocator);

void* avLinearAllocatorAllocate(uint64 size, AvLinearAllocator* allocator);
void* avLinearAllocatorAllocateAlligned(uint64 size, uint64 allignment, AvLinearAllocator* allocator);

void avLinearAllocatorReset(AvLinearAllocator* allocator);

void avLinearAllocatorDestroy(AvLinearAllocator* allocator);

uint64 avLinearAllocatorGetAllocatedSize(AvLinearAllocator* allocator);

uint64 avLinearAllocatorGetRemainingSize(AvLinearAllocator allocator);


C_SYMBOLS_END
#endif//__AV_LINEAR_ALLOCATOR__


