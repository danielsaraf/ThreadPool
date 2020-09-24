//Daniel Saraf 311312045
#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <stdlib.h>
#include <pthread.h>
#include "osqueue.h"
#include <stdio.h>

typedef struct {
    void (*computeFunc)(void *);
    void *param;
} Task;

typedef struct thread_pool {
    int numberOfThreads;
    int destroyed;
    int* available;
    OSQueue* tasksQueue;
    pthread_mutex_t* mutex;
    pthread_t *threads;
    pthread_cond_t* cond;
} ThreadPool;

ThreadPool *tpCreate(int numOfThreads);

void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param);

#endif
