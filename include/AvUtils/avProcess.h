#ifndef __AV_PROCESS__
#define __AV_PROCESS__
#include "avDefinitions.h"
C_SYMBOLS_START

#include "avTypes.h"
#include <AvUtils/avString.h>
#include <AvUtils/dataStructures/avArray.h>
#include <AvUtils/avFileSystem.h>

typedef struct AvProcess_T* AvProcess;

typedef struct AvProcessStartInfo {
    AvString executable;
    AV_DS(AvArray, AvString) args;
    AvString workingDirectory;
    const AvFileDescriptor* input;
    const AvFileDescriptor* output;
} AvProcessStartInfo;

void avProcessStartInfoPopulate_(AvProcessStartInfo* info, AvString bin, AvString cwd, ...);
void avProcessStartInfoPopulateARR(AvProcessStartInfo* info, AvString bin, AvString cwd, uint32 count, AvString* args);
#define avProcessStartInfoPopulate(info, bin, cwd, ...) avProcessStartInfoPopulate_(info, bin, cwd, __VA_ARGS__, AV_STR(nullptr, 0))
void avProcessStartInfoCreate(AvProcessStartInfo* info, AvString bin, AvString cwd, uint32 argCount, AvString* argValues);
void avProcessStartInfoDestroy(AvProcessStartInfo* info);

bool32 avProcessStart(AvProcessStartInfo info, AvProcess* process);
int32 avProcessWaitExit(AvProcess process);
void avProcessKill(AvProcess process);
void avProcessDiscard(AvProcess process);
int32 avProcessRun(AvProcessStartInfo info);
C_SYMBOLS_END
#endif//__AV_PROCESS__