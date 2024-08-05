#ifndef __AV_FILE__
#define __AV_FILE__
#include "../avDefinitions.h"
C_SYMBOLS_START

#include "../avTypes.h"
#include "../avString.h"
#include <stdarg.h>
#include "../util/avTime.h"

#define AV_ROOT_DIR ""
#define AV_CURRENT_DIR "."
#define AV_PARENT_DIR ".."

typedef int32 AvFileDescriptor;

typedef struct AvFile_T* AvFile;

extern const AvFileDescriptor avStdOut;
extern const AvFileDescriptor avStdErr;
extern const AvFileDescriptor avStdIn;
#define AV_FILE_DESCRIPTOR_NULL (-1)

typedef enum {
	AV_FILE_OK = 0,
	AV_FILE_ERROR = 1,
	AV_FILE_NOT_FOUND = 2,
	AV_FILE_UNABLE_TO_OPEN = 3,
}AvFileErrorCode;

typedef enum AvFileStatus {
	AV_FILE_STATUS_UNKNOWN = 0,
	AV_FILE_STATUS_CLOSED = 1,
	AV_FILE_STATUS_OPEN_READ = 0x2,
	AV_FILE_STATUS_OPEN_WRITE = 0x4,
	AV_FILE_STATUS_OPEN_UPDATE = 0x8,
} AvFileStatus;

typedef enum AvFileOpenMode {
	AV_FILE_OPEN_WRITE = 0,
	AV_FILE_OPEN_READ = 0x1,
	AV_FILE_OPEN_APPEND = 0x2,
}AvFileOpenMode;

typedef struct AvFileOpenOptions {
	byte openMode;
	bool8 update;
	bool8 binary;
} AvFileOpenOptions;

#define AV_FILE_MAX_FILENAME_LENGTH 512
#define AV_FILE_MAX_PATH_LENGTH 512
#define AV_FILE_PADDING_LENGTH 3

typedef struct AvFileNameProperties {
	AvStringMemory fileStr;
	AvString fileFullPath;
	AvString filePath;
	AvString fileName;
	AvString fileNameWithoutExtension;
	AvString fileExtension;
}AvFileNameProperties;

#define avFileBuildPathVAR(fileName, filePath, ...) avFileBuildPathVAR_(fileName, filePath, __VA_ARGS__, NULL)
void avFileBuildPathVAR_(const char* fileName, AvStringRef filePath, ...);
void avFileBuildPathARR(const char* fileName, AvStringRef filePath, uint32 directoryCount, AvString direcotries[]);

void avFileHandleCreate(AvString filePath, AvFile* file);

AvFileNameProperties* avFileHandleGetFileNameProperties(AvFile file);

bool32 avFileExists(AvFile file);
uint64 avFileGetSize(AvFile file);
AvDateTime avFileGetCreationTime(AvFile file);
AvDateTime avFileGetAccessedTime(AvFile file);
AvDateTime avFileGetModifiedTime(AvFile file);
AvFileStatus avFileGetStatus(AvFile file);
AvFileDescriptor avFileGetDescriptor(AvFile file);

bool32 avFileDelete(AvFile file);

#define AV_FILE_OPEN_READ_DEFAULT (AvFileOpenOptions) {.openMode=AV_FILE_OPEN_READ, .binary=false, .update=false}
#define AV_FILE_OPEN_READ_BINARY_DEFAULT (AvFileOpenOptions) {.openMode=AV_FILE_OPEN_READ, .binary=true, .update=false}


#define AV_FILE_OPEN_WRITE_DEFAULT (AvFileOpenOptions) {.openMode=AV_FILE_OPEN_WRITE, .binary=false, .update=false}
#define AV_FILE_OPEN_WRITE_BINARY_DEFAULT (AvFileOpenOptions) {.openMode=AV_FILE_OPEN_WRITE, .binary=true, .update=false}

bool32 avFileOpen(AvFile file, AvFileOpenOptions mode);

uint64 avFileRead(void* dst, uint64 size, AvFile file);
uint64 avFileWrite(void* src, uint64 size, AvFile file);


void avFileClose(AvFile file);

void avFileHandleDestroy(AvFile file);

void avStringPrintfToFile(AvFile file, AvString format, ...);
void avStringPrintfToFileVA(AvFile file, AvString format, va_list args);
void avStringPrintfToFileDescriptor(AvFileDescriptor out, AvString format, ...);
void avStringPrintfToFileDescriptorVA(AvFileDescriptor out, AvString format, va_list args);

C_SYMBOLS_END
#endif//__AV_FILE__
