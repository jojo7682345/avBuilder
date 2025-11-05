#ifndef __COMPILE_COMMANDS__
#define __COMPILE_COMMANDS__

#include <AvUtils/avTypes.h>

void ensureJsonOpen();
void addCommandToCompileCommands(const char* command);
void finalizeCompileCommands();

#endif//__COMPILE_COMMANDS__
