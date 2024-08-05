#ifndef __AV_MATH__
#define __AV_MATH__
#include "avDefinitions.h"
C_SYMBOLS_START

#include "avTypes.h"

#define AV_MAX(a, b) ((a) > (b) ? (a) : (b))
#define AV_MIN(a, b) ((a) < (b) ? (a) : (b))

#define AV_CLAMP(x, min, max) (AV_MIN(AV_MAX((x),(min)),(max)))

C_SYMBOLS_END
#endif//__AV_MATH__
