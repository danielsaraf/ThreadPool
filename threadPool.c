//Daniel Saraf 311312045
#include "threadPool.h"
void freeMemory(ThreadPool *);

void exitProgram(ThreadPool *);

int checkThreadPool(ThreadPool *);

// this function run the loop of the threads that take tasks from queue and execute it
void doTasks(ThreadPool *threadPool) {
    Task *t;
    while (1) {
        //Acquire the mutex
        if (pthread_mutex_lock(threadPool->mutex) != 0) {
            perror("Error in mutex_lock");
            exitProgram(threadPool);
        }
        while (1) {
            //Check if the program is shutting down, if so, release the mutex and terminate
            if ((*(threadPool->available) == 0) ||
                ((*(threadPool->available) == 1) && (osIsQueueEmpty(threadPool->tasksQueue)))) {
                if (pthread_mutex_unlock(threadPool->mutex) != 0) {
                    perror("Error in mutex_unlock");
                    exitProgram(threadPool);
                }
                return;
            }
            //Check if the queue is empty. If so, block on the condition variable and go to step 2
            if (osIsQueueEmpty(threadPool->tasksQueue)) {
                if (pthread_cond_wait(threadPool->cond, threadPool->mutex) != 0) {
                    perror("Error in pthread_cond_wait");
                    exitProgram(threadPool);
                }
            } else {
                break;
            }
        }
        //Take the top job from the queue
        t = (Task *) osDequeue(threadPool->tasksQueue);
        //Release the mutex
        if (pthread_mutex_unlock(threadPool->mutex) != 0) {
            perror("Error in mutex_unlock");
            exitProgram(threadPool);
        }
        //Do the job
        if (t != NULL) {
            (t->computeFunc)(t->param);
            free(t);
        }
        //Go to step 1
    }
}

ThreadPool *tpCreate(int numOfThreads) {
    int i;
    // create and allocate the mutex and all its members
    ThreadPool *threadPool = (ThreadPool *) malloc(sizeof(ThreadPool));
    threadPool->cond = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    threadPool->available = (int *) malloc(sizeof(int));
    threadPool->mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    threadPool->threads = (pthread_t *) malloc(sizeof(pthread_t) * numOfThreads);
    threadPool->tasksQueue = osCreateQueue();
    threadPool->destroyed = 0;
    threadPool->numberOfThreads = numOfThreads;
    // available modes:
    // 0 - cant enter new tasks and cant execute tasks from queue,
    // 1 - cannot enter new tasks but can execute tasks from queue,
    // 2 - can execute and can enter new tasks
    *(threadPool->available) = 2;
    if(pthread_cond_init(threadPool->cond, NULL) != 0){
        perror("Error in cond_init");
        exitProgram(threadPool);
    }
    if(pthread_mutex_init(threadPool->mutex, NULL) != 0){
        perror("Error in mutex_init");
        exitProgram(threadPool);
    }
    // check if allocate succeeded
    if (checkThreadPool(threadPool) != 0) {
        perror("Error in allocate thread pool");
        exitProgram(threadPool);
    }
    // create threads and make them run the "doTasks" function
    for (i = 0; i < numOfThreads; i++) {
        if (pthread_create(&(threadPool->threads[i]), NULL, (void * (*)(void *))doTasks, (void *) threadPool) != 0) {
            perror("Error in system call");
            exitProgram(threadPool);
        }
    }
    return threadPool;
}

//this function validate all the threadPool memory allocation
int checkThreadPool(ThreadPool *threadPool) {
    if (threadPool == NULL) return 1;
    if (threadPool->mutex == NULL || threadPool->available == NULL || threadPool->tasksQueue == NULL ||
        threadPool->cond == NULL || threadPool->threads == NULL)
        return 1;
    return 0;
}


int tpInsertTask(ThreadPool *threadPool, void (*computeFunc)(void *), void *param) {
    // check thread pool and function validation
    if (checkThreadPool(threadPool) != 0 || computeFunc == NULL)
        return -1;
    if (*(threadPool->available) != 2)
        return -1;
    Task *t = (Task *) malloc(sizeof(Task));
    if (t == NULL) {
        perror("Error in allocate task");
        exitProgram(threadPool);
    }
    t->computeFunc = computeFunc;
    t->param = param;
    // Acquire the mutex
    if (pthread_mutex_lock(threadPool->mutex) != 0) {
        perror("Error in mutex_lock");
        exitProgram(threadPool);
    }
    //Add the job to the queue
    osEnqueue(threadPool->tasksQueue, t);
    //Signal the condition variable
    if (pthread_cond_signal(threadPool->cond) != 0) {
        perror("Error in system call");
        exitProgram(threadPool);
    }
    //Release the mutex
    if (pthread_mutex_unlock(threadPool->mutex) != 0) {
        perror("Error in mutex_unlock");
        exitProgram(threadPool);
    }

    return 0;
}

// free all allocated memory
void tpDestroy(ThreadPool *threadPool, int shouldWaitForTasks) {
    int i;
    // check validation
    if (checkThreadPool(threadPool) != 0)
        return;
    if (threadPool->destroyed)
        return;
    // mark thread pool ad destroyed
    threadPool->destroyed = 1;
    // Acquire the mutex
    if (pthread_mutex_lock(threadPool->mutex) != 0) {
        perror("Error in mutex_lock");
        exitProgram(threadPool);
    }
    // change the status of available according to 'shouldWaitForTasks'
    if (shouldWaitForTasks) // wait until q is empty
    {
        *(threadPool->available) = 1;
    } else {
        *(threadPool->available) = 0;
    }
    // Broadcast the condition variable
    if (pthread_cond_broadcast(threadPool->cond) != 0) {
        perror("Error in cond_broadcast");
        exitProgram(threadPool);
    }
    //Release the mutex
    if (pthread_mutex_unlock(threadPool->mutex) != 0) {
        perror("Error in mutex_unlock");
        exitProgram(threadPool);
    }
    // Join all threads
    for (i = 0; i < threadPool->numberOfThreads; i++) {
        if (pthread_join(threadPool->threads[i], NULL) != 0) {
            perror("Error in system call");
            exitProgram(threadPool);
        }
    }
    freeMemory(threadPool);
}

// this function gets thread pool and free any allocated memory its own
void freeMemory(ThreadPool *threadPool) {
    Task *t;
    if (threadPool == NULL) return;
    if (threadPool->mutex != NULL) {
        pthread_mutex_destroy(threadPool->mutex);
        free(threadPool->mutex);
    }
    if (threadPool->tasksQueue != NULL){
	while(!osIsQueueEmpty(threadPool->tasksQueue))
	{
            t = (Task *) osDequeue(threadPool->tasksQueue);
            free(t); 
	}
        osDestroyQueue(threadPool->tasksQueue);
	}
    if (threadPool->cond != NULL) {
        pthread_cond_destroy(threadPool->cond);
        free(threadPool->cond);
    }
    if (threadPool->threads != NULL)
        free(threadPool->threads);
    if (threadPool->available != NULL)
        free(threadPool->available);
    free(threadPool);
}

// free all allocated memory and exit the program
void exitProgram(ThreadPool *threadPool) {
    freeMemory(threadPool);
    exit(0);
}

