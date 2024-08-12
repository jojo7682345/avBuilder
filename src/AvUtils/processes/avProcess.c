#include <AvUtils/avProcess.h>
#include <AvUtils/avMemory.h>
#include <AvUtils/avString.h>
#include <AvUtils/avLogging.h>
#include <AvUtils/dataStructures/avDynamicArray.h>
#include <AvUtils/dataStructures/avArray.h>
#include <AvUtils/avFileSystem.h>

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

typedef uint64 PID;

typedef struct AvProcess_T {
    PID pid;
#ifdef _WIN32
    STARTUPINFO siStartInfo;
    PROCESS_INFORMATION piProcInfo;
#else

#endif
} AvProcess_T;

void avProcessStartInfoPopulateARR(AvProcessStartInfo* info, AvString bin, AvString cwd, uint32 count, AvString* args){
    avProcessStartInfoCreate(info, bin, cwd, count, args);
}

void avProcessStartInfoPopulate_(AvProcessStartInfo* info, AvString bin, AvString cwd, ...) {
    AvDynamicArray arr = AV_EMPTY;
    avDynamicArrayCreate(0, sizeof(AvString), &arr);
    va_list args;
    va_start(args, cwd);
    do {
        AvString arg = va_arg(args, AvString);
        if (arg.chrs == nullptr || arg.len == 0) {
            break;
        }
        avDynamicArrayAdd(&arg, arr);
    } while (1);
    va_end(args);

    uint32 argCount = avDynamicArrayGetSize(arr);
    AvString* argList = avCallocate(argCount, sizeof(AvString), "allocating args");
    avDynamicArrayForEachElement(AvString, arr, {
        memcpy(argList + index, &element, sizeof(AvString));
        });
    avDynamicArrayDestroy(arr);
    avProcessStartInfoCreate(info, bin, cwd, argCount, argList);
    avFree(argList);
}

void avProcessStartInfoCreate(AvProcessStartInfo* info, AvString bin, AvString cwd, uint32 argCount, AvString* argValues) {
    avAssert(info != nullptr, "info must be a valid pointer");
    avAssert(bin.len != 0, "executable cannot be empty");

    avStringClone(&info->executable, bin);
    if (cwd.len != 0) {
        avStringClone(&info->workingDirectory, cwd);
    }

    avArrayAllocate(argCount, sizeof(AvString), &info->args);
    for (uint32 i = 0; i < argCount; i++) {
        avStringClone(((AvString*)info->args.data) + i, argValues[i]);
    }
}

void avProcessStartInfoDestroy(AvProcessStartInfo* info) {
    avAssert(info != nullptr, "info must be a valid pointer");
    avStringFree(&info->executable);
    avStringFree(&info->workingDirectory);
    avArrayFreeAndDestroy(&info->args, (AvDestroyElementCallback)avStringFree);
}


static void configureProcess(AvProcessStartInfo info, AvProcess process);
static bool32 executeProcess(AvProcessStartInfo info, AvProcess process);
bool32 waitForProcess(AvProcess process, int32* exitStatus);
static void killProcess(AvProcess process);

bool32 avProcessStart(AvProcessStartInfo info, AvProcess* process) {
    (*process) = avCallocate(1, sizeof(AvProcess_T), "allocating process");
    configureProcess(info, *process);
    return executeProcess(info, *process);
}

int32 avProcessRun(AvProcessStartInfo info) {
    AvProcess process = AV_EMPTY;
    if (!avProcessStart(info, &process)) {
        return -1;
    }
    int32 ret = avProcessWaitExit(process);
    avProcessDiscard(process);
    return ret;
}

int32 avProcessWaitExit(AvProcess process) {
    int32 exitStatus = -1;
    if (!waitForProcess(process, &exitStatus)) {
        avAssert(false, "failed to get process status, pobably an invalid process handle");
        return exitStatus;
    }
    return exitStatus;
}
void avProcessKill(AvProcess process) {
    killProcess(process);
    avProcessDiscard(process);
}
void avProcessDiscard(AvProcess process) {
    avFree(process);
}

#ifdef _WIN32
static LPSTR GetLastErrorAsString(void) {

    DWORD errorMessageId = GetLastError();
    avAssert(errorMessageId != 0, "");

    LPSTR messageBuffer = NULL;

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, // DWORD   dwFlags,
        NULL, // LPCVOID lpSource,
        errorMessageId, // DWORD   dwMessageId,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // DWORD   dwLanguageId,
        (LPSTR)&messageBuffer, // LPTSTR  lpBuffer,
        0, // DWORD   nSize,
        NULL // va_list *Arguments
    );

    return messageBuffer;
}



static void configureProcess(AvProcessStartInfo startInfo, AvProcess process) {
    ZeroMemory(&process->siStartInfo, sizeof(process->siStartInfo));
    process->siStartInfo.cb = sizeof(STARTUPINFO);
    process->siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    process->siStartInfo.hStdOutput = (startInfo.output != nullptr) ? (HANDLE)_get_osfhandle(*startInfo.output) : GetStdHandle(STD_OUTPUT_HANDLE);
    process->siStartInfo.hStdInput = (startInfo.input != nullptr) ? (HANDLE)_get_osfhandle(*startInfo.input) : GetStdHandle(STD_INPUT_HANDLE);
    process->siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&process->piProcInfo, sizeof(PROCESS_INFORMATION));
}

static bool32 executeProcess(AvProcessStartInfo info, AvProcess process) {
    avStringDebugContextStart;
    AvStringMemory memory = AV_EMPTY;
    uint32 commandLength = 0;
    commandLength += info.executable.len;
    avArrayForEachElement(AvString, arg, i, &info.args, {
        commandLength += 1 + arg.len;
        });
    avStringMemoryAllocate(commandLength, &memory);
    avStringMemoryStore(info.executable, 0, AV_STRING_FULL_LENGTH, &memory);
    uint32 index = info.executable.len;
    avArrayForEachElement(AvString, arg, i, &info.args, {
        avStringMemoryStore(AV_STR(" ", 1), index++, 1, &memory);
        avStringMemoryStore(arg, index, AV_STRING_FULL_LENGTH, &memory);
        index += arg.len;
        });
    AvString cmd = AV_EMPTY;
    avStringFromMemory(&cmd, AV_STRING_WHOLE_MEMORY, &memory);

    int32 bSuccess = CreateProcessA(
        NULL,
        (char*)cmd.chrs,
        NULL,
        NULL, 
        TRUE,
        0,
        NULL,
        info.workingDirectory.chrs,
        (LPSTARTUPINFOA)&process->siStartInfo,
        &process->piProcInfo
    );

    avStringFree(&cmd);
    if (!bSuccess) {
        printf("Could not create child process %s: %s\n",
            cmd.chrs, GetLastErrorAsString());
        avStringDebugContextEnd;
        return false;
    }
    CloseHandle(process->piProcInfo.hThread);
    process->pid = (PID)(process->piProcInfo.hProcess);

    avStringDebugContextEnd;
    return true;
}

static void killProcess(AvProcess process) {
    TerminateProcess((HANDLE)(process->pid), 0);
}


bool32 waitForProcess(AvProcess process, int32* exitStatus) {
    DWORD result = WaitForSingleObject(
        (HANDLE)process->pid,     // HANDLE hHandle,
        INFINITE // DWORD  dwMilliseconds
    );

    if (result == WAIT_FAILED) {
        //TODO: add logging
        printf("could not wait on child process: %s", GetLastErrorAsString());
        return false;
    }

    if (GetExitCodeProcess((HANDLE)process->pid, (LPDWORD)exitStatus) == 0) {
        printf("could not get process exit code: %lu", GetLastError());
        return false;
    }

    CloseHandle((HANDLE)process->pid);
    return true;
}

#else

static void killProcess(AvProcess process){

}
static void configureProcess(AvProcessStartInfo info, AvProcess process) {
    
    //TODO: implement
    return;
}

bool32 waitForProcess(AvProcess process, int32* exitStatus) {

    while(0 == waitpid(process->pid , exitStatus , WNOHANG)){}
    return true;
}

static bool32 executeProcess(AvProcessStartInfo info, AvProcess process) {
    PID cpid = fork();
    if (cpid < 0) {
         avStringPrintf(AV_CSTR("Could not fork child process: %s: %0s\n"), info.executable, strerror(errno));
         return false;
    }

    if (cpid == 0) {

        
        //TODO: piping in and output
        if(info.output){
            close(STDOUT_FILENO); 
            dup2(*info.output, STDOUT_FILENO);
            close(*info.output);
        }  
        if(info.input){
            close(STDIN_FILENO);   //closing stdin
            dup2(*info.input, STDIN_FILENO); 
            close(*info.input);  
        }
        char** args = avCallocate(info.args.count+1, sizeof(char*), "args");
        for(uint32 i = 0; i < info.args.count; i++){
             args[i] = (char*)((AvString*)info.args.data)[i].chrs;
        }
        args[info.args.count] = 0;

        if (execvp(args[0], args) < 0) {
            printf("Could not exec child process: %s: %s\n",
                  args[0], strerror(errno));
            avFree(args);
            exit(-1);
        }

        close(STDOUT_FILENO);
        close(STDIN_FILENO);
        close(STDERR_FILENO);
        if(info.output){
            close(*info.output);
        }
        if(info.input){
            close(*info.input);
        }

        avFree(args);
        exit(0);
    }else{
        process->pid = cpid;
    }

    return true;//cpid;
}

#endif