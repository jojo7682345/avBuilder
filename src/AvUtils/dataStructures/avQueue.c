#include <AvUtils/dataStructures/avQueue.h>
#include <AvUtils/avMemory.h>
#include <string.h>

typedef struct AvQueue_T {

	byte* data;
	const uint64 elementSize;
	uint64 size;

	uint64 head;
	uint64 length;

} AvQueue_T;

static void* getPtr(AvQueue queue, uint64 index) {
	index %= (queue->size);
	return queue->data + queue->elementSize * index;
}

void avQueueCreate(uint64 elementSize, uint64 queueSize, AvQueue* queue) {
	
	if (elementSize == 0) {
		return;
	}
	if (queueSize == 0) {
		return;
	}

	(*queue) = avAllocate(sizeof(AvQueue_T), "allocating handle for queue");
	memcpy((void*) &((*queue)->elementSize), &elementSize, sizeof(elementSize));
	(*queue)->data = avCallocate(queueSize, elementSize, "allocating queue data");
	(*queue)->size = queueSize;
	(*queue)->head = 0;
	(*queue)->length = 0;
}

void avQueueDestroy(AvQueue queue) {
	avFree(queue->data);
	avFree(queue);
}

uint64 avQueueGetRemainingSpace(AvQueue queue) {
	return queue->size - avQueueGetOccupiedSpace(queue);
}

uint64 avQueueGetOccupiedSpace(AvQueue queue) {
	return queue->length;
}

bool8 avQueueIsFull(AvQueue queue) {
	return queue->length == queue->size;
}

bool8 avQueueIsEmpty(AvQueue queue) {
	return queue->length == 0;
}

bool8 avQueuePush(void* element, AvQueue queue) {
	if (avQueueIsFull(queue)) {
		return 0;
	}
	if (element == NULL) {
		return 0;
	}

	memcpy(getPtr(queue, queue->head+queue->length), element, queue->elementSize);
	queue->length++;
	return 1;
}

bool8 avQueuePull(void* element, AvQueue queue) {
	if (avQueueIsEmpty(queue)) {
		return 0;
	}
	if(element==NULL){
		return 0;
	}
	memcpy(element, getPtr(queue, queue->head), queue->elementSize);
	queue->head++;
	queue->head %= queue->size;
	queue->length--;
	return 1;
}

uint64 avQueueGetSize(AvQueue queue) {
	return queue->size;
}

uint64 avQueueGetElementSize(AvQueue queue) {
	return queue->elementSize;
}

void avQueueClone(AvQueue src, AvQueue* dst){
	avQueueCreate(src->elementSize, src->size, dst);
	memcpy((*dst)->data, src->data, src->size * src->elementSize);
	(*dst)->head = src->head;
	(*dst)->length = src->length;
}

void* avQueueGetTopPtr(AvQueue queue){
	if(avQueueIsEmpty(queue)){
		return nullptr;
	}
	return getPtr(queue, queue->head + queue->length);
}

void* avQueueGetBottomPtr(AvQueue queue) {
	if (avQueueIsEmpty(queue)) {
		return nullptr;
	}
	return getPtr(queue, queue->head);
}
