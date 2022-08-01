/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
EventOS Kernel source files ek.c and ek.h are based on RT-Thread kernel source
files. 

All copyrights in ek.c and ek.h belongs to RT-Thread Development Team. Sincere
thanks to the RT-Thread Team.
*/

#ifndef EOS_KERNEL_H
#define EOS_KERNEL_H

#include "eos_def.h"

#define EOS_ASSERT(EX)

/**
 * @ingroup BasicDef
 *
 * @def EOS_ALIGN_DOWN(size, align)
 * Return the down number of aligned at specified width. EOS_ALIGN_DOWN(13, 4)
 * would return 12.
 */
#define EOS_ALIGN_DOWN(size, align)     ((size) & ~((align) - 1))

/**
 * Double List structure
 */
struct eos_list_node
{
    struct eos_list_node *next;             /**< point to next node. */
    struct eos_list_node *prev;             /**< point to prev node. */
};
typedef struct eos_list_node eos_list_t;    /**< Type for lists. */

/**
 * Base structure of Kernel object
 */
typedef struct eos_obj
{
    eos_u8_t type;                          /**< type of kernel object */
    eos_u8_t flag;                          /**< flag of kernel object */
    eos_list_t list;                        /**< list node of kernel object */
} eos_obj_t;
typedef eos_obj_t *eos_object_t;    /**< Type for kernel objects. */

/**
 *  The object type can be one of the follows with specific
 *  macros enabled:
 *  - Thread
 *  - Semaphore
 *  - Mutex
 *  - Timer
 *  - Unknown
 *  - Static
 */
enum eos_obj_type
{
    EOS_Object_Null          = 0x00,        /**< The object is not used. */
    EOS_Object_Thread        = 0x01,        /**< The object is a task. */
    EOS_Object_Semaphore     = 0x02,        /**< The object is a semaphore. */
    EOS_Object_Mutex         = 0x03,        /**< The object is a mutex. */
    EOS_Object_Event         = 0x04,        /**< The object is a event. */
    EOS_Object_Timer         = 0x0a,        /**< The object is a timer. */
    EOS_Object_Unknown       = 0x0e,        /**< The object is unknown. */
    EOS_Object_Static        = 0x80         /**< The object is a static object. */
};

/**
 * The information of the kernel object
 */
struct eos_obj_info
{
    enum eos_obj_type type;                 /**< object class type */
    eos_list_t object_list;                 /**< object list */
    eos_size_t object_size;                 /**< object size */
};

/**
 * clock & timer macros
 */
#define EOS_TIMER_FLAG_DEACTIVATED       0x0             /**< timer is deactive */
#define EOS_TIMER_FLAG_ACTIVATED         0x1             /**< timer is active */
#define EOS_TIMER_FLAG_ONE_SHOT          0x0             /**< one shot timer */
#define EOS_TIMER_FLAG_PERIODIC          0x2             /**< periodic timer */

#define EOS_TIMER_FLAG_HARD_TIMER        0x0             /**< hard timer,the timer's callback function will be called in tick isr. */
#define EOS_TIMER_FLAG_SOFT_TIMER        0x4             /**< soft timer,the timer's callback function will be called in timer task. */

#define EOS_TIMER_CTRL_SET_TIME          0x0             /**< set timer control command */
#define EOS_TIMER_CTRL_GET_TIME          0x1             /**< get timer control command */
#define EOS_TIMER_CTRL_SET_ONESHOT       0x2             /**< change timer to one shot */
#define EOS_TIMER_CTRL_SET_PERIODIC      0x3             /**< change timer to periodic */
#define EOS_TIMER_CTRL_GET_STATE         0x4             /**< get timer run state active or deactive*/
#define EOS_TIMER_CTRL_GET_REMAIN_TIME   0x5             /**< get the remaining hang time */

#ifndef EOS_TIMER_SKIP_LIST_LEVEL
#define EOS_TIMER_SKIP_LIST_LEVEL        1
#endif

/* 1 or 3 */
#ifndef EOS_TIMER_SKIP_LIST_MASK
#define EOS_TIMER_SKIP_LIST_MASK         0x3
#endif

/**
 * timer structure
 */
typedef struct eos_timer
{
    eos_obj_t super;                            /**< inherit from eos_object */

    eos_list_t row[EOS_TIMER_SKIP_LIST_LEVEL];

    void (*timeout_func)(void *parameter);              /**< timeout function */
    void *parameter;                         /**< timeout function's parameter */

    eos_u32_t init_tick;                         /**< timer timeout tick */
    eos_u32_t timeout_tick;                      /**< timeout tick */
} eos_timer_t;
typedef struct eos_timer *eos_timer_handle_t;

/*
 * Thread
 */

/**
 * task control command definitions
 */
#define EOS_TASK_CTRL_STARTUP          0x00                /**< Startup task. */
#define EOS_TASK_CTRL_CLOSE            0x01                /**< Close task. */
#define EOS_TASK_CTRL_CHANGE_PRIORITY  0x02                /**< Change task priority. */
#define EOS_TASK_CTRL_INFO             0x03                /**< Get task information. */
#define EOS_TASK_CTRL_BIND_CPU         0x04                /**< Set task bind cpu. */

/**
 * Thread structure
 */
typedef struct eos_task
{
    /* eos object */
    eos_u8_t type;                                   /**< type of object */
    eos_u8_t flags;                                  /**< task's flags */

    eos_list_t list;                                   /**< the object list */
    eos_list_t tlist;                                  /**< the task list */

    /* stack point and entry */
    void *sp;                                     /**< stack point */
    void *entry;                                  /**< entry */
    void *parameter;                              /**< parameter */
    void *stack_addr;                             /**< stack address */
    eos_u32_t stack_size;                             /**< stack size */

    /* error code */
    eos_err_t error;                                  /**< error code */

    eos_u8_t status;                                   /**< task status */

    /* priority */
    eos_u8_t  current_priority;                       /**< current priority */
    eos_u32_t number_mask;

    eos_ubase_t init_tick;                              /**< task's initialized tick */
    eos_ubase_t remaining_tick;                         /**< remaining tick */

#ifdef EOS_USING_CPU_USAGE
    eos_u64_t  duration_tick;                          /**< cpu usage tick */
#endif

    struct eos_timer task_timer;                       /**< built-in task timer */

    void (*cleanup)(struct eos_task *tid);             /**< cleanup function when task exit */

    /* light weight process if present */
    eos_ubase_t user_data;                             /**< private user data beyond this task */
} eos_task_t;

typedef struct eos_task *eos_task_handle_t;

/**
 * IPC flags and control command definitions
 */
#define EOS_IPC_FLAG_FIFO                0x00            /**< FIFOed IPC. @ref IPC. */
#define EOS_IPC_FLAG_PRIO                0x01            /**< PRIOed IPC. @ref IPC. */

#define EOS_WAITING_FOREVER              -1              /**< Block forever until get resource. */
#define EOS_WAITING_NO                   0               /**< Non-block. */

/**
 * Base structure of IPC object
 */
struct eos_ipc_object
{
    eos_obj_t super;                            /**< inherit from eos_object */

    eos_list_t suspend_task;                    /**< tasks pended on this resource */
};

#ifdef EOS_USING_SEMAPHORE
/**
 * Semaphore structure
 */
struct eos_semaphore
{
    struct eos_ipc_object super;                        /**< inherit from ipc_object */

    eos_u16_t value;                         /**< value of semaphore. */
};
typedef struct eos_semaphore *eos_sem_t;
#endif

#ifdef EOS_USING_MUTEX
/**
 * Mutual exclusion (mutex) structure
 */
struct eos_mutex
{
    struct eos_ipc_object super;                        /**< inherit from ipc_object */

    eos_u16_t value;                         /**< value of mutex */

    eos_u8_t prio_bkp;             /**< priority of last task hold the mutex */
    eos_u8_t hold;                          /**< numbers of task hold the mutex */

    struct eos_task *owner;                         /**< current owner of mutex */
};
typedef struct eos_mutex *eos_mutex_t;
#endif

/**@}*/

eos_base_t eos_hw_interrupt_disable(void);
void eos_hw_interrupt_enable(eos_base_t level);


eos_u32_t eos_tick_get(void);
void eos_tick_set(eos_u32_t tick);
void eos_tick_increase(void);
eos_u32_t eos_tick_from_millisecond(eos_s32_t ms);
eos_u32_t eos_tick_get_millisecond(void);

void eos_system_timer_init(void);
void eos_system_timer_task_init(void);

void eos_timer_init(eos_timer_handle_t  timer,
                   const char *name,
                   void (*timeout)(void *parameter),
                   void *parameter,
                   eos_u32_t time,
                   eos_u8_t flag);
eos_err_t eos_timer_detach(eos_timer_handle_t timer);
eos_err_t eos_timer_start(eos_timer_handle_t timer);
eos_err_t eos_timer_stop(eos_timer_handle_t timer);
eos_err_t eos_timer_control(eos_timer_handle_t timer, int cmd, void *arg);

eos_u32_t eos_timer_next_timeout_tick(void);
void eos_timer_check(void);

/* API for task ------------------------------------------------------------- */
/*
 * task interface
 */
eos_err_t eos_task_init(struct eos_task *task,
                        const char *name,
                        void (*entry)(void *parameter),
                        void *parameter,
                        void *stack_start,
                        eos_u32_t stack_size,
                        eos_u8_t priority,
                        eos_u32_t tick);
eos_err_t eos_task_detach(eos_task_handle_t task);
eos_task_handle_t eos_task_self(void);
eos_task_handle_t eos_task_find(char *name);
eos_err_t eos_task_startup(eos_task_handle_t task);
eos_err_t eos_task_yield(void);
eos_err_t eos_task_delay(eos_u32_t tick);
eos_err_t eos_task_delay_until(eos_u32_t *tick, eos_u32_t inc_tick);
eos_err_t eos_task_mdelay(eos_s32_t ms);
eos_err_t eos_task_control(eos_task_handle_t task, int cmd, void *arg);
eos_err_t eos_task_suspend(eos_task_handle_t task);
eos_err_t eos_task_resume(eos_task_handle_t task);

/*
 * idle task interface
 */
void eos_task_idle_init(void);
/*
 * schedule service
 */
void eos_kernel_init(void);
void eos_kernel_start(void);

void eos_enter_critical(void);
void eos_exit_critical(void);
eos_u16_t eos_critical_level(void);

/* API for semaphore -------------------------------------------------------- */
#ifdef EOS_USING_SEMAPHORE
/*
 * semaphore interface
 */
eos_err_t eos_sem_init(eos_sem_t    sem,
                       const char *name,
                       eos_u32_t value,
                       eos_u8_t flag);
eos_err_t eos_sem_detach(eos_sem_t sem);

eos_err_t eos_sem_take(eos_sem_t sem, eos_s32_t time);
eos_err_t eos_sem_trytake(eos_sem_t sem);
eos_err_t eos_sem_release(eos_sem_t sem);
eos_err_t eos_sem_reset(eos_sem_t sem, eos_ubase_t value);
#endif

#ifdef EOS_USING_MUTEX
/*
 * mutex interface
 */
eos_err_t eos_mutex_init(eos_mutex_t mutex, const char *name, eos_u8_t flag);
eos_err_t eos_mutex_detach(eos_mutex_t mutex);

eos_err_t eos_mutex_take(eos_mutex_t mutex, eos_s32_t time);
eos_err_t eos_mutex_trytake(eos_mutex_t mutex);
eos_err_t eos_mutex_release(eos_mutex_t mutex);
#endif

/* defunct */
void eos_task_defunct_enqueue(eos_task_handle_t task);
eos_task_handle_t eos_task_defunct_dequeue(void);

/*
 * interrupt service
 */

/*
 * eos_interrupt_enter and eos_interrupt_leave only can be called by BSP
 */
void eos_interrupt_enter(void);
void eos_interrupt_leave(void);

/*
 * the number of nested interrupts.
 */
eos_u8_t eos_interrupt_get_nest(void);

eos_err_t eos_get_errno(void);
void eos_set_errno(eos_err_t no);
int *_eos_errno(void);

#endif
