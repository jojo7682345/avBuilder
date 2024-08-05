#ifndef __AV_GRID__
#define __AV_GRID__
#include "../avDefinitions.h"
C_SYMBOLS_START

#include "../avTypes.h"

typedef struct AvGrid_T* AvGrid;

/// <summary>
/// creates a grid instance
/// </summary>
/// <param name="elementSize">: the size of the elements in the grid</param>
/// <param name="width">: the  width of the grid</param>
/// <param name="height">: the height of the grid</param>
/// <param name="grid">: the grid handle</param>
void avGridCreate(uint64 elementSize, uint32 width, uint32 height, AvGrid* grid);

/// <summary>
/// write to the grid
/// </summary>
/// <param name="data"> pointer of the data you want to be written</param>
/// <param name="x"></param>
/// <param name="y"></param>
/// <param name="grid"> the grid handle</param>
void avGridWrite(void* data, uint32 x, uint32 y, AvGrid grid);

/// <summary>
/// read from the grid
/// </summary>
/// <param name="data"> pointer to where the data needs to be stored </param>
/// <param name="x"></param>
/// <param name="y"></param>
/// <param name="grid">: the grid handle</param>
void avGridRead(void* data, uint32 x, uint32 y, AvGrid grid);

/// <summary>
/// clear the grid
/// </summary>
/// <param name="data">: clear value</param>
/// <param name="grid">: the grid handle</param>
void avGridClear(void* data, AvGrid grid);

/// <summary>
/// gets the pointer to a cell in the grid
/// </summary>
/// <param name="x"></param>
/// <param name="y"></param>
/// <param name="grid">: the grid handle</param>
/// <returns>the pointer to the data in the cell</returns>
void* avGridGetPtr(uint32 x, uint32 y, AvGrid grid);

/// <summary>
/// destroys the grid instance
/// </summary>
/// <param name="grid"></param>
void aGridDestroy(AvGrid grid);

uint32 avGridGetWidth(AvGrid grid);
uint32 avGridGetHeight(AvGrid grid);
uint64 avGridGetElementSize(AvGrid grid);

void avGridWriteGrid(AvGrid src, uint32 x, uint32 y, uint32 width, uint32 height, AvGrid dst);



void avGridFindMinRectSurrounding(void* data, uint32* xMin, uint32* yMin, uint32* xMax, uint32* yMax, AvGrid grid);

C_SYMBOLS_END
#endif//__AV_GRID__
