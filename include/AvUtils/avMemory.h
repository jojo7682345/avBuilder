#ifndef __AV_MEMORY__
#define __AV_MEMORY__
#include "avDefinitions.h"
C_SYMBOLS_START

#include "avTypes.h"


void* avAllocate_(uint64 size, const char* message, uint line, const char* func, const char* file);

#ifdef AV_ALLOCATE_ZERO_OUT
#define avAllocate(size, message) avCallocate_(1, size, message, __LINE__, __func__, __FILE__);
#else
#define avAllocate(size, message) avAllocate_(size, message, __LINE__, __func__, __FILE__)
#endif

void* avCallocate_(uint64 count, uint64 size, const char* message, uint line, const char* func, const char* file);
#define avCallocate(count, size, message) avCallocate_(count, size, message, __LINE__, __func__, __FILE__)

void* avReallocate_(void* data, uint64 size, const char* message, uint line, const char* func, const char* file);
#define avReallocate(data, size, message) avReallocate_(data, size, message, __LINE__, __func__, __FILE__)

void avFree_(void* data, uint line, const char* func, const char* file);
#define avFree(data) avFree_(data, __LINE__, __func__, __FILE__)

C_SYMBOLS_END
#endif //__AV_MEMORY__