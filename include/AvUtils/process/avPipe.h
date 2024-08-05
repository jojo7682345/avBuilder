#ifndef __AV_PIPE__
#define __AV_PIPE__
#include "../avDefinitions.h"
C_SYMBOLS_START
#include "../avTypes.h"
#include "../filesystem/avFile.h"


typedef struct AvPipe {
    AvFileDescriptor write;
    AvFileDescriptor read;
}AvPipe;

void avPipeCreate(AvPipe* pipe);
void avPipeConsumeWriteChannel(AvPipe* pipe);
void avPipeConsumeReadChannel(AvPipe* pipe);
void avPipeDestroy(AvPipe* pipe);

C_SYMBOLS_END
#endif//__AV_PIPE__