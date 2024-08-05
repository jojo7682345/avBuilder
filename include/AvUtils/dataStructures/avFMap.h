#ifndef __AV_FIXED_MAP__
#define __AV_FIXED_MAP__
#include "../avDefinitions.h"
C_SYMBOLS_START

#include "../avTypes.h"

typedef struct AvFMap_T* AvFMap;

#define AV_MAP_STORED_KEY_SIZE 0

typedef uint32(*HashFunction)(void* data, uint64 dataSize, uint32 mapSize);

void avFMapCreate(uint32 size, uint64 dataSize, uint64 keySize, HashFunction hashFunction, AvFMap* map);

bool32 avFMapWrite(void* data, void* key, uint64 keySize, AvFMap map);
void avFMapRead(void* data, void* key, uint64 keySize, AvFMap map);

void* avFMapGetPtr(void* key, uint64 keySize, AvFMap map);

uint32 avFMapGetSize(AvFMap map);
uint64 avFMapGetDataSize(AvFMap map);
uint64 avFMapGetKeySize(AvFMap map);

void avFMapClear(void* data, AvFMap map);

void avFMapDestroy(AvFMap map);

C_SYMBOLS_END
#endif//__AV_MAP__