#include <AvUtils/filesystem/avFile.h>
#include <AvUtils/avMemory.h>

#define AV_DYNAMIC_ARRAY_EXPOSE_MEMORY_LAYOUT
#include <AvUtils/dataStructures/avDynamicArray.h>
#define _POSIX_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#define F_OK 0
#define access _access
#define stat _stat
#define S_IFREG _S_IFREG
#else
#include <unistd.h>

#endif

typedef struct AvFile_T {
	AvFileNameProperties nameProperties;
	AvFileStatus status;
	FILE* filehandle;
	bool32 statted;
	struct stat* stats;
#ifdef _WIN32
#define getFileDescriptor _fileno
#else
#define getFileDescriptor fileno
#endif
} AvFile_T;

const AvFileDescriptor avStdIn = 0;
const AvFileDescriptor avStdOut = 1;
const AvFileDescriptor avStdErr = 2;

void avFileBuildPathVAR_(const char* fileName, AvStringRef filePath, ...) {
	AvDynamicArray arr;
	avDynamicArrayCreate(0, sizeof(AvString), &arr);
	va_list args;
	va_start(args, filePath);
	char* arg;
	while ((arg = va_arg(args, char*)) != nullptr) {
		AvString currentArg = AV_CSTR(arg);
		avDynamicArrayAdd(&currentArg, arr);
	}
	va_end(args);
	avDynamicArrayMakeContiguous(arr);

	avFileBuildPathARR(fileName, filePath, avDynamicArrayGetSize(arr), avDynamicArrayGetPageDataPtr(0, arr));

	avDynamicArrayDestroy(arr);
}

void avFileBuildPathARR(const char* fileName, AvStringRef filePath, uint32 directoryCount, AvString directories[]) {

	AvString fileNameStr = AV_CSTR(fileName);

	uint64 length = 0;
	for (uint i = 0; i < directoryCount; i++) {
		length += directories[i].len + 1;
	}
	length += fileNameStr.len;
	AvStringHeapMemory strMem;
	avStringMemoryHeapAllocate(length, &strMem);

	uint64 offset = 0;
	for (uint32 i = 0; i < directoryCount; i++) {
		avStringMemoryStore(directories[i], offset, directories[i].len, strMem);
		offset += directories[i].len;
		avStringMemoryStore(AV_STR("/", 1), offset++, 1, strMem);
	}
	avStringMemoryStore(fileNameStr, offset, fileNameStr.len, strMem);
	avStringFromMemory(filePath, 0, AV_STRING_FULL_LENGTH, strMem);

}

void avFileHandleCreate(AvString filePath, AvFile* file) {

	(*file) = avCallocate(1, sizeof(AvFile_T), "allocating file handle");
	(*file)->status = AV_FILE_STATUS_CLOSED;
	(*file)->stats = avCallocate(1, sizeof(struct stat), "allocating file stat");
	AvString filePathStr = filePath;

	AvFileNameProperties* nameProperties = &(*file)->nameProperties;
	avStringMemoryAllocate(filePathStr.len, &nameProperties->fileStr);
	avStringMemoryStore(filePathStr, 0, AV_STRING_FULL_LENGTH, &nameProperties->fileStr);
	avStringFromMemory(&nameProperties->fileFullPath, 0, AV_STRING_FULL_LENGTH, &nameProperties->fileStr);

	avStringReplaceChar(&nameProperties->fileFullPath, '\\', '/');

	strOffset filePathLen = avStringFindLastOccuranceOfChar(nameProperties->fileFullPath, '/');
	if (filePathLen++ == AV_STRING_NULL) {
		filePathLen = 0;
	} else {
		avStringFromMemory(&nameProperties->filePath, 0, filePathLen, &nameProperties->fileStr);
	}

	avStringFromMemory(&nameProperties->fileName, filePathLen, AV_STRING_FULL_LENGTH, &nameProperties->fileStr);

	strOffset fileExtOffset = avStringFindLastOccuranceOfChar(nameProperties->fileName, '.');
	if (fileExtOffset == AV_STRING_NULL) {
		fileExtOffset = 0;
	} else {
		avStringFromMemory(&nameProperties->fileExtension, filePathLen + fileExtOffset, AV_STRING_FULL_LENGTH, &nameProperties->fileStr);
	}
	avStringFromMemory(&nameProperties->fileNameWithoutExtension, filePathLen, fileExtOffset, &nameProperties->fileStr);
}

AvFileNameProperties* avFileHandleGetFileNameProperties(AvFile file) {
	return &file->nameProperties;
}

#define statProp(prop) (offsetof(struct stat, prop))
static void* getFileStat(AvFile file, uint64 offset) {
	if (!file->statted) {
		stat(file->nameProperties.fileFullPath.chrs, file->stats);
		file->statted = true;
	}
	return (void*)(((byte*)file->stats) + offset);
}


bool32 avFileExists(AvFile file) {
	if (access(file->nameProperties.fileFullPath.chrs, F_OK) == 0) {
		return true;
	} else {
		return false;
	}
}

bool32 avFileOpen(AvFile file, AvFileOpenOptions mode) {

	char openMode[4] = { 0 };
	uint wi = 1;
	(mode.openMode == AV_FILE_OPEN_READ) ? openMode[0] = 'r' : 0;
	(mode.openMode == AV_FILE_OPEN_WRITE) ? openMode[0] = 'w' : 0;
	(mode.openMode == AV_FILE_OPEN_APPEND) ? openMode[0] = 'a' : 0;
	(mode.update == true) ? openMode[wi++] = '+' : 0;
	(mode.binary = true) ? openMode[wi++] = 'b' : 0;

	file->filehandle = fopen(file->nameProperties.fileFullPath.chrs, openMode);
	if (file->filehandle == NULL) {
		file->status = AV_FILE_STATUS_CLOSED;
		return false;
	}

	file->status =
		(AV_FILE_STATUS_OPEN_READ * (mode.openMode == AV_FILE_OPEN_READ)) |
		(AV_FILE_STATUS_OPEN_WRITE * (mode.openMode == AV_FILE_OPEN_WRITE))|
		(AV_FILE_STATUS_OPEN_WRITE * (mode.openMode == AV_FILE_OPEN_APPEND));

	return true;
}

AvFileStatus avFileGetStatus(AvFile file) {
	return file->status;
}
AvFileDescriptor avFileGetDescriptor(AvFile file){
	if(file->status == AV_FILE_STATUS_CLOSED || file->status == AV_FILE_STATUS_UNKNOWN ){
		return AV_FILE_DESCRIPTOR_NULL;
	}
	return getFileDescriptor(file->filehandle);
}

bool32 avFileDelete(AvFile file){
	return remove(file->nameProperties.fileFullPath.chrs)==0;
}

static AvDateTime getFileTimeStat(AvFile file, uint64 offset) {
	time_t time = *(time_t*)getFileStat(file, offset);
	struct tm* dateTime = gmtime(&time);
	AvDateTime result = { 0 };
	result.second = (uint8)dateTime->tm_sec;
	result.minute = (uint8)dateTime->tm_min;
	result.hour = (uint8)dateTime->tm_hour;
	result.day = (uint8)dateTime->tm_mday;
	result.month = (uint8)dateTime->tm_mon;
	result.year = (uint8)dateTime->tm_year + 1900;
	return result;
}

AvDateTime avFileGetCreationTime(AvFile file) {
	return getFileTimeStat(file, statProp(st_ctime));
}

AvDateTime avFileGetAccessedTime(AvFile file) {
	return getFileTimeStat(file, statProp(st_atime));
}

AvDateTime avFileGetModifiedTime(AvFile file) {
	return getFileTimeStat(file, statProp(st_mtime));
}

uint64 avFileGetSize(AvFile file) {
	// TODO: fix for 32 bit machines and 64 bit machines

	return *(uint32*)getFileStat(file, statProp(st_size));
}

uint64 avFileRead(void* dst, uint64 size, AvFile file) {

	if ((file->status & (AV_FILE_STATUS_OPEN_READ | AV_FILE_STATUS_OPEN_UPDATE)) == 0) {
		return 0;
	}
	return fread(dst, 1, size, file->filehandle);
}

uint64 avFileWrite(void* src, uint64 size, AvFile file) {
	if ((file->status & (AV_FILE_STATUS_OPEN_WRITE | AV_FILE_STATUS_OPEN_UPDATE)) == 0) {
		return 0;
	}
	return fwrite(src, size, 1, file->filehandle);
}


void avFileClose(AvFile file) {
	if (file->status <= AV_FILE_STATUS_CLOSED) {
		return;
	}
	if (file->filehandle) {
		fclose(file->filehandle);
		file->filehandle = nullptr;
		file->status = AV_FILE_STATUS_CLOSED;
	}
}

void avFileHandleDestroy(AvFile file) {
	if (file->status > AV_FILE_STATUS_CLOSED) {
		avFileClose(file);
	}
	avFree(file->stats);
	avStringFree(&file->nameProperties.fileNameWithoutExtension);
	avStringFree(&file->nameProperties.fileExtension);
	avStringFree(&file->nameProperties.fileName);
	avStringFree(&file->nameProperties.filePath);
	avStringFree(&file->nameProperties.fileFullPath);
	avStringMemoryFree(&file->nameProperties.fileStr);
	file->status = AV_FILE_STATUS_UNKNOWN;
	avFree(file);
}

#ifdef _WIN32

#else

#endif
