#include <AvUtils/dataStructures/avFMap.h>
#include <AvUtils/avMemory.h>
#include <string.h>


//TODO: fix collisions, just make a better implementation

typedef struct AvFMap_T {
	HashFunction hash;

	uint64 dataSize;
	uint64 keySize;
	uint32 size;
	void* data;
} AvFMap_T; 

static uint32 defaultHashFunction(void* data, uint64 dataSize, uint32 mapSize) {
	byte* ch = (byte*)data;
	int i, sum;
	for (sum = 0, i = 0; i < dataSize; i++)
		sum += ch[i];
	return sum % mapSize;
}

void avFMapCreate(uint32 size, uint64 dataSize, uint64 keySize, HashFunction hashFunction, AvFMap* map) {
	if (size == 0 || dataSize == 0) {
		//TODO: add error log
		return;
	}
	(*map) = avCallocate(1, sizeof(AvFMap_T), "allocating map handle");
	(*map)->size = size;
	(*map)->dataSize = dataSize;
	(*map)->hash = hashFunction == NULL ? &defaultHashFunction : hashFunction;
	(*map)->keySize = keySize;
	(*map)->data = avCallocate(size, dataSize, "allocating map data");
}

static bool32 checkBounds(uint32 index, AvFMap map) {
	return index < map->size;
}

static void* getPtr(uint32 index, AvFMap map) {
	return (byte*)map->data + (uint64)index * map->dataSize;;
}

bool32 avFMapWrite(void* data, void* key, uint64 keySize, AvFMap map) {
	
	uint32 index = map->hash(key, keySize == AV_MAP_STORED_KEY_SIZE ? map->keySize : keySize, map->size);
	if (!checkBounds(index, map)) {
		//TODO: add error log
		return false; 
	}
	
	memcpy(data, getPtr(index, map), map->dataSize);
	return false;
}

void avFMapRead(void* data, void* key, uint64 keySize, AvFMap map) {
	uint32 index = map->hash(key, keySize == AV_MAP_STORED_KEY_SIZE ? map->keySize : keySize, map->size);
	if (!checkBounds(index, map)) {
		//TODO: add error log
		return;
	}
	memcpy(getPtr(index, map), data, map->dataSize);
}

void* avFMapGetPtr(void* key, uint64 keySize, AvFMap map) {
	uint32 index = map->hash(key, keySize == AV_MAP_STORED_KEY_SIZE ? map->keySize : keySize, map->size);
	if (!checkBounds(index, map)) {
		//TODO: add error log
		return nullptr;
	}
	return getPtr(index, map);
}

uint32 avFMapGetSize(AvFMap map) {
	return map->size;
}

uint64 avFMapGetDataSize(AvFMap map) {
	return map->dataSize;
}

uint64 avFMapGetKeySize(AvFMap map) {
	return map->keySize;
}

void avFMapClear(void* data, AvFMap map) {
	if (data == nullptr) {
		memset(map->data, 0, (uint64)map->size * map->dataSize);
		return;
	}

	for (uint32 i = 0; i < map->size; i++) {
		memcpy(getPtr(i, map), data, map->dataSize);
	}
}

void avFMapDestroy(AvFMap map) {
	avFree(map->data);
	avFree(map);
}
