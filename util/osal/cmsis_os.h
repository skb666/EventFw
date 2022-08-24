#ifndef __CMSIS_OS_H
#define __CMSIS_OS_H

#include <stdint.h>
#include <stdbool.h>

/* -----------------------------------------------------------------------------
OS Define
----------------------------------------------------------------------------- */
/// Status code values returned by CMSIS-RTOS functions.
typedef enum
{
   osOK                    =  0,         ///< Operation completed successfully.
   osError                 = -1,         ///< Unspecified RTOS error: run-time error but no other error message fits.
   osErrorTimeout          = -2,         ///< Operation not completed within the timeout period.
   osErrorResource         = -3,         ///< Resource not available.
   osErrorParameter        = -4,         ///< Parameter error.
   osErrorNoMemory         = -5,         ///< System is out of memory: it was impossible to allocate or reserve memory for the operation.
   osErrorISR              = -6,         ///< Not allowed in ISR context: the function cannot be called from interrupt service routines.
   osStatusReserved        = 0x7FFFFFFF  ///< Prevents enum down-size compiler optimization.
} osStatus_t;

/// Priority values.
typedef enum
{
    osPriorityNone              =  0,     ///< No priority (not initialized).
    osPriorityIdle              =  1,     ///< Reserved for Idle thread.
    osPriorityLow               =  8,     ///< Priority: low
    osPriorityLow1              =  8+1,   ///< Priority: low + 1
    osPriorityLow2              =  8+2,   ///< Priority: low + 2
    osPriorityLow3              =  8+3,   ///< Priority: low + 3
    osPriorityLow4              =  8+4,   ///< Priority: low + 4
    osPriorityLow5              =  8+5,   ///< Priority: low + 5
    osPriorityLow6              =  8+6,   ///< Priority: low + 6
    osPriorityLow7              =  8+7,   ///< Priority: low + 7
    osPriorityBelowNormal       = 16,     ///< Priority: below normal
    osPriorityBelowNormal1      = 16+1,   ///< Priority: below normal + 1
    osPriorityBelowNormal2      = 16+2,   ///< Priority: below normal + 2
    osPriorityBelowNormal3      = 16+3,   ///< Priority: below normal + 3
    osPriorityBelowNormal4      = 16+4,   ///< Priority: below normal + 4
    osPriorityBelowNormal5      = 16+5,   ///< Priority: below normal + 5
    osPriorityBelowNormal6      = 16+6,   ///< Priority: below normal + 6
    osPriorityBelowNormal7      = 16+7,   ///< Priority: below normal + 7
    osPriorityNormal            = 24,     ///< Priority: normal
    osPriorityNormal1           = 24+1,   ///< Priority: normal + 1
    osPriorityNormal2           = 24+2,   ///< Priority: normal + 2
    osPriorityNormal3           = 24+3,   ///< Priority: normal + 3
    osPriorityNormal4           = 24+4,   ///< Priority: normal + 4
    osPriorityNormal5           = 24+5,   ///< Priority: normal + 5
    osPriorityNormal6           = 24+6,   ///< Priority: normal + 6
    osPriorityNormal7           = 24+7,   ///< Priority: normal + 7
    osPriorityAboveNormal       = 32,     ///< Priority: above normal
    osPriorityAboveNormal1      = 32+1,   ///< Priority: above normal + 1
    osPriorityAboveNormal2      = 32+2,   ///< Priority: above normal + 2
    osPriorityAboveNormal3      = 32+3,   ///< Priority: above normal + 3
    osPriorityAboveNormal4      = 32+4,   ///< Priority: above normal + 4
    osPriorityAboveNormal5      = 32+5,   ///< Priority: above normal + 5
    osPriorityAboveNormal6      = 32+6,   ///< Priority: above normal + 6
    osPriorityAboveNormal7      = 32+7,   ///< Priority: above normal + 7
    osPriorityHigh              = 40,     ///< Priority: high
    osPriorityHigh1             = 40+1,   ///< Priority: high + 1
    osPriorityHigh2             = 40+2,   ///< Priority: high + 2
    osPriorityHigh3             = 40+3,   ///< Priority: high + 3
    osPriorityHigh4             = 40+4,   ///< Priority: high + 4
    osPriorityHigh5             = 40+5,   ///< Priority: high + 5
    osPriorityHigh6             = 40+6,   ///< Priority: high + 6
    osPriorityHigh7             = 40+7,   ///< Priority: high + 7
    osPriorityRealtime          = 48,     ///< Priority: realtime
    osPriorityRealtime1         = 48+1,   ///< Priority: realtime + 1
    osPriorityRealtime2         = 48+2,   ///< Priority: realtime + 2
    osPriorityRealtime3         = 48+3,   ///< Priority: realtime + 3
    osPriorityRealtime4         = 48+4,   ///< Priority: realtime + 4
    osPriorityRealtime5         = 48+5,   ///< Priority: realtime + 5
    osPriorityRealtime6         = 48+6,   ///< Priority: realtime + 6
    osPriorityRealtime7         = 48+7,   ///< Priority: realtime + 7
    osPriorityISR               = 56,     ///< Reserved for ISR deferred thread.
    osPriorityError             = -1,     ///< System cannot determine priority or illegal priority.
    osPriorityReserved          = 0x7FFFFF///< Prevents enum down-size compiler optimization.
} osPriority_t;

#define portMAX_DELAY           (uint32_t)0xffffffffUL
#define osWaitForever           0xFFFFFFFFU ///< Wait forever timeout value.

/// Entry point of a thread.
typedef void (*osThreadFunc_t) (void *argument);

/// \details Data type that identifies secure software modules called by a process.
typedef uint32_t TZ_ModuleId_t;

/* -----------------------------------------------------------------------------
OS Basic
----------------------------------------------------------------------------- */
// TODO 此处使用mutex实现，并不能真正关闭调度。
int32_t osKernelLock(void);
int32_t osKernelUnlock(void);
osStatus_t osDelay(uint32_t ticks);
uint32_t osKernelGetTickCount(void);
uint32_t osKernelGetSysTimerCount(void);

#define osKernelSysTick                 osKernelGetSysTimerCount

/* -----------------------------------------------------------------------------
Thread
----------------------------------------------------------------------------- */
/// \details Thread ID identifies the thread.
typedef void *osThreadId_t;

// Thread attributes (attr_bits in \ref osThreadAttr_t).
#define osThreadDetached      0x00000000U ///< Thread created in detached mode (default)
#define osThreadJoinable      0x00000001U ///< Thread created in joinable mode

/// Attributes structure for thread.
typedef struct
{
    const char *name;                   ///< name of the thread
    uint32_t attr_bits;                 ///< attribute bits
    void *cb_mem;                       ///< memory for control block
    uint32_t cb_size;                   ///< size of provided memory for control block
    void *stack_mem;                    ///< memory for stack
    uint32_t stack_size;                ///< size of stack
    osPriority_t priority;              ///< initial thread priority (default: osPriorityNormal)
    TZ_ModuleId_t tz_module;            ///< TrustZone module identifier
    uint32_t reserved;                  ///< reserved (must be 0)
} osThreadAttr_t;

osThreadId_t osThreadNew(osThreadFunc_t func, void *argument, const osThreadAttr_t *attr);
osThreadId_t osThreadGetId(void);
osStatus_t osThreadTerminate(osThreadId_t thread_id);
void osThreadExit(void);

/* -----------------------------------------------------------------------------
Message queue
----------------------------------------------------------------------------- */
/// Attributes structure for message queue.
typedef struct
{
    const char *name;                   ///< name of the message queue
    uint32_t attr_bits;                 ///< attribute bits
    void *cb_mem;                       ///< memory for control block
    uint32_t cb_size;                   ///< size of provided memory for control block
    void *mq_mem;                       ///< memory for data storage
    uint32_t mq_size;                   ///< size of provided memory for data storage
} osMessageQueueAttr_t;

/// \details Message Queue ID identifies the message queue.
typedef void *osMessageQueueId_t;

osMessageQueueId_t osMessageQueueNew(uint32_t msg_count,
                                     uint32_t msg_size,
                                     const osMessageQueueAttr_t *attr);
osStatus_t osMessageQueueDelete(osMessageQueueId_t mq_id);
osStatus_t osMessageQueueReset(osMessageQueueId_t mq_id);
osStatus_t osMessageQueuePut(osMessageQueueId_t mq_id, const void *msg_ptr, uint8_t msg_prio, uint32_t timeout);
osStatus_t osMessageQueueGet(osMessageQueueId_t mq_id, void *msg_ptr, uint8_t *msg_prio, uint32_t timeout);

/* -----------------------------------------------------------------------------
Mutex
----------------------------------------------------------------------------- */
/// \details Mutex ID identifies the mutex.
typedef void *osMutexId_t;

// Mutex attributes (attr_bits in \ref osMutexAttr_t).
#define osMutexRecursive      0x00000001U ///< Recursive mutex.
#define osMutexPrioInherit    0x00000002U ///< Priority inherit protocol.
#define osMutexRobust         0x00000008U ///< Robust mutex.

/// Attributes structure for mutex.
typedef struct
{
    const char *name;                   ///< name of the mutex
    uint32_t attr_bits;                 ///< attribute bits
    void *cb_mem;                       ///< memory for control block
    uint32_t cb_size;                   ///< size of provided memory for control block
} osMutexAttr_t;

osMutexId_t osMutexNew(const osMutexAttr_t *attr);
osStatus_t osMutexDelete(osMutexId_t mutex_id);
osStatus_t osMutexAcquire(osMutexId_t mutex_id, uint32_t timeout);
osStatus_t osMutexRelease(osMutexId_t mutex_id);

/* -----------------------------------------------------------------------------
Semaphore
----------------------------------------------------------------------------- */
/// \details Semaphore ID identifies the semaphore.
typedef void *osSemaphoreId_t;

/// Attributes structure for semaphore.
typedef struct
{
    const char *name;                   ///< name of the semaphore
    uint32_t attr_bits;                 ///< attribute bits
    void *cb_mem;                       ///< memory for control block
    uint32_t cb_size;                   ///< size of provided memory for control block
} osSemaphoreAttr_t;

osSemaphoreId_t osSemaphoreNew(uint32_t max_count, uint32_t initial_count, const osSemaphoreAttr_t *attr);
osStatus_t osSemaphoreDelete(osSemaphoreId_t semaphore_id);
osStatus_t osSemaphoreRelease(osSemaphoreId_t semaphore_id);
osStatus_t osSemaphoreAcquire(osSemaphoreId_t semaphore_id, uint32_t timeout);

/* -----------------------------------------------------------------------------
Assert
----------------------------------------------------------------------------- */
extern bool platform_runing;
void os_assert_handler(const char *ex_string, const char *func, uint32_t line);

#endif
