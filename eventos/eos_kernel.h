#ifndef EOS_KERNEL_H
#define EOS_KERNEL_H

/* include ------------------------------------------------------------------ */
#include "eventos_config.h"
#include "eos_def.h"

/* Basic data structure ----------------------------------------------------- */
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
 * Base structure of Kernel object
 */
typedef struct eos_obj
{
    eos_u8_t type;                          /**< type of kernel object */
    eos_u8_t flag;                          /**< flag of kernel object */
    eos_list_t list;                        /**< list node of kernel object */
} eos_obj_t;
typedef eos_obj_t *eos_obj_handle_t;    /**< Type for kernel objects. */

/*
 * Defines the prototype to which the application task hook function must
 * conform.
 */
typedef void (* eos_func_t)(void *parameter);

/*
 * Definition of the event class.
 */
typedef struct eos_event
{
    const char *topic;                      // The event topic.
    eos_u32_t eid                    : 16;   // The event ID.
    eos_u32_t size                   : 16;   // The event content's size.
} eos_event_t;

/* Timer -------------------------------------------------------------------- */
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
    eos_obj_t super;

    eos_list_t row[EOS_TIMER_SKIP_LIST_LEVEL];

    void (*timeout_func)(void *parameter);
    void *parameter;

    eos_u32_t init_tick;
    eos_u32_t timeout_tick;
} eos_timer_t;
typedef struct eos_timer *eos_timer_handle_t;

/* Task --------------------------------------------------------------------- */

/**
 * Thread structure
 */
typedef struct ek_task
{
    /* eos object */
    eos_u8_t type;                              /**< type of object */
    eos_u8_t flags;                             /**< task's flags */

    eos_list_t list;                            /**< the object list */
    eos_list_t tlist;                           /**< the task list */

    /* stack point and entry */
    void *sp;                                   /**< stack point */
    void *entry;                                /**< entry */
    void *parameter;                            /**< parameter */
    void *stack_addr;                           /**< stack address */
    eos_u32_t stack_size;                       /**< stack size */

    /* error code */
    eos_err_t error;                            /**< error code */

    eos_u8_t status;                            /**< task status */

    /* priority */
    eos_u8_t  current_priority;                 /**< current priority */
    eos_u32_t number_mask;

    eos_ubase_t init_tick;                      /**< task's initialized tick */
    eos_ubase_t remaining_tick;                 /**< remaining tick */

#ifdef EOS_USING_CPU_USAGE
    eos_u64_t duration_tick;                    /**< cpu usage tick */
#endif

    eos_timer_t task_timer;                     /**< built-in task timer */
    
    void (*cleanup)(struct ek_task *tid);      /**< cleanup function when task exit */

    /* light weight process if present */
    void *user_data;                            /**< private user data beyond this task */
} ek_task_t;

typedef struct ek_task *ek_task_handle_t;

eos_err_t ek_task_init(ek_task_handle_t task,
                        const char *name,
                        void (*entry)(void *parameter),
                        void *parameter,
                        void *stack_start,
                        eos_u32_t stack_size,
                        eos_u8_t priority,
                        eos_u32_t tick);

/* mutex -------------------------------------------------------------------- */
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
struct ek_ipc_object
{
    eos_obj_t super;                            /**< inherit from eos_object */

    eos_list_t suspend_task;                    /**< tasks pended on this resource */
};

#ifdef EOS_USING_MUTEX
/**
 * Mutual exclusion (mutex) structure
 */
typedef struct ek_mutex
{
    struct ek_ipc_object super;                        /**< inherit from ipc_object */

    eos_u16_t value;                         /**< value of mutex */

    eos_u8_t prio_bkp;             /**< priority of last task hold the mutex */
    eos_u8_t hold;                          /**< numbers of task hold the mutex */

    struct ek_task *owner;                         /**< current owner of mutex */
} ek_mutex_t;

typedef struct ek_mutex *ek_mutex_handle_t;
#endif

/* Semaphore ---------------------------------------------------------------- */
#ifdef EOS_USING_SEMAPHORE
/**
 * Semaphore structure
 */
typedef struct ek_semaphore
{
    struct ek_ipc_object super;                /**< inherit from ipc_object */

    eos_u16_t value;                            /**< value of semaphore. */
} ek_sem_t;

typedef struct ek_semaphore *ek_sem_handle_t;
#endif

#endif
