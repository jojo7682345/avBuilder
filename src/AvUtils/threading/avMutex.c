#include <AvUtils/threading/avMutex.h>
#include <AvUtils/avMemory.h>

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

typedef struct AvMutex_T {
#ifdef _WIN32
	HANDLE mutexHandle;
#else
	pthread_mutex_t mutex;
#endif
} AvMutex_T;

static bool32 createMutex(AvMutex mutex);
static void destroyMutex(AvMutex mutex);
static void lockMutex(AvMutex mutex);
static void unlockMutex(AvMutex mutex);

bool32 avMutexCreate(AvMutex* mutex) {
	(*mutex) = avCallocate(1, sizeof(AvMutex_T), "allocating mutex");

	return createMutex(*mutex);
}

void avMutexDestroy(AvMutex mutex) {
	destroyMutex(mutex);

	avFree(mutex);
}

void avMutexLock(AvMutex mutex) {

	lockMutex(mutex);
}

void avMutexUnlock(AvMutex mutex) {
	unlockMutex(mutex);
}

#ifdef _WIN32
static bool32 createMutex(AvMutex mutex) {
	(mutex)->mutexHandle = CreateMutex(NULL, FALSE, NULL);
	if ((mutex)->mutexHandle == NULL) {
		printf("CreateMutex error: %ld\n", GetLastError());
		return false;
	}
	return true;
}

static void destroyMutex(AvMutex mutex) {
	CloseHandle(mutex->mutexHandle);
}

static void lockMutex(AvMutex mutex) {
	WaitForSingleObject(mutex->mutexHandle, INFINITE);
}
static void unlockMutex(AvMutex mutex) {
	ReleaseMutex(mutex->mutexHandle);
}
#else
static bool32 createMutex(AvMutex mutex) {
	// no unix specific initialisation is required.
	return true;
}
static void destroyMutex(AvMutex mutex) {
	// do nothing no unix specific initialisation is required.
}

static void lockMutex(AvMutex mutex) {
	pthread_mutex_lock(&mutex->mutex);
}
static void unlockMutex(AvMutex mutex) {
	pthread_mutex_unlock(&mutex->mutex);
}
#endif
