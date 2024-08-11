#ifndef __AV_DIRECTORY__
#define __AV_DIRECTORY__
#include "../avDefinitions.h"
C_SYMBOLS_START

#include "../avTypes.h"
#include "../avString.h"
#include "../memory/avAllocator.h"
#include "avFile.h"

typedef enum AvPathNodeType {
	AV_PATH_NODE_TYPE_NONE = 0,
	AV_PATH_NODE_TYPE_FILE,
	AV_PATH_NODE_TYPE_DIRECTORY,
} AvPathNodeType;

typedef struct AvPathNode {
	AvPathNodeType type;
	AvString name;
	AvString fullName;
}AvPathNode;

typedef struct AvPath {
	AvString path;
	struct AvPath* root;
	AvAllocator allocator;
	uint32 contentCount;
	AvPathNode* content;
} AvPath;
typedef struct AvPath* AvPathRef;

bool32 avDirectoryExists(AvString location);
uint32 avMakeDirectory(AvString location);
uint32 avMakeDirectoryRecursive(AvString location);

bool32 avDirectoryOpen(AvString location, AvPath* root, AvPathRef path);
void avDirectoryClose(AvPathRef path);

C_SYMBOLS_END
#endif//__AV_DIRECTORY__
