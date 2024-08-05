#ifndef __AV_DEFINITIONS__
#define __AV_DEFINITIONS__


#ifdef __cplusplus
#define C_SYMBOLS_START extern "C" {
#define C_SYMBOLS_END };
#else
#define C_SYMBOLS_START
#define C_SYMBOLS_END
#endif

#define AV_DS(ds, ...) ds

#define AV_NULL_OPTION
#define AV_EMPTY {0}

#endif//__AV_DEFINITIONS__