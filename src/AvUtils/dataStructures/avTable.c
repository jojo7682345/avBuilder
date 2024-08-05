#include <AvUtils/dataStructures/avTable.h>
#include <AvUtils/avMemory.h>
#include <string.h>
#include <stdarg.h>

typedef struct AvTable_T {

	uint64* columnSizes;
	uint64* columnOffsets;
	uint32 columns;

	uint32 rows;
	uint64 rowSize;

	void* data;

} AvTable_T;

void avTableCreateFromArray(uint32 columns, uint32 rows, AvTable* table, uint64* columnSizes) {
	if (columns == 0 || rows == 0 || table == nullptr || columnSizes == nullptr) {
		return;
	}

	(*table) = avCallocate(1, sizeof(AvTable_T), "allocating table handle");
	(*table)->columns = columns;
	(*table)->rows = rows;
	(*table)->columnSizes = avAllocate(columns * sizeof(uint64), "allocating collumnsizes");
	(*table)->columnOffsets = avAllocate(columns * sizeof(uint64), "allocating collumn offsets");
	memcpy((*table)->columnSizes, columnSizes, columns * sizeof(uint64));

	uint64 rowSize = 0;
	for (uint i = 0; i < columns; i++) {
		(*table)->columnOffsets[i] = rowSize;
		rowSize += (*table)->columnSizes[i];

	}
	(*table)->rowSize = rowSize;
	(*table)->data = avCallocate(1, rows * rowSize, "allocating table data");
}

void avTableCreate(uint32 columns, uint32 rows, AvTable* table, ...) {
	
	va_list args;
	va_start(args, table);
	uint64* sizes = avCallocate(columns, sizeof(uint64), "allocating collumnsizes");
	for (uint i = 0; i < columns; i++) {
		sizes[i] = va_arg(args, uint64);
	}	
	va_end(args);
	avTableCreateFromArray(columns, rows, table, sizes);
	avFree(sizes);
}

static bool32 checkBounds(uint32 column, uint32 row, AvTable table) {
	if (table->rows <= row || table->columns <= column) {
		return false;
	}
	return true;
}

static void* getPtr(uint32 column, uint32 row, AvTable table) {
	uint64 rowOffset = (uint64)row * table->rowSize;
	uint64 columnOffset = table->columnOffsets[column];

	return (byte*)table->data + rowOffset + columnOffset;
}

void avTableWrite(void* data, uint32 column, uint32 row, AvTable table) {
	if (!checkBounds(column, row, table)) {
		return;
	}
	memcpy(getPtr(column, row, table), data, table->columnSizes[column]);
}

void avTableRead(void* data, uint32 column, uint32 row, AvTable table) {
	if (!checkBounds(column, row, table)) {
		return;
	}
	memcpy(data, getPtr(column, row, table), table->columnSizes[column]);
}

uint64 avTableGetRowSize(AvTable table) {
	return table->rowSize;
}

uint64 avTableGetColumnSize(uint32 column, AvTable table) {
	return table->columnSizes[column] * (uint64)table->rows;
}

uint64 avTableGetDataSize(uint32 column, AvTable table) {
	return table->columnSizes[column];
}

uint64 avTableGetCellSize(uint32 column, uint32 row, AvTable table) {
	return table->columnSizes[column];
}

void avTableWriteRow(void* data, uint32 row, AvTable table) {
	if (!checkBounds(0, row, table)) {
		return;
	}
	memcpy(getPtr(0, row, table), data, table->rowSize);
}

void avTableWriteColumn(void* data, uint32 column, AvTable table) {
	if (!checkBounds(column, 0, table)) {
		return;
	}

	uint64 columnSize = table->columnSizes[column];
	uint64 offset = 0;
	for (uint32 i = 0; i < table->rows; i++) {
		memcpy(getPtr(column, i, table), (byte*)data + offset, columnSize);
	}
}

void avTableReadRow(void* data, uint32 row, AvTable table) {
	if (!checkBounds(0, row, table)) {
		return;
	}
	memcpy(data, getPtr(0, row, table), table->rowSize);
}

void avTableReadColumn(void* data, uint32 column, AvTable table) {
	if (!checkBounds(column, 0, table)) {
		return;
	}

	uint64 columnSize = table->columnSizes[column];
	uint64 offset = 0;
	for (uint32 i = 0; i < table->rows; i++) {
		memcpy((byte*)data + offset, getPtr(column, i, table), columnSize);
	}
}

uint32 avTableGetColumns(AvTable table) {
	return table->columns;
}

uint32 avTableGetRows(AvTable table) {
	return table->rows;
}

void avTableDestroy(AvTable table) {
	avFree(table->columnOffsets);
	avFree(table->columnSizes);
	avFree(table->data);
	avFree(table);
}
