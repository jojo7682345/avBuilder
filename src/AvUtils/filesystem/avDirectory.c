#include <AvUtils/filesystem/avDirectory.h>
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

static AvPathType pathGetType(AvString str);
// static bool32 directoryExists(AvString str) {
//     return pathGetType(str) == AV_PATH_TYPE_DIRECTORY;
// }

typedef struct Content {
    AvPathType type;
    AvString name;
} Content;

typedef struct ContentList {
    bool32 started;
    AvString path;
#ifdef _WIN32
    HANDLE hFind;
    WIN32_FIND_DATAA  findFileData;
#else
    DIR* dir;
    struct dirent* ent;
#endif
}ContentList;

static void startContentList(AvPathRef path, Content* content, ContentList* list);
static bool32 getNextEntry(Content* content, ContentList* list);
static void endContentList(Content* content, ContentList* list);


static void createContent(Content* content, AvString name, AvPathType type) {
    avStringClone(&content->name, name);
    content->type = type;
}

static void destroyContent(Content* content) {
    avStringFree(&content->name);
    content->type = AV_PATH_TYPE_UNKNOWN;
}

static void processPath(AvStringRef dst, AvString src) {
    avAssert(dst->memory == nullptr, "string must be emtpy");

    AvString str = AV_EMPTY;
    if (src.len == 0 || src.chrs == nullptr) {
        avStringClone(&str, AV_CSTR("."));
        //printf("cloned\n");
    } else {
        avStringClone(&str, src);
        avStringReplaceChar(&str, '\\', '/');
    }

    AvArray paths = AV_EMPTY;
    avStringSplitOnChar(&paths, '/', str);
    //printf("splitted\n");
    uint64 length = 0;
    for (uint32 index = 0; index < paths.count; index++) {
        AvString dir = AV_EMPTY;
        avArrayRead(&dir, index, &paths);
        if (dir.len == 0 && index != 0) {
            continue;
        }
        length += dir.len;
        length += 1;
    }
    length += 1; // for * character, for easy file searching for win32 without having to allocate a new string
    //printf("lengthd %lu\n", length);
    AvStringHeapMemory memory;
    avStringMemoryHeapAllocate(length, &memory);
    uint64 writeIndex = 0;
    for (uint32 i = 0; i < paths.count; i++) {
        AvString dir = AV_EMPTY;
        avArrayRead(&dir, i, &paths);
        if (dir.len == 0 && i != 0) {
            continue;
        }
        avStringMemoryStore(dir, writeIndex, dir.len, memory);
        writeIndex += dir.len;
        avStringMemoryStore(AV_CSTR("/"), writeIndex++, 1, memory);
    }
    avStringMemoryStore(AV_CSTR("*"), writeIndex++, 1, memory);
    if (memory->capacity - 2 == 0) {
        avStringFromMemory(dst, 0, memory->capacity - 1, memory);
    } else {
        avStringFromMemory(dst, 0, memory->capacity - 2, memory);
    }
    //printf("stored\n");
    avArrayFree(&paths);
    //printf("array freed\n");
    avStringFree(&str);
    //printf("freed agained\n");

}

bool32 avPathOpen(AvString str, AvPathRef path) {

    processPath(&path->path, str);
    //printf("processed\n");
    path->type = pathGetType(path->path);
    //printf("typed\n");
    if (path->type == AV_PATH_TYPE_UNKNOWN) {
        avPathClose(path);
        return false;
    }
    return true;
}

bool32 avPathGetFile(AvPathRef path) {
    if (path->type != AV_PATH_TYPE_FILE) {
        return false;
    }
    avFileHandleCreate(path->path, &path->file);
    return true;
}

bool32 avPathGetDirectoryContents(AvPathRef path) {
    if (path->type != AV_PATH_TYPE_DIRECTORY) {
        return false;
    }

    AvDynamicArray contents = AV_EMPTY;
    avDynamicArrayCreate(0, sizeof(AvPath), &contents);

    ContentList list = AV_EMPTY;
    Content content = AV_EMPTY;
    startContentList(path, &content, &list);
    do {
        if (avStringEquals(content.name, AV_CSTR(".")) || avStringEquals(content.name, AV_CSTR(".."))) {
            continue;
        }

        AvString fullPath = AV_EMPTY;
        avStringJoin(&fullPath, path->path, AV_CSTR((path->path.chrs[path->path.len-1]=='/' ? "" : "/")), content.name);
        if (pathGetType(fullPath) == AV_PATH_TYPE_UNKNOWN) {
            avStringFree(&fullPath);
            continue;
        }

        AvPath tmpPath = AV_EMPTY;
        uint32 index = avDynamicArrayAdd(&tmpPath, contents);
        avPathOpen(fullPath, avDynamicArrayGetPtr(index, contents));

        avStringFree(&fullPath);
    } while (getNextEntry(&content, &list));
    endContentList(&content, &list);

    AvDirectory* dir = &path->directory;
    uint32 contentCount = avDynamicArrayGetSize(contents);
    dir->content = avCallocate(contentCount, sizeof(AvPath), "allocating path contents");
    dir->contentCount = contentCount;

    for (uint32 i = 0; i < contentCount; i++) {
        AvPathRef dst = dir->content + i;
        AvPath* src = (AvPath*)avDynamicArrayGetPtr(i, contents);

        if (src->type == AV_PATH_TYPE_UNKNOWN) {
            continue;
        }

        memcpy(dst, src, sizeof(AvPath));
        dst->parent = path;
    }
    dir->explored = true;
    avDynamicArrayDestroy(contents);
    return true;
}

void avPathClose(AvPathRef path) {

    avStringFree(&path->path);

    if (path->type == AV_PATH_TYPE_FILE && path->file != nullptr) {
        avFileHandleDestroy(path->file);
        path->file = nullptr;
        return;
    }

    if (path->type == AV_PATH_TYPE_DIRECTORY && path->directory.explored) {
        for (uint32 i = 0; i < path->directory.contentCount; i++) {
            avPathClose(&path->directory.content[i]);
        }
        avFree(path->directory.content);
        path->directory.content = nullptr;
        path->directory.contentCount = 0;
        path->directory.explored = false;
        return;
    }
}

#ifdef _WIN32

static AvPathType pathGetType(AvString str) {
    AvString path = AV_EMPTY;
    avStringClone(&path, str);
    DWORD attributes = GetFileAttributesA(path.chrs);
    AvPathType type = AV_PATH_TYPE_UNKNOWN;
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        avStringFree(&path);
        type = AV_PATH_TYPE_UNKNOWN;
        return type;
    }

    // if (pathProperties) {
    //     (*pathProperties) = 0;
    //     (*pathProperties) |= ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) ? AV_PATH_PROPERTY_DIR : 0;
    //     (*pathProperties) |= ((attributes & FILE_ATTRIBUTE_READONLY) != 0) ? AV_PATH_PROPERTY_READONLY : 0;
    //     (*pathProperties) |= ((attributes & FILE_ATTRIBUTE_HIDDEN) != 0) ? AV_PATH_PROPERTY_HIDDEN : 0;
    //     (*pathProperties) |= ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) ? AV_PATH_PROPERTY_SYMLINK : 0;
    // }

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        type = AV_PATH_TYPE_DIRECTORY;
        avStringFree(&path);
        return type;
    }

    type = AV_PATH_TYPE_FILE;
    avStringFree(&path);
    return type;
}

static void startContentList(AvPathRef path, Content* content, ContentList* list) {
    avStringFromMemory(&list->path, AV_STRING_WHOLE_MEMORY, path->path.memory);
    list->hFind = FindFirstFileA(list->path.chrs, &list->findFileData);
    if (list->hFind == INVALID_HANDLE_VALUE) {
        list->started = false;
        avStringFree(&list->path);
        return;
    }
    list->started = true;
    createContent(
        content,
        AV_CSTR(list->findFileData.cFileName),
        list->findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? AV_PATH_TYPE_DIRECTORY : AV_PATH_TYPE_FILE
    );
}

static void endContentList(Content* content, ContentList* list) {
    destroyContent(content);

    if (list->started == false) {
        return;
    }
    FindClose(list->hFind);
    avStringFree(&list->path);
    list->started = false;
    list->hFind = 0;
}

static bool32 getNextEntry(Content* content, ContentList* list) {
    avAssert(list->started, "list must be started first");
    destroyContent(content);
    if (FindNextFileA(list->hFind, &list->findFileData) == 0) {
        return false;
    }
    createContent(
        content,
        AV_CSTR(list->findFileData.cFileName),
        list->findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? AV_PATH_TYPE_DIRECTORY : AV_PATH_TYPE_FILE
    );
    return true;
}

#else

static AvPathType pathGetType(AvString str) {
    AvString path = AV_EMPTY;
    avStringClone(&path, str);

    struct stat buffer;
    int result = stat(path.chrs, &buffer);

    AvPathType type = AV_PATH_TYPE_UNKNOWN;
    if (result != 0) {
        avStringFree(&path);
        type = AV_PATH_TYPE_UNKNOWN;
        return type;
    }
    if ((buffer.st_mode & S_IFDIR) != 0) {
        type = AV_PATH_TYPE_DIRECTORY;
        avStringFree(&path);
        return type;
    }

    type = AV_PATH_TYPE_FILE;
    avStringFree(&path);
    return type;
}

static void startContentList(AvPathRef path, Content* content, ContentList* list) {
    avStringClone(&list->path, path->path);
    list->dir = opendir(list->path.chrs);
    if (list->dir == NULL) {
        list->started = false;
        avStringFree(&list->path);
        return;
    }
    list->ent = readdir(list->dir);
    if (list->ent == NULL) {
        list->started = false;
        avStringFree(&list->path);
        return;
    }
    list->started = true;
    createContent(
        content,
        AV_CSTR(list->ent->d_name),
        pathGetType(list->path));
}
static bool32 getNextEntry(Content* content, ContentList* list) {
    avAssert(list->started, "list must be started first");
    destroyContent(content);
    list->ent = readdir(list->dir);
    if (list->ent == NULL) {
        return false;
    }
    createContent(
        content,
        AV_CSTR(list->ent->d_name),
        pathGetType(list->path));
    return true;
}
static void endContentList(Content* content, ContentList* list) {
    destroyContent(content);

    if (list->started == false) {
        return;
    }
    closedir(list->dir);
    avStringFree(&list->path);
    list->started = false;
    list->dir = 0;
}

#endif