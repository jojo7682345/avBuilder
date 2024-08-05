#ifndef __AV_STREAM__
#define __AV_STREAM__
#include "../avDefinitions.h"
C_SYMBOLS_START

#include "../avTypes.h"
#include "../avFileSystem.h"

typedef struct AvStream {
    const AvFileDescriptor discard;
    byte* const buffer;
    const uint64 size;
    uint64 pos;
}* AvStream;

struct AvStream avStreamCreate(void* buffer, uint64 size, const AvFileDescriptor discard);

void avStreamDiscard(AvStream stream);

void avStreamPutC(char chr, AvStream stream);

void avStreamFlush(AvStream stream);

C_SYMBOLS_END
#endif//__AV_STREAM__