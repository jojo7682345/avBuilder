#ifndef __AV_ENVIRONMENT__
#define __AV_ENVIRONMENT__
#include "avDefinitions.h"
C_SYMBOLS_START

#include "avString.h"
#include "filesystem/avDirectory.h"

bool32 avChangeDirectory(AvString path);
bool32 avGetEnvironmentVariable(AvString variable, AvStringRef value);

C_SYMBOLS_END
#endif//__AV_FILE_SYSTEM__