#include <AvUtils/filesystem/avDirectoryV2.h>
#include <AvUtils/avMemory.h>
#define AV_DYNAMIC_ARRAY_EXPOSE_MEMORY_LAYOUT
#include <AvUtils/dataStructures/avDynamicArray.h>
#include <AvUtils/dataStructures/avArray.h>
#include <AvUtils/avLogging.h>
#include <AvUtils/string/avChar.h>

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#define __USE_MISC
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#endif

static AvPathNodeType pathGetType(AvString str) {
    AvString path = AV_EMPTY;
    avStringClone(&path, str);

    struct stat buffer;
    int result = stat(path.chrs, &buffer);

    AvPathNodeType type = AV_PATH_NODE_TYPE_NONE;
    if (result != 0) {
        type = AV_PATH_NODE_TYPE_NONE;
        goto typeFound;
    }
    if ((buffer.st_mode & S_IFDIR) != 0) {
        type = AV_PATH_NODE_TYPE_DIRECTORY;
        goto typeFound;
    }
    type = AV_PATH_NODE_TYPE_FILE;
typeFound:
    avStringFree(&path);
    return type;
}

bool32 avDirectoryOpen(AvString location, AvPath* root, AvPathRef pathRef){
    avStringDebugContextStart;
    AvPath path = {0};
    AvString fullPath = AV_EMPTY;
    if(root){
        path.root = root;
        path.allocator = root->allocator;
        avStringJoin(&fullPath, root->path, AV_CSTR("/"), location);
    }else{
        path.root = nullptr;
        avAllocatorCreate(0, AV_ALLOCATOR_TYPE_DYNAMIC, &path.allocator);
        avStringClone(&fullPath, location);
    }
    avStringReplaceChar(&fullPath, '\\', '/');
    avStringCopyToAllocator(fullPath, &path.path, &path.allocator);
    if(pathGetType(fullPath)!=AV_PATH_NODE_TYPE_DIRECTORY){
        goto pathNotDirectory;
    }

    DIR* dir = opendir(fullPath.chrs);
    if(!dir){
        goto pathNotFound;
    }

    AvDynamicArray entries = nullptr;
    avDynamicArrayCreate(0, sizeof(AvPathNode), &entries);

    struct dirent* entry = readdir(dir);
    while(entry){
        if(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0){
            goto next;
        }

        AvString entryName = {
            .chrs = entry->d_name,
            .len = avCStringLength(entry->d_name),
            .memory = nullptr,
        };
        AvString entryPath = {0};
        avStringJoin(&entryPath, fullPath, AV_CSTR("/"), entryName);
        struct stat stats = {0};
        stat(entryPath.chrs, &stats);
        AvPathNodeType type = AV_PATH_NODE_TYPE_FILE;
        if((stats.st_mode & S_IFDIR) != 0){
            type = AV_PATH_NODE_TYPE_DIRECTORY;
        }
        AvPathNode node = {
            .name = AV_EMPTY,
            .type = type,
        };

        avStringCopyToAllocator(entryPath, &node.fullName, &path.allocator);
        AvString fileName = {
            .chrs = node.fullName.chrs + fullPath.len + 1,
            .len = node.fullName.len - fullPath.len - 1,
            .memory = nullptr,
        };
        memcpy(&node.name, &fileName, sizeof(AvString));
        avStringFree(&entryPath);
        avDynamicArrayAdd(&node, entries);
    next:
        entry = readdir(dir);
    }

    closedir(dir);

    path.contentCount = avDynamicArrayGetSize(entries);
    if(path.contentCount){
        AvPathNode* content = avAllocatorAllocate(sizeof(AvPathNode)*path.contentCount, &path.allocator);
        avDynamicArrayReadRange(content, path.contentCount, 0, sizeof(AvPathNode), 0, entries);
        path.content = content;
    }
    avDynamicArrayDestroy(entries);
    
    avStringFree(&fullPath);
    avStringDebugContextEnd;

    memcpy(pathRef, &path, sizeof(AvPath));
    return true;

pathNotFound:
pathNotDirectory:
    if(!root){
        avAllocatorDestroy(&path.allocator);
    }

    avStringFree(&fullPath);
    avStringDebugContextEnd;

    return false;
}

void avDirectoryClose(AvPathRef path){
    if(path->root){
        return;
    }
    avAllocatorDestroy(&path->allocator);
}