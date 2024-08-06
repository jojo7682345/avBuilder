#include "avBuilder.h"
#include <AvUtils/filesystem/avFile.h>
#include <AvUtils/memory/avAllocator.h>
#include <AvUtils/avMemory.h>

bool32 loadProjectFile(const AvString projectFilePath, AvStringRef projectFileContent, AvStringRef projectFileName){
    AvFile file = AV_EMPTY;
    avFileHandleCreate(projectFilePath, &file);
    if(!avFileOpen(file, AV_FILE_OPEN_READ_DEFAULT)){
        avFileHandleDestroy(file);
        return false;
    }
    uint64 size = avFileGetSize(file);

    char* buffer = avCallocate(size+1, 1, "allocating buffer");
    while(avFileRead(buffer, size, file)!=size);
    AvStringMemory strmem = AV_EMPTY;
    strmem.capacity = size;
    strmem.properties.contextAllocationIndex = 0;
    strmem.properties.heapAllocated = false;
    strmem.properties.debugContext = NULL;
    strmem.data = buffer;

    AvString projectFileContentRaw = AV_EMPTY;
    avStringFromMemory(&projectFileContentRaw, AV_STRING_WHOLE_MEMORY, &strmem);
    
    AvString sanitizedProjectFileContent = AV_EMPTY;
    avStringReplace(&sanitizedProjectFileContent, projectFileContentRaw, AV_CSTR("\r\n"), AV_CSTR("\n"));

    AvFileNameProperties* fileNameProps = avFileHandleGetFileNameProperties(file);
    
    avStringClone(projectFileContent, sanitizedProjectFileContent);
    avStringClone(projectFileName, fileNameProps->fileNameWithoutExtension);

    avStringFree(&sanitizedProjectFileContent);
    avStringFree(&projectFileContentRaw);

    avFileClose(file);
    avFileHandleDestroy(file);
    return true;
}