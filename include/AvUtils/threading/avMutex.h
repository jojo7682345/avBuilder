#ifndef __AV_MUTEX__
#define __AV_MUTEX__
#include "../avDefinitions.h"
C_SYMBOLS_START

#include "../avTypes.h"

typedef struct AvMutex_T* AvMutex;

bool32 avMutexCreate(AvMutex* mutex);

void avMutexDestroy(AvMutex mutex);

void avMutexLock(AvMutex mutex);
void avMutexUnlock(AvMutex mutex);

C_SYMBOLS_END
#endif//__AV_MUTEX__