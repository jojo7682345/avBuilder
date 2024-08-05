#ifndef __AV_DIRECTORY__
#define __AV_DIRECTORY__
#include "../avDefinitions.h"
C_SYMBOLS_START

#include "../avTypes.h"
#include "../avString.h"
#include "avFile.h"

typedef enum AvPathType {
	AV_PATH_TYPE_UNKNOWN,
	AV_PATH_TYPE_FILE,
	AV_PATH_TYPE_DIRECTORY,
} AvPathType;

// typedef enum AvPathProperties {
// 	AV_PATH_PROPERTY_NORMAL			= 0,
// 	AV_PATH_PROPERTY_DIR 			= 1 << 0,
// 	AV_PATH_PROPERTY_SYMLINK 		= 1 << 1,
// 	AV_PATH_PROPERTY_HIDDEN			= 1 << 2,
// 	AV_PATH_PROPERTY_READONLY		= 1 << 3,
// } AvPathProperties;
// typedef uint32 AvPathPropertiesFlags;

typedef struct AvDirectory {
	bool32 explored;
	uint32 contentCount;
	struct AvPath* content;
} AvDirectory;

typedef struct AvPath {
	AvPathType type;
	//AvPathPropertiesFlags properties;
	struct AvPath* parent;
	AvString path;
	union {
		AvFile file;
		AvDirectory directory;
	};
} AvPath;
typedef AvPath* AvPathRef; 

bool32 avPathOpen(AvString str, AvPathRef path);

bool32 avPathGetFile(AvPathRef path);
bool32 avPathGetDirectoryContents(AvPathRef path);

void avPathClose(AvPathRef path);

C_SYMBOLS_END
#endif//__AV_DIRECTORY__
