#include <AvUtils/process/avPipe.h>
#include <stdio.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/types.h>
#endif

static void createPipe(AvPipe* pipe);
static void destroyPipe(AvPipe* pipe);
static void consumeWriteChannel(AvPipe* pipe);
static void consumeReadChannel(AvPipe* pipe);

void avPipeCreate(AvPipe* pipe) {
    createPipe(pipe);
}

void avPipeConsumeWriteChannel(AvPipe* pipe){
    consumeWriteChannel(pipe);
}

void avPipeConsumeReadChannel(AvPipe* pipe){
    consumeReadChannel(pipe);
}

void avPipeDestroy(AvPipe* pipe) {
    destroyPipe(pipe);
}


#ifdef _WIN32
static void createPipe(AvPipe* pipe) {
    HANDLE readPipe = INVALID_HANDLE_VALUE;
    HANDLE writePipe = INVALID_HANDLE_VALUE;

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    CreatePipe(&readPipe, &writePipe, &saAttr, 0);
    pipe->write = _open_osfhandle((intptr_t)writePipe, _O_WRONLY);
    pipe->read = _open_osfhandle((intptr_t)readPipe, _O_RDONLY);
}
static void destroyPipe(AvPipe* pipe) {
    CloseHandle((HANDLE)_get_osfhandle(pipe->write));
    CloseHandle((HANDLE)_get_osfhandle(pipe->read));
}

static void closePipe(AvPipe* pipe, uint32 index) {
    AvFileDescriptor handles[2] = { pipe->read, pipe->write };

    HANDLE handle = (HANDLE)_get_osfhandle(handles[index]);
    CloseHandle(handle);
}
static void consumeWriteChannel(AvPipe* pipe){
    closePipe(pipe, 1);
}
static void consumeReadChannel(AvPipe* pipe){
    closePipe(pipe,0);
}


#else
static void createPipe(AvPipe* handle) {
    int fd[2];
    pipe(fd);
    handle->read = fd[0];
    handle->write = fd[1];
}
static void destroyPipe(AvPipe* handle) {
    close(handle->read);
    close(handle->write);
}

static void consumeWriteChannel(AvPipe* handle){
    close(handle->write);
}
static void consumeReadChannel(AvPipe* handle){
    close(handle->read);
}
#endif