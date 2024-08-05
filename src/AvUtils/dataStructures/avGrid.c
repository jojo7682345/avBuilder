#include <AvUtils/dataStructures/avGrid.h>
#include <AvUtils/avMemory.h>
#include <AvUtils/avLogging.h>
#include <AvUtils/avMath.h>
#include <string.h>


typedef struct AvGrid_T {
	const byte* data;
	const uint64 elementSize;
	const uint32 width;
	const uint32 height;
} AvGrid_T;

void avGridCreate(uint64 elementSize, uint32 width, uint32 height, AvGrid* grid) {
	if (elementSize == 0) {
		return;
	}
	if (width == 0 || height == 0) {
		return;
	}
	uint64 elementCount = (uint64)width * (uint64)height;

	(*grid) = avAllocate(sizeof(AvGrid_T), "allocating grid handle");
	AvGrid_T tmpGrid = {
		.data = avCallocate(elementCount, elementSize, "allocating grid memory"),
		.elementSize = elementSize,
		.width = width,
		.height = height
	};
	memcpy(*grid, &tmpGrid, sizeof(AvGrid_T));
}

static bool32 checkBounds(uint32 x, uint32 y, AvGrid grid) {
	return x < grid->width && y < grid->height;
}

static uint64 getIndex(uint32 x, uint32 y, AvGrid grid) {
	return (uint64)y * (uint64)grid->width + (uint64)x;
}

static void* getPtr(uint32 x, uint32 y, AvGrid grid) {
	return (byte*)grid->data + grid->elementSize * getIndex(x, y, grid);
}

static uint64 getElementCount(AvGrid grid) {
	return (uint64)grid->width * (uint64)grid->height;
}

void avGridWrite(void* data, uint32 x, uint32 y, AvGrid grid) {
	if (!checkBounds(x, y, grid)) {
		return;
	}
	if (data == NULL) {
		return;
	}

	memcpy(getPtr(x, y, grid), data, grid->elementSize);
}

void avGridRead(void* data, uint32 x, uint32 y, AvGrid grid) {
	if (!checkBounds(x, y, grid)) {
		return;
	}
	if (data == NULL) {
		return;
	}
	memcpy(data, getPtr(x, y, grid), grid->elementSize);
}

void avGridClear(void* data, AvGrid grid) {
	if (data == NULL) {
		memset((void*)grid->data, 0, getElementCount(grid) * grid->elementSize);
		return;
	}

	for (uint32 y = 0; y < grid->height; y++) {
		for (uint32 x = 0; x < grid->width; x++) {
			avGridWrite(data, x, y, grid);
		}
	}
}

void* avGridGetPtr(uint32 x, uint32 y, AvGrid grid) {
	if (!checkBounds(x, y, grid)) {
		return nullptr;
	}
	return getPtr(x, y, grid);
}

void aGridDestroy(AvGrid grid) {
	avFree((void*)grid->data);
	avFree(grid);
}


uint32 avGridGetWidth(AvGrid grid) {
	return grid->width;
}

uint32 avGridGetHeight(AvGrid grid) {
	return grid->height;
}

uint64 avGridGetElementSize(AvGrid grid) {
	return grid->elementSize;
}

void avGridWriteGrid(AvGrid src, uint32 x, uint32 y, uint32 width, uint32 height, AvGrid dst) {
	avAssert(src != nullptr, "source must be a valid grid");
	avAssert(dst != nullptr, "destination must be a valid grid");
	avAssert(src->elementSize == dst->elementSize, "element size must be equal");
	avAssert(width != 0 || height != 0, "cannot copy zero size region");


	if (x >= dst->width || y >= dst->height) {
		return;
	}
	width = AV_MIN(AV_MIN(width, src->width), x - dst->width);
	height = AV_MIN(AV_MIN(height, src->height), y + dst->height);

	for (uint32 yy = 0; yy < height; yy++) {
		for (uint32 xx = 0; xx < width; xx++) {
			avGridRead(avGridGetPtr(xx + x, yy + y, dst), xx, yy, src);
		}
	}
}


void avGridFindMinRectSurrounding(void* data, uint32* xMin, uint32* yMin, uint32* xMax, uint32* yMax, AvGrid grid) {
	*xMin = grid->width;
	*yMin = grid->height;
	*xMax = 0;
	*yMax = 0;
	void* tmpData = avAllocate(grid->elementSize, "allocating temp data");
	for (uint32 v = 0; v < grid->height; v++) {
		for (uint32 u = 0; u < grid->width; u++) {
			avGridRead(tmpData, u, v, grid);
			if(memcmp(tmpData, data, grid->elementSize)==0){
				*xMin = AV_MIN(*xMin, u);
				*yMin = AV_MIN(*yMin, v);
				*xMax = AV_MAX(*xMax, u);
				*yMax = AV_MAX(*yMax, v);
			}
		}
	}
	avFree(tmpData);
}