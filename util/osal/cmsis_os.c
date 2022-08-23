#include "cmsis_os.h"
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <synchapi.h>

/* -----------------------------------------------------------------------------
Data structure
----------------------------------------------------------------------------- */
typedef struct m_block
{
    struct m_block * next;
    uint32_t is_free                            : 1;
    uint32_t size;
} m_block_t;

typedef struct m_heap
{
    void * heap;
    m_block_t * list;
    uint32_t size;                              /* total size */
    uint32_t error_id                           : 3;
} m_heap_t;

static const osMutexAttr_t mutex_attr_kernel =
{
    "mutex_kernel", 
    osMutexRecursive | osMutexPrioInherit, 
    NULL,
    0U 
};

static const osMutexAttr_t mutex_attr_heap =
{
    "mutex_heap", 
    osMutexRecursive | osMutexPrioInherit, 
    NULL,
    0U 
};

static const osMutexAttr_t mutex_attr_queue =
{
    "mutex_queue", 
    osMutexRecursive | osMutexPrioInherit, 
    NULL,
    0U 
};

/* -----------------------------------------------------------------------------
Private functtion
----------------------------------------------------------------------------- */
static void m_heap_init(m_heap_t * const me, void *memory, uint32_t heap_size);
static void *m_heap_malloc(m_heap_t * const me, uint32_t size);
static void m_heap_free(m_heap_t * const me, void * data);

/* -----------------------------------------------------------------------------
OS Basic
----------------------------------------------------------------------------- */
osStatus_t osDelay(uint32_t ticks)
{
    struct timespec ts, ts1;

    ts.tv_nsec = (ticks % 1000) * 1000000;
    ts.tv_sec = ticks / 1000;

    nanosleep(&ts, &ts1);

    return osOK;
}

static bool mutex_kernel_lock_init = false;
static CRITICAL_SECTION kernel_lock;
int32_t osKernelLock(void)
{
    if (mutex_kernel_lock_init == false)
    {
        InitializeCriticalSection(&kernel_lock);
        EnterCriticalSection(&kernel_lock);

        mutex_kernel_lock_init = true;
    }
    else
    {
        EnterCriticalSection(&kernel_lock);
    }
    
    return 0;
}

int32_t osKernelUnlock(void)
{
    assert(mutex_kernel_lock_init);

    LeaveCriticalSection(&kernel_lock);

    return 0;
}

static uint64_t time_init = UINT64_MAX;
uint32_t osKernelGetTickCount(void)
{
    // Get the current time.
    struct timeval tt;
    gettimeofday(&tt, NULL);
    uint64_t time_current = (tt.tv_sec * 1000) + (tt.tv_usec / 1000);

    if (time_init == UINT64_MAX)
    {
        time_init = time_current;
    }

    return (uint32_t)(time_current - time_init);
}

uint32_t osKernelGetSysTimerCount(void)
{
    return osKernelGetTickCount();
}

void * platform_malloc(uint32_t size)
{
    return malloc(size);
}

void platform_free(void *data)
{
    free(data);
}

/* -----------------------------------------------------------------------------
Thread
----------------------------------------------------------------------------- */
typedef void *(*os_pthread_func_t) (void *argument);

osThreadId_t osThreadNew(osThreadFunc_t func, void *argument, const osThreadAttr_t *attr)
{
    (void)attr;

    pthread_t thread;
    int ret = pthread_create(&thread, NULL, (os_pthread_func_t)func, argument);
    assert(ret == 0);

    return (osThreadId_t)thread;
}

osThreadId_t osThreadGetId(void)
{
    return (osThreadId_t)pthread_self();
}

void osThreadExit(void)
{
    // NULL
}

/* -----------------------------------------------------------------------------
Message queue
----------------------------------------------------------------------------- */
typedef struct os_mq
{
    osMutexId_t mutex;
    osSemaphoreId_t sem_empty;
    osSemaphoreId_t sem_full;
    uint8_t *memory;
    uint32_t msg_size;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
    bool empty;
    bool full;
} os_mq_t;

osMessageQueueId_t osMessageQueueNew(uint32_t msg_count,
                                     uint32_t msg_size,
                                     const osMessageQueueAttr_t *attr)
{
    assert(attr == NULL);

    os_mq_t *mq = malloc(sizeof(os_mq_t));
    assert(mq != NULL);

    mq->mutex = osMutexNew(&mutex_attr_queue);
    assert(mq->mutex != NULL);
    mq->sem_full = osSemaphoreNew(msg_count, msg_count, NULL);
    assert(mq->sem_full != NULL);
    mq->sem_empty = osSemaphoreNew(msg_count, 0, NULL);
    assert(mq->sem_empty != NULL);

    mq->capacity = msg_count;
    mq->msg_size = msg_size;
    mq->memory = (uint8_t *)malloc(msg_size * msg_count);
    assert(mq->memory != NULL);

    mq->head = 0;
    mq->tail = 0;
    mq->empty = true;
    mq->full = false;

    return (osMessageQueueId_t)mq;
}

osStatus_t osMessageQueueDelete(osMessageQueueId_t mq_id)
{
    os_mq_t *mq = (os_mq_t *)mq_id;

    osMutexDelete(mq->mutex);
    free(mq->memory);
    free(mq);

    return osOK;
}

osStatus_t osMessageQueueReset(osMessageQueueId_t mq_id)
{
    os_mq_t *mq = (os_mq_t *)mq_id;

    osMutexAcquire(mq->mutex, osWaitForever);

    mq->head = 0;
    mq->tail = 0;
    mq->empty = true;
    mq->full = false;

    osMutexRelease(mq->mutex);

    return osOK;
}

osStatus_t osMessageQueuePut(osMessageQueueId_t mq_id,
                             const void *msg_ptr,
                             uint8_t msg_prio,
                             uint32_t timeout)
{
    (void)msg_prio;

    if (timeout != osWaitForever)
    {
        assert(timeout < (1000 * 60 * 60 * 24));
    }

    os_mq_t *mq = (os_mq_t *)mq_id;
    osSemaphoreAcquire(mq->sem_full, timeout);

    osMutexAcquire(mq->mutex, osWaitForever);
    assert(mq->full == false || mq->empty == false);
    memcpy(&mq->memory[mq->head * mq->msg_size], msg_ptr, mq->msg_size);
    mq->head = (mq->head + 1) % mq->capacity;
    mq->empty = false;
    if (mq->head == mq->tail)
    {
        mq->full = true;
    }
    osMutexRelease(mq->mutex);
    osSemaphoreRelease(mq->sem_empty);

    return osOK;
}

osStatus_t osMessageQueueGet(osMessageQueueId_t mq_id,
                             void *msg_ptr,
                             uint8_t *msg_prio,
                             uint32_t timeout)
{
    (void)msg_prio;

    if (timeout != osWaitForever)
    {
        assert(timeout < (1000 * 60 * 60 * 24));
    }

    os_mq_t *mq = (os_mq_t *)mq_id;
    osStatus_t ret = osSemaphoreAcquire(mq->sem_empty, timeout);
    if (ret == osErrorTimeout)
    {
        return osErrorTimeout;
    }

    osMutexAcquire(mq->mutex, osWaitForever);
    assert(mq->full == false || mq->empty == false);
    memcpy(msg_ptr, &mq->memory[mq->tail * mq->msg_size], mq->msg_size);
    mq->tail = (mq->tail + 1) % mq->capacity;
    mq->full = false;
    if (mq->head == mq->tail)
    {
        mq->empty = true;
    }
    osMutexRelease(mq->mutex);
    osSemaphoreRelease(mq->sem_full);

    return osOK;
}

/* -----------------------------------------------------------------------------
Mutex
----------------------------------------------------------------------------- */
typedef struct os_mutex_data
{
    osMutexAttr_t attr;
    pthread_mutex_t mutex;
} os_mutex_data_t;

osMutexId_t osMutexNew(const osMutexAttr_t *attr)
{
    (void)attr;

    os_mutex_data_t *data = malloc(sizeof(os_mutex_data_t));
    assert(data != NULL);

    int ret = pthread_mutex_init(&data->mutex, NULL);
    assert(ret == 0);

    memcpy(&data->attr, attr, sizeof(osMutexAttr_t));

    return (osMutexId_t)data;
}

osStatus_t osMutexDelete(osMutexId_t mutex_id)
{
    os_mutex_data_t *data = (os_mutex_data_t *)mutex_id;

    int ret = pthread_mutex_destroy(&data->mutex);
    assert(ret == 0);

    free(data);

    return osOK;
}

osStatus_t osMutexAcquire(osMutexId_t mutex_id, uint32_t timeout)
{
    assert(timeout == osWaitForever);
    assert(mutex_id != NULL);

    os_mutex_data_t *data = (os_mutex_data_t *)mutex_id;

    int ret = pthread_mutex_lock(&data->mutex);
    assert(ret == 0);

    return osOK;
}

osStatus_t osMutexRelease(osMutexId_t mutex_id)
{
    os_mutex_data_t *data = (os_mutex_data_t *)mutex_id;

    int ret = pthread_mutex_unlock(&data->mutex);
    if (ret != 0)
    {
        return osError;
    }
    else
    {
        return osOK;
    }
}

/* -----------------------------------------------------------------------------
Semaphore
----------------------------------------------------------------------------- */
#include <semaphore.h>

osSemaphoreId_t osSemaphoreNew(uint32_t max_count, uint32_t initial_count, const osSemaphoreAttr_t *attr)
{
    assert(initial_count <= max_count);

    sem_t *sem = malloc(sizeof(sem_t));
    assert(sem != NULL);

    int ret = sem_init(sem, 0, max_count);
    assert(ret == 0);

    for (uint32_t i = initial_count; i < max_count; i ++)
    {
        ret = sem_wait(sem);
        assert(ret == 0);
    }

    return (osSemaphoreId_t)sem;
}

osStatus_t osSemaphoreDelete(osSemaphoreId_t semaphore_id)
{
    sem_destroy((sem_t *)semaphore_id);
    free((sem_t *)semaphore_id);

    return osOK;
}

osStatus_t osSemaphoreRelease(osSemaphoreId_t semaphore_id)
{
    int ret = sem_post((sem_t *)semaphore_id);
    assert(ret == 0);

    return osOK;
}

osStatus_t osSemaphoreAcquire(osSemaphoreId_t semaphore_id, uint32_t timeout)
{
    if (timeout != osWaitForever)
    {
        assert(timeout < (1000 * 60 * 60 * 24));
    }

    int ret;
    if (timeout == osWaitForever)
    {
        ret = sem_wait((sem_t *)semaphore_id);
        assert(ret == 0);
    }
    else
    {
        struct timeval tt;
        gettimeofday(&tt, NULL);

        uint32_t second = timeout / 1000;
        uint32_t ns = (timeout % 1000) * 1000 * 1000;

        struct timespec ts;
        ts.tv_sec = tt.tv_sec + second;
        ts.tv_nsec = tt.tv_usec * 1000 + ns;
        ts.tv_sec += (ts.tv_nsec / (1000 * 1000 * 1000));
        ts.tv_nsec %= (1000 * 1000 * 1000);

        ret = sem_timedwait((sem_t *)semaphore_id, &ts);
        if (ret == -1)
        {
            if (errno == ETIMEDOUT)
            {
                return osErrorTimeout;
            }
        }
    }

    return osOK;
}

bool platform_runing = true;
void os_assert_handler(const char *ex_string, const char *func, uint32_t line)
{
    printf("assert: %s, %s, %u.\n", ex_string, func, line);
    fflush(stdout);

    platform_runing = false;
}

/* -----------------------------------------------------------------------------
Heap
----------------------------------------------------------------------------- */
/**
  * @brief  heap initialization.
  * @param  me          : The heap object.
  * @param  memory      : The heap memory pointer.
  * @param  heap_size   : The heap memory size to be written.
  */
static void m_heap_init(m_heap_t * const me, void *memory, uint32_t heap_size)
{
    m_block_t *block_1st;
    
    // Make sure that the heap memory is 4-byte aligned.
    uint32_t mod = (((uint32_t)memory) % 4);
    if (mod != 0)
    {
        memory = (void *)((uint32_t)memory + 4 - mod);
    }
    me->heap = memory;
    me->size = heap_size;
    if (mod != 0)
    {
        me->size -= (4 - mod);
    }

    // block start
    me->list = (m_block_t *)memory;
    // the 1st free block
    block_1st = (m_block_t *)memory;
    block_1st->next = NULL;
    block_1st->size = me->size - (uint32_t)sizeof(m_block_t);
    block_1st->is_free = 1;

    me->error_id = 0;
}

/**
  * @brief  Heap malloc funtion.
  * @param  me          : The heap object.
  * @param  size        : Size of the memory applied.
  * @retval             : if = NULL, malloc fail. if != NULL, malloc success.
  */
static void *m_heap_malloc(m_heap_t * const me, uint32_t size)
{
    if (size == 0)
    {
        me->error_id = 1;
        return NULL;
    }

    // Make sure that the applied memory is 4-byte aligned.
    if ((size % 4) != 0)
    {
        size = (size + 4 - (size % 4));
    }
    
    /* Find the first free block in the block-list. */
    m_block_t *block = (m_block_t *)me->list;
    do
    {
        if (block->is_free == 1 && block->size > (size + sizeof(m_block_t)))
        {
            break;
        }
        else
        {
            block = block->next;
        }
    } while (block != NULL);

    if (block == NULL)
    {
        me->error_id = 2;
        return NULL;
    }

    /* If the block's byte number is NOT enough to create a new block. */
    if (block->size <= (uint32_t)((uint32_t)size + sizeof(m_block_t)))
    {
        block->is_free = 0;
    }
    /* Divide the block into two blocks. */
    else
    {
        m_block_t *new_block =
            (m_block_t *)((uint32_t)block + size + sizeof(m_block_t));
        new_block->size = block->size - size - sizeof(m_block_t);
        new_block->is_free = 1;
        new_block->next = NULL;

        /* Update the list. */
        new_block->next = block->next;
        block->next = new_block;
        block->size = size;
        block->is_free = 0;
    }

    me->error_id = 0;

    return (void *)((uint32_t)block + (uint32_t)sizeof(m_block_t));
}

/**
  * @brief  Heap free funtion.
  * @param  me          : The heap object.
  * @param  data        : The memory pointer to free
  */
static void m_heap_free(m_heap_t * const me, void * data)
{
    m_block_t *block_crt = (m_block_t *)((uint32_t)data - sizeof(m_block_t));

    /* Search for this block in the block-list. */
    m_block_t *block = me->list;
    m_block_t *block_last = NULL;
    do
    {
        if (block->is_free == 0 && block == block_crt)
        {
            break;
        }
        else
        {
            block_last = block;
            block = block->next;
        }
    } while (block != NULL);

    /* Not found. */
    if (block == NULL)
    {
        me->error_id = 4;
        return;
    }

    block->is_free = 1;
    /* Check the block can be combined with the front one. */
    if (block_last != NULL && block_last->is_free == 1)
    {
        block_last->size += (block->size + sizeof(m_block_t));
        block_last->next = block->next;
        block = block_last;
    }
    /* Check the block can be combined with the later one. */
    if (block->next != NULL && block->next->is_free == 1)
    {
        block->size += (block->next->size + (uint32_t)sizeof(m_block_t));
        block->next = block->next->next;
        block->is_free = 1;
    }

    me->error_id = 0;
}
