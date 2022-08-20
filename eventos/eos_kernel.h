#ifndef EOS_KERNEL_H
#define EOS_KERNEL_H

/* include ------------------------------------------------------------------ */
#include "eos_config.h"
#include "eos_def.h"

/* Basic data structure ----------------------------------------------------- */
/**
 * Double List structure
 */
struct ek_list_node
{
    struct ek_list_node *next;             /**< point to next node. */
    struct ek_list_node *prev;             /**< point to prev node. */
};
typedef struct ek_list_node ek_list_t;    /**< Type for lists. */

/**
 * The information of the kernel object
 */
struct ek_obj_info
{
    eos_u8_t type;                          /**< object class type */
    ek_list_t object_list;                 /**< object list */
    eos_size_t object_size;                 /**< object size */
};

/**
 * Base structure of Kernel object
 */
typedef struct eos_obj
{
    eos_u8_t type;                          /**< type of kernel object */
    eos_u8_t flag;                          /**< flag of kernel object */
    ek_list_t list;                         /**< list node of kernel object */
} ek_obj_t;

typedef ek_obj_t *ek_obj_handle_t;    /**< Type for kernel objects. */

/* Timer -------------------------------------------------------------------- */
#ifndef EOS_TIMER_SKIP_LIST_LEVEL
#define EOS_TIMER_SKIP_LIST_LEVEL        1
#endif

/**
 * timer structure
 */
typedef struct ek_timer
{
    ek_obj_t super;

    ek_list_t row[EOS_TIMER_SKIP_LIST_LEVEL];

    void (*timeout_func)(void *parameter);
    void *parameter;

    eos_u32_t init_tick;
    eos_u32_t timeout_tick;
} ek_timer_t;

typedef struct ek_timer *ek_timer_handle_t;

/* Task --------------------------------------------------------------------- */

/**
 * Thread structure
 */
typedef struct ek_task
{
    /* eos object */
    eos_u8_t type;                              /**< type of object */
    eos_u8_t flags;                             /**< task's flags */
    
    ek_list_t list;                            /**< the object list */
    ek_list_t tlist;                           /**< the task list */

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
    eos_u8_t current_priority;                 /**< current priority */
    eos_u32_t number_mask;

    eos_ubase_t init_tick;                      /**< task's initialized tick */
    eos_ubase_t remaining_tick;                 /**< remaining tick */

#ifdef EOS_USING_CPU_USAGE
    eos_u64_t duration_tick;                    /**< cpu usage tick */
#endif

    ek_timer_t task_timer;                      /**< built-in task timer */
    
    void (*cleanup)(struct ek_task *tid);      /**< cleanup function when task exit */

    /* light weight process if present */
    void *user_data;                            /**< private user data beyond this task */
} ek_task_t;

typedef struct ek_task *ek_task_handle_t;

eos_err_t ek_task_init(ek_task_handle_t task,
                        void (*entry)(void *parameter),
                        void *parameter,
                        void *stack_start,
                        eos_u32_t stack_size,
                        eos_u8_t priority,
                        eos_u32_t tick);

/* mutex -------------------------------------------------------------------- */
/**
 * Base structure of IPC object
 */
struct ek_ipc_object
{
    ek_obj_t super;                            /**< inherit from eos_object */
    ek_list_t suspend_task;                    /**< tasks pended on this resource */
};

/**
 * Mutual exclusion (mutex) structure
 */
typedef struct ek_mutex
{
    struct ek_ipc_object super;                 /**< inherit from ipc_object */
    eos_u16_t value;                            /**< value of mutex */
    eos_u8_t prio_bkp;                          /**< priority of last task hold the mutex */
    eos_u8_t hold;                              /**< numbers of task hold the mutex */
    struct ek_task *owner;                      /**< current owner of mutex */
} ek_mutex_t;

typedef struct ek_mutex *ek_mutex_handle_t;

/* Semaphore ---------------------------------------------------------------- */
/**
 * Semaphore structure
 */
typedef struct ek_semaphore
{
    struct ek_ipc_object super;                 /**< inherit from ipc_object */
    eos_u16_t value;                            /**< value of semaphore. */
} ek_sem_t;

typedef struct ek_semaphore *ek_sem_handle_t;

#endif
