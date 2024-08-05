#ifndef __AV_TYPES__
#define __AV_TYPES__

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef nullptr 
#define nullptr (void*){ 0 }
#endif

#ifndef AV_UTILS_DISABLE_TYPEDEFS

#ifdef __INT64_TYPE__
typedef __UINT64_TYPE__ uint64;
typedef __INT64_TYPE__ int64;
typedef __UINT64_TYPE__ bool64;
#endif

#ifdef __INT32_TYPE__
typedef __UINT32_TYPE__ uint32;
typedef __INT32_TYPE__ int32;
typedef __UINT32_TYPE__ bool32;
typedef __UINT32_TYPE__ uint;
#endif

#ifdef __INT16_TYPE__
typedef __UINT16_TYPE__ uint16;
typedef __INT16_TYPE__ int16;
typedef __UINT16_TYPE__ bool16;
#endif

#ifdef __INT8_TYPE__
typedef __UINT8_TYPE__ uint8;
typedef __INT8_TYPE__ int8;
typedef __UINT8_TYPE__ byte;
typedef __INT8_TYPE__ sbyte;
typedef __UINT8_TYPE__ bool8;
typedef uint8* AvAddress;
#endif

#else //AV_UTILS_DISABLE_TYPEDEFS

#ifdef __INT64_TYPE__
#define uint64 __UINT64_TYPE__
#define int64 __INT64_TYPE__
#define bool64 __UINT64_TYPE__
#endif

#ifdef __INT32_TYPE__
#define uint32 __UINT32_TYPE__
#define int32 __INT32_TYPE__
#define bool32 __UINT32_TYPE__
#endif

#ifdef __INT16_TYPE__
#define uint16 __UINT16_TYPE__
#define int16 __INT16_TYPE__
#define bool16 __UINT16_TYPE__
#endif

#ifdef __INT8_TYPE__
#define uint8 __UINT8_TYPE__
#define int8 __INT8_TYPE__
#define byte __UINT8_TYPE__
#define sbyte __INT8_TYPE__
#define bool8 __UINT8_TYPE__
#define AvAddress uint8*
#endif

#endif//AV_UTILS_DISABLE_TYPEDEFS

typedef void (*AvDeallocateElementCallback)(void*, uint64);
typedef void (*AvDestroyElementCallback)(void*);
typedef uint64 strOffset;

#endif//__AV_TYPES__