#ifndef __AV_ALLOCATOR__
#define __AV_ALLOCATOR__
#include "../avDefinitions.h"
C_SYMBOLS_START
#include <AvUtils/avTypes.h>
#include "avDynamicAllocator.h"
#include "avLinearAllocator.h"

typedef enum AvAllocatorType {
    AV_ALLOCATOR_TYPE_NONE = 0,
    AV_ALLOCATOR_TYPE_DYNAMIC,
    AV_ALLOCATOR_TYPE_LINEAR,
} AvAllocatorType;

typedef struct AvAllocator {
    union {
        AvDynamicAllocator dynamicAllocator;
        AvLinearAllocator linearAllocator;
    };
    AvAllocatorType type;
} AvAllocator;

void avAllocatorCreate(uint64 size, AvAllocatorType type, AvAllocator* allocator);
void* avAllocatorAllocate(uint64 size, AvAllocator* allocator);
void avAllocatorReset(AvAllocator* allocator);
void avAllocatorDestroy(AvAllocator* allocator);

C_SYMBOLS_END
#endif//__AV_ALLOCATOR__