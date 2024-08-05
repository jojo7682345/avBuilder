#ifndef __AV_TABLE__
#define __AV_TABLE__
#include "../avDefinitions.h"
C_SYMBOLS_START


#include "../avTypes.h"

typedef struct AvTable_T* AvTable;


void avTableCreateFromArray(uint32 columns, uint32 rows, AvTable* table, uint64* columnSizes);
void avTableCreate(uint32 columns, uint32 rows, AvTable* table, ...);

void avTableWrite(void* data, uint32 column, uint32 row, AvTable table);
void avTableRead(void* data, uint32 column, uint32 row, AvTable table);

uint64 avTableGetRowSize(AvTable table);
uint64 avTableGetColumnSize(uint32 column, AvTable table);

uint64 avTableGetDataSize(uint32 column, AvTable table);
uint64 avTableGetCellSize(uint32 column, uint32 row, AvTable table);

void avTableWriteRow(void* data, uint32 row, AvTable table);
void avTableWriteColumn(void* data, uint32 column, AvTable table);

void avTableReadRow(void* data, uint32 row, AvTable table);
void avTableReadColumn(void* data, uint32 column, AvTable table);

uint32 avTableGetColumns(AvTable table);
uint32 avTableGetRows(AvTable table);

void avTableDestroy(AvTable table);

C_SYMBOLS_END
#endif//__AV_TABLE__