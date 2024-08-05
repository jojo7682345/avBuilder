#ifndef __AV_ARRAY__
#define __AV_ARRAY__
#include "../avDefinitions.h"
C_SYMBOLS_START

#include "../avTypes.h"

typedef struct AvArrayFreeCallbackOptions{
    AvDestroyElementCallback destroyElementCallback;
    AvDeallocateElementCallback deallocateElementCallback;
    bool8 dereferenceElement : 1;
}AvArrayFreeCallbackOptions;

typedef struct AvArray{
    const uint32 count;
    const uint64 elementSize;
    const uint64 allocatedSize;
    const void* data;
    const AvArrayFreeCallbackOptions freeCallbackOptions;
}AvArray;

typedef AvArray* AvArrayRef;
void avArrayAllocate(uint32 count, uint64 dataSize, AvArrayRef array);
void avArrayAllocateWithFreeCallback(uint32 count, uint64 dataSize, AvArrayRef array, bool32 dereferenceElementBeforeFree, AV_NULL_OPTION AvDeallocateElementCallback deallocateElementCallback, AV_NULL_OPTION AvDestroyElementCallback destroyElementCallback);

bool32 avArrayWrite(void* data, uint32 index, AvArrayRef array);
bool32 avArrayRead(void* data, uint32 index, AvArrayRef array);
void* avArrayGetPtr(uint32 index, AvArrayRef array);

#define avArrayForEachElement(type, element, index, array, code) for(uint32 index = 0; index < (array)->count; index++) { type element; avArrayRead(&element, index, (array));\
 code\
}

void avArrayFreeAndDeallocate(AvArrayRef array, AvDeallocateElementCallback deallocElement);
void avArrayFreeAndDestroy(AvArrayRef array, AvDestroyElementCallback destroyElement);
void avArrayFree(AvArrayRef array);



C_SYMBOLS_END
#endif//__AV_ARRAY__