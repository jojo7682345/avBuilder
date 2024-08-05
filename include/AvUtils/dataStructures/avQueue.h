#ifndef __AV_QUEUE__
#define __AV_QUEUE__
#include "../avDefinitions.h"
C_SYMBOLS_START

#include "../avTypes.h"

typedef struct AvQueue_T* AvQueue;

/// <summary>
/// creates a queue instance
/// </summary>
/// <param name="elementSize">: the size of the elements in the queue</param>
/// <param name="queueSize">: the amount of elements allocated in the queue</param>
/// <param name="queue">: the queue handle</param>
void avQueueCreate(uint64 elementSize, uint64 queueSize, AvQueue* queue);

/// <summary>
/// destroys the queue instance
/// </summary>
/// <param name="queue"></param>
void avQueueDestroy(AvQueue queue);

/// <summary>
/// returns the amount of available element slots that are not occupied
/// </summary>
/// <param name="queue">: the queue handle</param>
/// <returns>the number of empty slots in the queue</returns>
uint64 avQueueGetRemainingSpace(AvQueue queue);

/// <summary>
/// returns the amount of elements in the queue
/// </summary>
/// <param name="queue">: the queue handle</param>
/// <returns>the number of elements</returns>
uint64 avQueueGetOccupiedSpace(AvQueue queue);

/// <summary>
/// checks if the queue is full
/// </summary>
/// <param name="queue">: the queue handle</param>
/// <returns>1 if the queue is full 0 if it is not</returns>
bool8 avQueueIsFull(AvQueue queue);

/// <summary>
/// checks if the queue is empty
/// </summary>
/// <param name="queue">: the queue handle</param>
/// <returns>1 if the queue is empty, 0 otherwise</returns>
bool8 avQueueIsEmpty(AvQueue queue);

/// <summary>
/// adds an element to the queue
/// </summary>
/// <param name="element">: pointer to the element to be added, (must be a valid element)</param>
/// <param name="queue">: the queue handle</param>
/// <returns>1 if the push was successfull, 0 if not</returns>
bool8 avQueuePush(void* element, AvQueue queue);

/// <summary>
/// retrieves an element from the queue
/// </summary>
/// <param name="element">: pointer to store the element in, (must be a valid element)</param>
/// <param name="queue">: the queue handle</param>
/// <returns>1 if the pull was successfull, 0 otherwise</returns>
bool8 avQueuePull(void* element, AvQueue queue);

/// <summary>
/// returns the allocated number of elements
/// </summary>
/// <param name="queue">: the queue handle</param>
uint64 avQueueGetSize(AvQueue queue);

/// <summary>
/// returns the element size
/// </summary>
/// <param name="queue">: the queue handle</param>
/// <returns></returns>
uint64 avQueueGetElementSize(AvQueue queue);

void avQueueClone(AvQueue src, AvQueue* dst);

void* avQueueGetTopPtr(AvQueue queue);
void* avQueueGetBottomPtr(AvQueue queue);
#define avQueueAccessTop(type, queue) ((type*)avQueueGetTopPtr(queue))
#define avQueueAccessBotom(type, queue) ((type*)avQueueGetBottomPtr(queue))

C_SYMBOLS_END
#endif //__AV_QUEUE__
