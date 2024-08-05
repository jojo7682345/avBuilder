#if(false) /* BOOTSTRAP_CIRCUIT */
#!/bin/sh
echo compiling bootstrap mechanism
mkdir bootstrap
cd bootstrap
self=$(basename "$0")
gcc -o tmp -x c ../$self
echo compiled bootstrap mechanism
echo bootstrapping
./tmp $@
ret=$?
if [ $ret == 0 ]; then
    echo boostrapping successfull
else
    echo bootstrapping failed
fi
rm tmp
cd ..
rm -d -r bootstrap
exit $ret
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>    
#include "include/AvUtils/avTypes.h"

#define CC "gcc"
#define CFLAGS "-std=c11 -Wall -ggdb -fPIC -lm"
#define INCLUDES "include"
#define PROGRAM "avBuilder"

typedef struct File {
    const char* output;
    const char* source;
} File;

#define SOURCE_FILE(dir, file) (File){ file ".o", dir "/" file ".c" }

int compileFile(File file, unsigned int index, unsigned int max) {
    char buffer[2048] = { 0 };
    sprintf(buffer, "%s %s -c -o %s ../%s -I../include", CC, CFLAGS, file.output, file.source);
    printf("[%u/%u] compiling %s\n", index, max, file.source);
    int result = system(buffer);
    return result;
}

int linkFile(const char* output, unsigned int fileCount, const File files[], unsigned int index, unsigned int max){
    char buffer[2048] = { 0 };
    char objects[1024] = { 0 };
    
    int offset = 0;
    for(uint32 i = 0; i < fileCount; i++){
        offset += sprintf(objects + offset, "%s ", files[i].output);
    }

    sprintf(buffer, "%s %s -o ../%s %s ", CC, CFLAGS, output, objects);
    printf("[%u/%u] linking %s\n", index, max, output);
    int result = system(buffer);
    return result;
}


int main(int argC, char* argV[]) {

    bool32 failed = false;
    const File bootstrapFiles[] = {
        SOURCE_FILE("src/AvUtils/threading",        "avThread"),
        SOURCE_FILE("src/AvUtils/threading",        "avMutex"),
        SOURCE_FILE("src/AvUtils/memory",           "avMemory"),
        SOURCE_FILE("src/AvUtils/processes",        "avProcess"),
        SOURCE_FILE("src/AvUtils/logging",          "avAssert"),
        SOURCE_FILE("src/AvUtils/dataStructures",   "avDynamicArray"),
        SOURCE_FILE("src/AvUtils/dataStructures",   "avArray"),
        SOURCE_FILE("src/AvUtils/string",           "avChar"),
        SOURCE_FILE("src/AvUtils/string",           "avString"),
        SOURCE_FILE("src/AvUtils/filesystem",       "avFile"),
        SOURCE_FILE("src/AvUtils/filesystem",       "avDirectoryV2"),
        SOURCE_FILE("src/AvUtils/string",           "avPrintf"),
        SOURCE_FILE("src/AvUtils/memory",           "avAllocator"),
        SOURCE_FILE("src/AvUtils/memory",           "avDynamicAllocator"),
        SOURCE_FILE("src/AvUtils/memory",           "avLinearAllocator"),
        SOURCE_FILE("src/AvUtils/dataStructures",   "avStream"),
        SOURCE_FILE("src/AvUtils/string",           "avRegex"),
        SOURCE_FILE("src/AvBuilder",                "avProjectLoader"),
        SOURCE_FILE("src/AvBuilder",                "avProjectTokenizer"),
        SOURCE_FILE("src/AvBuilder",                "avProjectParser"),
        SOURCE_FILE("src/AvBuilder",                "avProjectProcessor"),
        SOURCE_FILE("src/AvBuilder",                "avProjectRunner"),
        SOURCE_FILE("src/AvBuilder/builtIn",        "avBuilderBuiltIn"),
        SOURCE_FILE("src/AvBuilder",                "avBuilder"),
    };
    const uint32 bootstrapFilesCount = sizeof(bootstrapFiles)/sizeof(File);
    for(uint32 i = 0; i < bootstrapFilesCount; i++){
        if(compileFile(bootstrapFiles[i],i+1,bootstrapFilesCount+1)!=0){
            failed = true;
        }
    }
    if(linkFile(PROGRAM, bootstrapFilesCount, bootstrapFiles, bootstrapFilesCount+1, bootstrapFilesCount + 1)!=0){
        failed = true;
    }

    if(failed){
        return failed;
    }

    if(argC < 2){
        return 0;
    }

    if(strcmp(argV[1],"run")!=0){
        printf("only \'run\' supported\n");
        return -1;
    }

    int ret = chdir("..");

    pid_t my_pid = 0;
    int status = 0, timeout = 0;
    argV[1] = "/win/shared/usr/avUtilsV2/avBuilder";
    
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);

    if(0 == (my_pid = fork())){
        
        if(-1 == execv(argV[1],argV+1)){
            perror("running failed [%m]");
            return -1;
        }
    }

    timeout = 100;
    
    while (0 == waitpid(my_pid , &status , WNOHANG)) {
        gettimeofday(&t2, NULL);

        if((t2.tv_sec - t1.tv_sec) >= timeout){
            perror("timeout");
            return -1;
        }         
        usleep(1000);
    }

    if (1 != WIFEXITED(status) || 0 != WEXITSTATUS(status)) {
        perror("failed, halt system");
        return -1;
    }

    return 0;

}