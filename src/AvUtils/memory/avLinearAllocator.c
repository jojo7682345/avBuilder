#include <AvUtils/memory/avLinearAllocator.h>
#include <AvUtils/avMemory.h>
#include <AvUtils/avLogging.h>
#include <string.h>

void avLinearAllocatorCreate(uint64 size, AvLinearAllocator* allocator) {
    avAssert(allocator->allocatedSize == 0, "allocator already allocated");
    avAssert(allocator->base == 0, "allocator already allocated");
    avAssert(allocator->current == 0, "allocator already allocated");

    allocator->allocatedSize = size;
    allocator->base = avCallocate(1, size, "allocating bytes for linear allocator");
    allocator->current = 0;
}

void* avLinearAllocatorAllocate(uint64 size, AvLinearAllocator* allocator) {
    return avLinearAllocatorAllocateAlligned(size, AV_DEFAULT_ALIGNMENT, allocator);
}
static bool32 isPowerOfTwo(uint64 offset) {
    return (offset & (offset - 1)) == 0;
}

static void* alignForward(void* ptr, uint64 allignment) {
    avAssert(isPowerOfTwo(allignment), "allignment must be a power of two");
    
    uint64 modulo = (uint64)ptr & (allignment - 1);
    if (modulo != 0) {

        ptr = (byte*)ptr + (allignment - modulo);
    }
    return ptr;
}

void* avLinearAllocatorAllocateAlligned(uint64 size, uint64 alignment, AvLinearAllocator* allocator) {
    uint64 current = (uint64)allocator->base + allocator->current;
    uint64 offset = (uint64)alignForward((void*)current, alignment);
    offset -= (uint64) allocator->base;

    if(offset + size <= allocator->allocatedSize){
        void* ptr = (byte*)allocator->base + offset;
        allocator->current = offset + size;

        memset(ptr, 0, size);
        return ptr;
    }
    avAssert(offset + size <= allocator->allocatedSize, "allocator ran out of space");
    return nullptr;
}

void avLinearAllocatorReset(AvLinearAllocator* allocator) {
    allocator->current = 0;
}

void avLinearAllocatorDestroy(AvLinearAllocator* allocator) {
    avAssert(allocator->base != nullptr, "allocator not allocated");

    avFree((void*)allocator->base);
    allocator->base = nullptr;
    allocator->allocatedSize = 0;
    avLinearAllocatorReset(allocator);
}

uint64 avLinearAllocatorGetAllocatedSize(AvLinearAllocator* allocator){
    return allocator->allocatedSize;
}

uint64 avLinearAllocatorGetRemainingSize(AvLinearAllocator allocator) {
    return allocator.allocatedSize - allocator.current;
}