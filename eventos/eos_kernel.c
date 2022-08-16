/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 * SPDX-License-Identifier: Apache-2.0
 */

/*
EventOS Kernel source files ek.c and ek.h are based on RT-Thread kernel source
files. 

All copyrights in ek.c and ek.h belongs to RT-Thread Development Team. Sincere
thanks to the RT-Thread Team.
*/

#include "eventos.h"
#include <string.h>
#include <stdio.h>

EOS_TAG("EosKernel")

#define EOS_TASK_STAT_YIELD            0x08                /**< indicate whether remaining_tick has been reloaded since last schedule */
#define EOS_TASK_STAT_YIELD_MASK       EOS_TASK_STAT_YIELD

void eos_schedule(void);
void eos_schedule_inseeos_task(ek_task_t *task);
void eos_schedule_remove_task(ek_task_t *task);

eos_u8_t *eos_hw_stack_init(void *entry,
                            void *parameter,
                            eos_u8_t *stack_addr,
                            void *exit);
/*
 * Context interfaces
 */
void eos_hw_context_switch(eos_ubase_t from, eos_ubase_t to);
void eos_hw_context_switch_to(eos_ubase_t to);
void eos_hw_context_switch_interrupt(eos_ubase_t from, eos_ubase_t to);

/**
 * eos_container_of - return the member address of ptr, if the type of ptr is the
 * struct type.
 */
#define eos_container_of(ptr, type, member)                                    \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

/**
 * @brief initialize a list object
 */
#define EOS_LIST_OBJECT_INIT(object) { &(object), &(object) }

/**
 * @brief initialize a list
 * @param l list to be initialized
 */
eos_inline void eos_list_init(eos_list_t *l)
{
    l->next = l->prev = l;
}

/**
 * @brief insert a node after a list
 * @param l list to insert it
 * @param n new node to be inserted
 */
eos_inline void eos_list_inseeos_after(eos_list_t *l, eos_list_t *n)
{
    l->next->prev = n;
    n->next = l->next;

    l->next = n;
    n->prev = l;
}

/**
 * @brief insert a node before a list
 * @param n new node to be inserted
 * @param l list to insert it
 */
eos_inline void eos_list_inseeos_before(eos_list_t *l, eos_list_t *n)
{
    l->prev->next = n;
    n->prev = l->prev;

    l->prev = n;
    n->next = l;
}

/**
 * @brief remove node from list.
 * @param n the node to remove from the list.
 */
eos_inline void eos_list_remove(eos_list_t *n)
{
    n->next->prev = n->prev;
    n->prev->next = n->next;

    n->next = n->prev = n;
}

/**
 * @brief tests whether a list is empty
 * @param l the list to test.
 */
eos_inline int eos_list_isempty(const eos_list_t *l)
{
    return l->next == l;
}

/**
 * @brief get the struct for this entry
 * @param node the entry point
 * @param type the type of structure
 * @param member the name of list in structure
 */
#define eos_list_entry(node, type, member) eos_container_of(node, type, member)

/*
 * define object_info for the number of _object_container items.
 */
enum eos_object_info_type
{
    EosObjInfo_Thread = 0,                         /**< The object is a task. */
#ifdef EOS_USING_SEMAPHORE
    EosObjInfo_Semaphore,                          /**< The object is a semaphore. */
#endif
#ifdef EOS_USING_MUTEX
    EosObjInfo_Mutex,                              /**< The object is a mutex. */
#endif
    EosObjInfo_Timer,                              /**< The object is a timer. */

    EosObjInfo_Max
};

#define _OBJ_CONTAINER_LIST_INIT(c)     \
    {&(_object_container[c].object_list), &(_object_container[c].object_list)}

static struct eos_obj_info _object_container[EosObjInfo_Max] =
{
    /* initialize object container - task */
    {
        EOS_Object_Thread,
        _OBJ_CONTAINER_LIST_INIT(EosObjInfo_Thread),
        sizeof(ek_task_t)
    },
#ifdef EOS_USING_SEMAPHORE
    /* initialize object container - semaphore */
    {
        EOS_Object_Semaphore,
        _OBJ_CONTAINER_LIST_INIT(EosObjInfo_Semaphore),
        sizeof(eos_sem_t)
    },
#endif
#ifdef EOS_USING_MUTEX
    /* initialize object container - mutex */
    {
        EOS_Object_Mutex,
        _OBJ_CONTAINER_LIST_INIT(EosObjInfo_Mutex),
        sizeof(eos_mutex_t)
    },
#endif
    /* initialize object container - timer */
    {
        EOS_Object_Timer,
        _OBJ_CONTAINER_LIST_INIT(EosObjInfo_Timer),
        sizeof(eos_timer_t)
    },
};

/**
 * @brief This function will return the specified type of object information.
 * @param type is the type of object, which can be
 *             EOS_Object_Thread/Semaphore/Mutex... etc
 * @return the object type information or EOS_NULL
 */
struct eos_obj_info *eos_object_get_info(enum eos_obj_type type)
{
    for (eos_u32_t i = 0; i < EosObjInfo_Max; i ++)
    {
        if (_object_container[i].type == type)
        {
            return &_object_container[i];
        }
    }

    return EOS_NULL;
}

/**
 * @brief This function will initialize an object and add it to object system
 *        management.
 * @param object is the specified object to be initialized.
 * @param type is the object type.
 * @param name is the object name. In system, the object's name must be unique.
 */
void eos_object_init(eos_obj_t *object,
                     enum eos_obj_type type,
                     const char *name)
{
    register eos_base_t temp;
    struct eos_list_node *node = EOS_NULL;
    struct eos_obj_info *information;

    /* get object information */
    information = eos_object_get_info(type);
    EOS_ASSERT(information != EOS_NULL);

    /* enter critical */
    eos_enter_critical();
    /* try to find object */
    for (node  = information->object_list.next;
            node != &(information->object_list);
            node  = node->next)
    {
        eos_obj_t *obj;

        obj = eos_list_entry(node, eos_obj_t, list);
        if (obj) /* skip warning when disable debug */
        {
            EOS_ASSERT(obj != object);
        }
    }
    /* leave critical */
    eos_exit_critical();

    /* initialize object's parameters */
    /* set object type to static */
    object->type = type | EOS_Object_Static;

    /* lock interrupt */
    temp = eos_hw_interrupt_disable();

    /* insert object into information object list */
    eos_list_inseeos_after(&(information->object_list), &(object->list));

    /* unlock interrupt */
    eos_hw_interrupt_enable(temp);
}

/**
 * @brief This function will detach a static object from object system,
 *        and the memory of static object is not freed.
 * @param object the specified object to be detached.
 */
void eos_object_detach(eos_obj_handle_t object)
{
    register eos_base_t temp;

    /* object check */
    EOS_ASSERT(object != EOS_NULL);

    /* reset object type */
    object->type = 0;

    /* lock interrupt */
    temp = eos_hw_interrupt_disable();

    /* remove from old list */
    eos_list_remove(&(object->list));

    /* unlock interrupt */
    eos_hw_interrupt_enable(temp);
}

/**
 * @brief This function will judge the object is system object or not.
 * @note  Normally, the system object is a static object and the type
 *        of object set to EOS_Object_Static.
 * @param object is the specified object to be judged.
 * @return EOS_True if a system object, EOS_False for others.
 */
eos_bool_t eos_object_is_systemobject(eos_obj_handle_t object)
{
    eos_bool_t ret = EOS_False;
    
    /* object check */
    EOS_ASSERT(object != EOS_NULL);

    if (object->type & EOS_Object_Static)
    {
        ret = EOS_True;
    }

    return ret;
}

/**
 * @brief This function will return the type of object without EOS_Object_Static flag.
 * @param object is the specified object to be get type.
 * @return the type of object.
 */
eos_u8_t eos_object_get_type(eos_obj_handle_t object)
{
    /* object check */
    EOS_ASSERT(object != EOS_NULL);

    return object->type & ~EOS_Object_Static;
}

volatile eos_u8_t eos_interrupt_nest = 0;

/**
 * @brief This function will be invoked by BSP, when enter interrupt service routine
 * @note Please don't invoke this routine in application
 * @see eos_interrupt_leave
 */
void eos_interrupt_enter(void)
{
    eos_base_t level;

    level = eos_hw_interrupt_disable();
    eos_interrupt_nest ++;
    eos_hw_interrupt_enable(level);
}

/**
 * @brief This function will be invoked by BSP, when leave interrupt service routine
 * @note Please don't invoke this routine in application
 * @see eos_interrupt_enter
 */
void eos_interrupt_leave(void)
{
    eos_base_t level;

    level = eos_hw_interrupt_disable();
    eos_interrupt_nest --;
    eos_hw_interrupt_enable(level);
}

/**
 * @brief This function will return the nest of interrupt.
 * User application can invoke this function to get whether current
 * context is interrupt context.
 * @return the number of nested interrupts.
 */
eos_u8_t eos_interrupt_get_nest(void)
{
    eos_u8_t ret;
    eos_base_t level;

    level = eos_hw_interrupt_disable();
    ret = eos_interrupt_nest;
    eos_hw_interrupt_enable(level);
    return ret;
}

/* global errno in EventOS Kernel */
static volatile int __eos_errno;

/**
 * This function gets the global errno for the current task.
 * @return errno
 */
eos_err_t eos_get_errno(void)
{
    eos_err_t ret;
    ek_task_handle_t tid = (ek_task_handle_t)eos_task_self();

    if (eos_interrupt_get_nest() != 0)
    {
        /* it's in interrupt context */
        ret = __eos_errno;
        goto exit;
    }

    if (tid == EOS_NULL)
    {
        ret = __eos_errno;
        goto exit;
    }

    ret = tid->error;

exit:
    return ret;
}

/**
 * This function sets the global errno for the current task.
 * @param error is the errno shall be set.
 */
void eos_set_errno(eos_err_t error)
{
    ek_task_handle_t tid;

    if (eos_interrupt_get_nest() != 0)
    {
        /* it's in interrupt context */
        __eos_errno = error;

        return;
    }

    tid = (ek_task_handle_t)eos_task_self();
    if (tid == EOS_NULL)
    {
        __eos_errno = error;

        return;
    }

    tid->error = error;
}

/**
 * This function returns the address of the current task errno.
 * @return The errno address.
 */
int *_eos_errno(void)
{
    ek_task_handle_t tid;

    if (eos_interrupt_get_nest() != 0)
        return (int *)&__eos_errno;

    tid = (ek_task_handle_t)eos_task_self();
    if (tid != EOS_NULL)
        return (int *) & (tid->error);

    return (int *)&__eos_errno;
}

#ifndef EOS_USING_CPU_FFS
#ifdef EOS_USING_TINY_FFS
const eos_u8_t __lowest_bit_bitmap[] =
{
    /*  0 - 7  */  0,  1,  2, 27,  3, 24, 28, 32,
    /*  8 - 15 */  4, 17, 25, 31, 29, 12, 32, 14,
    /* 16 - 23 */  5,  8, 18, 32, 26, 23, 32, 16,
    /* 24 - 31 */ 30, 11, 13,  7, 32, 22, 15, 10,
    /* 32 - 36 */  6, 21,  9, 20, 19
};

/**
 * This function finds the first bit set (beginning with the least significant bit)
 * in value and return the index of that bit.
 * Bits are numbered starting at 1 (the least significant bit).  A return value of
 * zero from any of these functions means that the argument was zero.
 * @return return the index of the first bit set. If value is 0, then this function
 * shall return 0.
 */
int __eos_ffs(int value)
{
    return __lowest_bit_bitmap[(eos_u32_t)(value & (value - 1) ^ value) % 37];
}
#else
const eos_u8_t __lowest_bit_bitmap[] =
{
    /* 00 */ 0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 10 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 20 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 30 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 40 */ 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 50 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 60 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 70 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 80 */ 7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* 90 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* A0 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* B0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* C0 */ 6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* D0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* E0 */ 5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
    /* F0 */ 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};

/**
 * This function finds the first bit set (beginning with the least significant bit)
 * in value and return the index of that bit.
 * Bits are numbered starting at 1 (the least significant bit).  A return value of
 * zero from any of these functions means that the argument was zero.
 * @return Return the index of the first bit set. If value is 0, then this function
 *         shall return 0.
 */
int __eos_ffs(int value)
{
    if (value == 0)
        return 0;

    if (value & 0xff)
        return __lowest_bit_bitmap[value & 0xff] + 1;

    if (value & 0xff00)
        return __lowest_bit_bitmap[(value & 0xff00) >> 8] + 9;

    if (value & 0xff0000)
        return __lowest_bit_bitmap[(value & 0xff0000) >> 16] + 17;

    return __lowest_bit_bitmap[(value & 0xff000000) >> 24] + 25;
}
#endif /* EOS_USING_TINY_FFS */
#endif /* EOS_USING_CPU_FFS */

eos_list_t eos_task_priority_table[EOS_TASK_PRIORITY_MAX];
eos_u32_t eos_task_ready_priority_group;

extern volatile eos_u8_t eos_interrupt_nest;
static eos_s16_t eos_scheduler_lock_nest;
ek_task_handle_t eos_current_task = EOS_NULL;
eos_u8_t eos_current_priority;

#ifdef EOS_USING_OVERFLOW_CHECK
static void _eos_scheduler_stack_check(ek_task_handle_t task)
{
    EOS_ASSERT(task != EOS_NULL);

#ifdef ARCH_CPU_STACK_GROWS_UPWARD
    if (*((eos_u8_t *)((eos_ubase_t)task->stack_addr + task->stack_size - 1)) != '#' ||
#else
    if (*((eos_u8_t *)task->stack_addr) != '#' ||
#endif /* ARCH_CPU_STACK_GROWS_UPWARD */
        (eos_ubase_t)task->sp <= (eos_ubase_t)task->stack_addr ||
        (eos_ubase_t)task->sp >
        (eos_ubase_t)task->stack_addr + (eos_ubase_t)task->stack_size)
    {
        eos_ubase_t level;

        level = eos_hw_interrupt_disable();
        while (level);
    }
#ifdef ARCH_CPU_STACK_GROWS_UPWARD
    else if ((eos_ubase_t)task->sp > ((eos_ubase_t)task->stack_addr + task->stack_size))
    {

    }
#else
    else if ((eos_ubase_t)task->sp <= ((eos_ubase_t)task->stack_addr + 32))
    {

    }
#endif /* ARCH_CPU_STACK_GROWS_UPWARD */
}
#endif /* EOS_USING_OVERFLOW_CHECK */

/*
 * get the highest priority task in ready queue
 */
static ek_task_t* _scheduler_get_highest_task(eos_ubase_t *highest_prio)
{
    register ek_task_handle_t highest_task;
    register eos_ubase_t highest_ready_priority;

    highest_ready_priority = __eos_ffs(eos_task_ready_priority_group) - 1;

    /* get highest ready priority task */
    highest_task = eos_list_entry(eos_task_priority_table[highest_ready_priority].next,
                              ek_task_t,
                              tlist);

    *highest_prio = highest_ready_priority;

    return highest_task;
}

/**
 * @brief This function will initialize the system scheduler.
 */
void eos_kernel_init(void)
{
    register eos_base_t offset;

    eos_scheduler_lock_nest = 0;

    for (offset = 0; offset < EOS_TASK_PRIORITY_MAX; offset ++)
    {
        eos_list_init(&eos_task_priority_table[offset]);
    }

    /* initialize ready priority group */
    eos_task_ready_priority_group = 0;
}

/**
 * @brief This function will startup the scheduler. It will select one task
 *        with the highest priority level, then switch to it.
 */
void eos_kernel_start(void)
{
    register ek_task_handle_t to_task;
    eos_ubase_t highest_ready_priority;

    to_task = _scheduler_get_highest_task(&highest_ready_priority);

    eos_current_task = to_task;

    eos_schedule_remove_task(to_task);
    to_task->status = EOS_TASK_RUNNING;

    /* switch to new task */
    eos_hw_context_switch_to((eos_ubase_t)&to_task->sp);

    /* never come back */
}

#ifdef EOS_USING_OVERFLOW_CHECK
#endif /* EOS_USING_OVERFLOW_CHECK */
/**
 * @brief This function will perform scheduling once. It will select one task
 *        with the highest priority, and switch to it immediately.
 */
void eos_schedule(void)
{
    eos_base_t level;
    ek_task_handle_t to_task;
    ek_task_handle_t from_task;

    /* disable interrupt */
    level = eos_hw_interrupt_disable();

    /* check the scheduler is enabled or not */
    if (eos_scheduler_lock_nest == 0)
    {
        eos_ubase_t highest_ready_priority;

        if (eos_task_ready_priority_group != 0)
        {
            /* need_inseeos_from_task: need to insert from_task to ready queue */
            int need_inseeos_from_task = 0;

            to_task = _scheduler_get_highest_task(&highest_ready_priority);

            if ((eos_current_task->status & EOS_TASK_STAT_MASK) == EOS_TASK_RUNNING)
            {
                if (eos_current_task->current_priority < highest_ready_priority)
                {
                    to_task = eos_current_task;
                }
                else if (eos_current_task->current_priority == highest_ready_priority && (eos_current_task->status & EOS_TASK_STAT_YIELD_MASK) == 0)
                {
                    to_task = eos_current_task;
                }
                else
                {
                    need_inseeos_from_task = 1;
                }
                eos_current_task->status &= ~EOS_TASK_STAT_YIELD_MASK;
            }

            if (to_task != eos_current_task)
            {
                /* if the destination task is not the same as current task */
                eos_current_priority = (eos_u8_t)highest_ready_priority;
                from_task         = eos_current_task;
                eos_current_task   = to_task;

                if (need_inseeos_from_task)
                {
                    eos_schedule_inseeos_task(from_task);
                }

                eos_schedule_remove_task(to_task);
                to_task->status = EOS_TASK_RUNNING | (to_task->status & ~EOS_TASK_STAT_MASK);

                /* switch to new task */

#ifdef EOS_USING_OVERFLOW_CHECK
                _eos_scheduler_stack_check(to_task);
#endif /* EOS_USING_OVERFLOW_CHECK */

                if (eos_interrupt_nest == 0)
                {
                    extern void eos_task_handle_sig(eos_bool_t clean_state);

            

                    eos_hw_context_switch((eos_ubase_t)&from_task->sp,
                            (eos_ubase_t)&to_task->sp);

                    /* enable interrupt */
                    eos_hw_interrupt_enable(level);

                    goto __exit;
                }
                else
                {

                    eos_hw_context_switch_interrupt((eos_ubase_t)&from_task->sp,
                            (eos_ubase_t)&to_task->sp);
                }
            }
            else
            {
                eos_schedule_remove_task(eos_current_task);
                eos_current_task->status =
                    EOS_TASK_RUNNING |
                    (eos_current_task->status & ~EOS_TASK_STAT_MASK);
            }
        }
    }

    /* enable interrupt */
    eos_hw_interrupt_enable(level);

__exit:
    return;
}

/**
 * @brief This function will insert a task to the system ready queue. The state of
 *        task will be set as READY and the task will be removed from suspend queue.
 * @param task is the task to be inserted.
 * @note  Please do not invoke this function in user application.
 */
void eos_schedule_inseeos_task(ek_task_handle_t task)
{
    register eos_base_t temp;

    EOS_ASSERT(task != EOS_NULL);

    /* disable interrupt */
    temp = eos_hw_interrupt_disable();

    /* it's current task, it should be RUNNING task */
    if (task == eos_current_task)
    {
        task->status = EOS_TASK_RUNNING | (task->status & ~EOS_TASK_STAT_MASK);
        goto __exit;
    }

    /* READY task, insert to ready queue */
    task->status = EOS_TASK_READY | (task->status & ~EOS_TASK_STAT_MASK);
    /* insert task to ready list */
    eos_list_inseeos_before(&(eos_task_priority_table[task->current_priority]),
                          &(task->tlist));

    /* set priority mask */
    eos_task_ready_priority_group |= task->number_mask;

__exit:
    /* enable interrupt */
    eos_hw_interrupt_enable(temp);
}

/**
 * @brief This function will remove a task from system ready queue.
 * @param task is the task to be removed.
 * @note  Please do not invoke this function in user application.
 */
void eos_schedule_remove_task(ek_task_handle_t task)
{
    register eos_base_t level;

    EOS_ASSERT(task != EOS_NULL);

    /* disable interrupt */
    level = eos_hw_interrupt_disable();

    /* remove task from ready list */
    eos_list_remove(&(task->tlist));
    if (eos_list_isempty(&(eos_task_priority_table[task->current_priority])))
    {
        eos_task_ready_priority_group &= ~task->number_mask;
    }

    /* enable interrupt */
    eos_hw_interrupt_enable(level);
}

/**
 * @brief This function will lock the task scheduler.
 */
void eos_enter_critical(void)
{
    register eos_base_t level;

    /* disable interrupt */
    level = eos_hw_interrupt_disable();

    /*
     * the maximal number of nest is EOS_U16_MAX, which is big
     * enough and does not check here
     */
    eos_scheduler_lock_nest ++;

    /* enable interrupt */
    eos_hw_interrupt_enable(level);
}

/**
 * @brief This function will unlock the task scheduler.
 */
void eos_exit_critical(void)
{
    register eos_base_t level;

    /* disable interrupt */
    level = eos_hw_interrupt_disable();

    eos_scheduler_lock_nest --;
    if (eos_scheduler_lock_nest <= 0)
    {
        eos_scheduler_lock_nest = 0;
        /* enable interrupt */
        eos_hw_interrupt_enable(level);

        if (eos_current_task)
        {
            /* if scheduler is started, do a schedule */
            eos_schedule();
        }
    }
    else
    {
        /* enable interrupt */
        eos_hw_interrupt_enable(level);
    }
}

/**
 * @brief Get the scheduler lock level.
 * @return the level of the scheduler lock. 0 means unlocked.
 */
eos_u16_t eos_critical_level(void)
{
    return eos_scheduler_lock_nest;
}

#include <stddef.h>

static void _task_exit(void)
{
    ek_task_handle_t task;
    register eos_base_t level;

    /* get current task */
    task = (ek_task_handle_t)eos_task_self();

    /* disable interrupt */
    level = eos_hw_interrupt_disable();

    /* remove from schedule */
    eos_schedule_remove_task(task);

    /* remove it from timer list */
    eos_timer_detach((eos_timer_handle_t)&task->task_timer);

    /* change stat */
    task->status = EOS_TASK_CLOSE;

    /* insert to defunct task list */
    eos_task_defunct_enqueue((eos_task_handle_t)task);

    /* enable interrupt */
    eos_hw_interrupt_enable(level);

    /* switch to next task */
    eos_schedule();
}

/**
 * @brief   This function is the timeout function for task, normally which is invoked
 *          when task is timeout to wait some resource.
 * @param   parameter is the parameter of task timeout function
 */
static void _task_timeout(void *parameter)
{
    ek_task_handle_t task;
    register eos_base_t temp;

    task = (ek_task_handle_t )parameter;

    /* parameter check */
    EOS_ASSERT(task != EOS_NULL);
    EOS_ASSERT((task->status & EOS_TASK_STAT_MASK) == EOS_TASK_SUSPEND);
    EOS_ASSERT(eos_object_get_type((eos_obj_handle_t)task) == EOS_Object_Thread);

    /* disable interrupt */
    temp = eos_hw_interrupt_disable();

    /* set error number */
    task->error = EOS_ETIMEOUT;

    /* remove from suspend list */
    eos_list_remove(&(task->tlist));

    /* insert to schedule ready list */
    eos_schedule_inseeos_task(task);

    /* enable interrupt */
    eos_hw_interrupt_enable(temp);

    /* do schedule */
    eos_schedule();
}

static eos_err_t _task_init(ek_task_handle_t task,
                             const char *name,
                             void (*entry)(void *parameter),
                             void *parameter,
                             void *stack_start,
                             eos_u32_t stack_size,
                             eos_u8_t priority,
                             eos_u32_t tick)
{
    /* init task list */
    eos_list_init(&(task->tlist));

    task->entry = (void *)entry;
    task->parameter = parameter;

    /* stack init */
    task->stack_addr = stack_start;
    task->stack_size = stack_size;

    /* init task stack */
    memset(task->stack_addr, '#', task->stack_size);
#ifdef ARCH_CPU_STACK_GROWS_UPWARD
    task->sp = (void *)eos_hw_stack_init(task->entry, task->parameter,
                                          (void *)((char *)task->stack_addr),
                                          (void *)_task_exit);
#else
    task->sp = (void *)eos_hw_stack_init(task->entry, task->parameter,
                                          (eos_u8_t *)((char *)task->stack_addr + task->stack_size - sizeof(eos_ubase_t)),
                                          (void *)_task_exit);
#endif /* ARCH_CPU_STACK_GROWS_UPWARD */

    /* priority init */
    EOS_ASSERT(priority < EOS_TASK_PRIORITY_MAX);
    task->current_priority = priority;

    task->number_mask = 0;

    /* tick init */
    task->init_tick      = tick;
    task->remaining_tick = tick;

    /* error and flags */
    task->error = EOS_EOK;
    task->status  = EOS_TASK_INIT;

    /* initialize cleanup function and user data */
    task->cleanup   = 0;
    task->user_data = EOS_NULL;

    /* initialize task timer */
    eos_timer_init((eos_timer_handle_t)&(task->task_timer),
                    NULL,
                    _task_timeout,
                    task,
                    0,
                    EOS_TIMER_FLAG_ONE_SHOT);

#ifdef EOS_USING_CPU_USAGE
    task->duration_tick = 0;
#endif

    return EOS_EOK;
}

/**
 * @brief   This function will initialize a task. It's used to initialize a
 *          static task object.
 * @param   task is the static task object.
 * @param   name is the name of task, which shall be unique.
 * @param   entry is the entry function of task.
 * @param   parameter is the parameter of task enter function.
 * @param   stack_start is the start address of task stack.
 * @param   stack_size is the size of task stack.
 * @param   priority is the priority of task.
 * @param   tick is the time slice if there are same priority task.
 * @return  Return the operation status. If the return value is EOS_EOK, the function is successfully executed.
 *          If the return value is any other values, it means this operation failed.
 */
eos_err_t ek_task_init(ek_task_handle_t task,
                        const char *name,
                        void (*entry)(void *parameter),
                        void *parameter,
                        void *stack_start,
                        eos_u32_t stack_size,
                        eos_u8_t priority,
                        eos_u32_t tick)
{
    /* parameter check */
    EOS_ASSERT(task != EOS_NULL);
    EOS_ASSERT(stack_start != EOS_NULL);

    /* initialize task object */
    eos_object_init((eos_obj_handle_t)task, EOS_Object_Thread, name);

    return _task_init(task, name, entry, parameter, stack_start, stack_size,
                        priority,
                        tick);
}

/**
 * @brief   This function will return self task object.
 * @return  The self task object.
 */
eos_task_handle_t eos_task_self(void)
{
    extern ek_task_handle_t eos_current_task;

    return (eos_task_handle_t)eos_current_task;
}

/**
 * @brief   This function will start a task and put it to system ready queue.
 * @param   task is the task to be started.
 * @return  Return the operation status. If the return value is EOS_EOK, the function is successfully executed.
 *          If the return value is any other values, it means this operation failed.
 */
eos_err_t eos_task_startup(eos_task_handle_t task)
{
    ek_task_handle_t task_ = (ek_task_handle_t)task;

    /* parameter check */
    EOS_ASSERT(task_ != EOS_NULL);
    EOS_ASSERT((task_->status & EOS_TASK_STAT_MASK) == EOS_TASK_INIT);
    EOS_ASSERT(eos_object_get_type((eos_obj_handle_t)task_) == EOS_Object_Thread);

    /* calculate priority attribute */
    task_->number_mask = 1L << task_->current_priority;

    /* change task stat */
    task_->status = EOS_TASK_SUSPEND;
    /* then resume it */
    eos_task_resume(task);
    if (eos_task_self() != EOS_NULL)
    {
        /* do a scheduling */
        eos_schedule();
    }

    return EOS_EOK;
}

/**
 * @brief   This function will detach a task. The task object will be removed from
 *          task queue and detached/deleted from the system object management.
 * @param   task is the task to be deleted.
 * @return  Return the operation status. If the return value is EOS_EOK, the function is successfully executed.
 *          If the return value is any other values, it means this operation failed.
 */
eos_err_t eos_task_detach(eos_task_handle_t task_)
{
    ek_task_handle_t task = (ek_task_handle_t)task_;

    eos_base_t lock;

    /* parameter check */
    EOS_ASSERT(task != EOS_NULL);
    EOS_ASSERT(eos_object_get_type((eos_obj_handle_t)task) == EOS_Object_Thread);
    EOS_ASSERT(eos_object_is_systemobject((eos_obj_handle_t)task));

    if ((task->status & EOS_TASK_STAT_MASK) == EOS_TASK_CLOSE)
        return EOS_EOK;

    if ((task->status & EOS_TASK_STAT_MASK) != EOS_TASK_INIT)
    {
        /* remove from schedule */
        eos_schedule_remove_task(task);
    }

    /* disable interrupt */
    lock = eos_hw_interrupt_disable();

    /* release task timer */
    eos_timer_detach((eos_timer_handle_t)&(task->task_timer));

    /* change stat */
    task->status = EOS_TASK_CLOSE;

    /* insert to defunct task list */
    eos_task_defunct_enqueue(task_);

    /* enable interrupt */
    eos_hw_interrupt_enable(lock);

    return EOS_EOK;
}

/**
 * @brief   This function will let current task yield processor, and scheduler will
 *          choose the highest task to run. After yield processor, the current task
 *          is still in READY state.
 * @return  Return the operation status. If the return value is EOS_EOK, the function is successfully executed.
 *          If the return value is any other values, it means this operation failed.
 */
eos_err_t eos_task_yield(void)
{
    ek_task_handle_t task;
    eos_base_t lock;

    task = (ek_task_handle_t)eos_task_self();
    lock = eos_hw_interrupt_disable();
    task->remaining_tick = task->init_tick;
    task->status |= EOS_TASK_STAT_YIELD;
    eos_schedule();
    eos_hw_interrupt_enable(lock);

    return EOS_EOK;
}

/**
 * @brief   This function will let current task sleep for some ticks. Change
 *          current task state to suspend, when the task timer reaches the tick
 *           value, scheduler will awaken this task.
 * @param   tick is the sleep ticks.
 * @return  Return the operation status. If the return value is EOS_EOK, the
 *          function is successfully executed. If the return value is any other 
 *          values, it means this operation failed.
 */
eos_err_t eos_task_sleep(eos_u32_t tick)
{
    register eos_base_t temp;
    ek_task_handle_t task;

    /* set to current task */
    task = (ek_task_handle_t)eos_task_self();
    EOS_ASSERT(task != EOS_NULL);
    EOS_ASSERT(eos_object_get_type((eos_obj_handle_t)task) == EOS_Object_Thread);

    /* disable interrupt */
    temp = eos_hw_interrupt_disable();

    /* reset task error */
    task->error = EOS_EOK;

    /* suspend task */
    eos_task_suspend((eos_task_handle_t)task);

    /* reset the timeout of task timer and start it */
    eos_timer_control((eos_timer_handle_t)&(task->task_timer),
                        EOS_TIMER_CTRL_SET_TIME,
                        &tick);
    eos_timer_start((eos_timer_handle_t)&(task->task_timer));

    /* enable interrupt */
    eos_hw_interrupt_enable(temp);

    eos_schedule();

    /* clear error number of this task to EOS_EOK */
    if (task->error == EOS_ETIMEOUT)
        task->error = EOS_EOK;

    return task->error;
}

/**
 * @brief   This function will let current task delay for some ticks.
 * @param   tick is the delay ticks.
 * @return  Return the operation status. If the return value is EOS_EOK, the function is successfully executed.
 *          If the return value is any other values, it means this operation failed.
 */
eos_err_t eos_task_delay(eos_u32_t tick)
{
    return eos_task_sleep(tick);
}

/**
 * @brief   This function will let current task delay until (*tick + inc_tick).
 * @param   tick is the tick of last wakeup.
 * @param   inc_tick is the increment tick.
 * @return  Return the operation status. If the return value is EOS_EOK, the function is successfully executed.
 *          If the return value is any other values, it means this operation failed.
 */
eos_err_t eos_task_delay_until(eos_u32_t *tick, eos_u32_t inc_tick)
{
    register eos_base_t level;
    ek_task_handle_t task;
    eos_u32_t cur_tick;

    EOS_ASSERT(tick != EOS_NULL);

    /* set to current task */
    task = (ek_task_handle_t)eos_task_self();
    EOS_ASSERT(task != EOS_NULL);
    EOS_ASSERT(eos_object_get_type((eos_obj_handle_t)task) == EOS_Object_Thread);

    /* disable interrupt */
    level = eos_hw_interrupt_disable();

    /* reset task error */
    task->error = EOS_EOK;

    cur_tick = eos_tick_get();
    if (cur_tick - *tick < inc_tick)
    {
        eos_u32_t left_tick;

        *tick += inc_tick;
        left_tick = *tick - cur_tick;

        /* suspend task */
        eos_task_suspend((eos_task_handle_t)task);

        /* reset the timeout of task timer and start it */
        eos_timer_control((eos_timer_handle_t)&(task->task_timer),
                            EOS_TIMER_CTRL_SET_TIME,
                            &left_tick);
        eos_timer_start((eos_timer_handle_t)&(task->task_timer));

        /* enable interrupt */
        eos_hw_interrupt_enable(level);

        eos_schedule();

        /* clear error number of this task to EOS_EOK */
        if (task->error == EOS_ETIMEOUT)
        {
            task->error = EOS_EOK;
        }
    }
    else
    {
        *tick = cur_tick;
        eos_hw_interrupt_enable(level);
    }

    return task->error;
}

/**
 * @brief   This function will let current task delay for some milliseconds.
 * @param   ms is the delay ms time.
 * @return  Return the operation status. If the return value is EOS_EOK, the function is successfully executed.
 *          If the return value is any other values, it means this operation failed.
 */
eos_err_t eos_task_mdelay(eos_s32_t ms)
{
    eos_u32_t tick;

    tick = eos_tick_from_millisecond(ms);

    return eos_task_sleep(tick);
}

/**
 * @brief   This function will control task behaviors according to control command.
 * @param   task is the specified task to be controlled.
 * @param   cmd is the control command, which includes.
 *              EOS_TASK_CTRL_CHANGE_PRIORITY for changing priority level of task.
 *              EOS_TASK_CTRL_STARTUP for starting a task.
 *              EOS_TASK_CTRL_CLOSE for delete a task.
 *              EOS_TASK_CTRL_BIND_CPU for bind the task to a CPU.
 * @param   arg is the argument of control command.
 * @return  Return the operation status. If the return value is EOS_EOK, the function is successfully executed.
 *          If the return value is any other values, it means this operation failed.
 */
eos_err_t eos_task_control(eos_task_handle_t task_, int cmd, void *arg)
{
    ek_task_handle_t task = (ek_task_handle_t)task_;
    register eos_base_t temp;

    /* parameter check */
    EOS_ASSERT(task != EOS_NULL);
    EOS_ASSERT(eos_object_get_type((eos_obj_handle_t)task) == EOS_Object_Thread);

    switch (cmd)
    {
        case EOS_TASK_CTRL_CHANGE_PRIORITY:
        {
            /* disable interrupt */
            temp = eos_hw_interrupt_disable();

            /* for ready task, change queue */
            if ((task->status & EOS_TASK_STAT_MASK) == EOS_TASK_READY)
            {
                /* remove task from schedule queue first */
                eos_schedule_remove_task(task);

                /* change task priority */
                task->current_priority = *(eos_u8_t *)arg;

                /* recalculate priority attribute */
                task->number_mask = 1 << task->current_priority;

                /* insert task to schedule queue again */
                eos_schedule_inseeos_task(task);
            }
            else
            {
                task->current_priority = *(eos_u8_t *)arg;

                /* recalculate priority attribute */
                task->number_mask = 1 << task->current_priority;
            }

            /* enable interrupt */
            eos_hw_interrupt_enable(temp);
            break;
        }

        case EOS_TASK_CTRL_STARTUP:
        {
            return eos_task_startup(task_);
        }

        case EOS_TASK_CTRL_CLOSE:
        {
            eos_err_t eos_err;

            if (eos_object_is_systemobject((eos_obj_handle_t)task) == EOS_True)
            {
                eos_err = eos_task_detach(task_);
            }
            eos_schedule();
            return eos_err;
        }

        default:
            break;
    }

    return EOS_EOK;
}

/**
 * @brief   This function will suspend the specified task and change it to suspend state.
 * @note    This function ONLY can suspend current task itself.
 *          Do not use the eos_task_suspend and eos_task_resume functions to synchronize the activities of tasks.
 *          You have no way of knowing what code a task is executing when you suspend it.
 *          If you suspend a task while it is executing a critical area which is protected by a mutex,
 *          other tasks attempt to use that mutex and have to wait. Deadlocks can occur very easily.
 * @param   task is the task to be suspended.
 * @return  Return the operation status. If the return value is EOS_EOK, the function is successfully executed.
 *          If the return value is any other values, it means this operation failed.
 */
eos_err_t eos_task_suspend(eos_task_handle_t task_)
{
    ek_task_handle_t task = (ek_task_handle_t)task_;
    register eos_base_t stat;
    register eos_base_t temp;

    /* parameter check */
    EOS_ASSERT(task != EOS_NULL);
    EOS_ASSERT(eos_object_get_type((eos_obj_handle_t)task) == EOS_Object_Thread);
    EOS_ASSERT(task == (ek_task_handle_t)eos_task_self());

    stat = task->status & EOS_TASK_STAT_MASK;
    if ((stat != EOS_TASK_READY) && (stat != EOS_TASK_RUNNING))
    {
        return EOS_ERROR;
    }

    /* disable interrupt */
    temp = eos_hw_interrupt_disable();

    /* change task stat */
    eos_schedule_remove_task(task);
    task->status = EOS_TASK_SUSPEND | (task->status & ~EOS_TASK_STAT_MASK);

    /* stop task timer anyway */
    eos_timer_stop((eos_timer_handle_t)&(task->task_timer));

    /* enable interrupt */
    eos_hw_interrupt_enable(temp);

    return EOS_EOK;
}

/**
 * @brief   This function will resume a task and put it to system ready queue.
 * @note    Do not use the eos_task_suspend and eos_task_resume functions to synchronize the activities of tasks.
 * @param   task is the task to be resumed.
 * @return  Return the operation status. If the return value is EOS_EOK, the function is successfully executed.
 *          If the return value is any other values, it means this operation failed.
 */
eos_err_t eos_task_resume(eos_task_handle_t task_)
{
    register eos_base_t temp;
    ek_task_handle_t task = (ek_task_handle_t)task_;

    /* parameter check */
    EOS_ASSERT(task != EOS_NULL);
    EOS_ASSERT(eos_object_get_type((eos_obj_handle_t)task) == EOS_Object_Thread);

    if ((task->status & EOS_TASK_STAT_MASK) != EOS_TASK_SUSPEND)
    {
        return EOS_ERROR;
    }

    /* disable interrupt */
    temp = eos_hw_interrupt_disable();

    /* remove from suspend list */
    eos_list_remove(&(task->tlist));

    eos_timer_stop((eos_timer_handle_t)&task->task_timer);

    /* insert to schedule ready list */
    eos_schedule_inseeos_task(task);

    /* enable interrupt */
    eos_hw_interrupt_enable(temp);

    return EOS_EOK;
}

eos_task_state_t eos_task_get_state(eos_task_handle_t task)
{
    return (eos_task_state_t)(
                ((ek_task_handle_t)task)->status & (eos_u8_t)EOS_TASK_STAT_MASK
            );
}

/**
 * @brief    This function will initialize an IPC object, such as semaphore, mutex, messagequeue and mailbox.
 * @note     Executing this function will complete an initialization of the suspend task list of the ipc object.
 * @param    ipc is a pointer to the IPC object.
 * @return   Return the operation status. When the return value is EOS_EOK, the initialization is successful.
 *           When the return value is any other values, it means the initialization failed.
 * @warning  This function can be called from all IPC initialization and creation.
 */

eos_inline eos_err_t _ipc_object_init(struct ek_ipc_object *ipc)
{
    /* initialize ipc object */
    eos_list_init(&(ipc->suspend_task));

    return EOS_EOK;
}

/**
 * @brief    This function will suspend a task to a IPC object list.
 * @param    list is a pointer to a suspended task list of the IPC object.
 * @param    task is a pointer to the task object to be suspended.
 * @param    flag is a flag for the task object to be suspended. It determines how the task is suspended.
 *           The flag can be ONE of the following values:
 *               EOS_IPC_FLAG_PRIO          The pending tasks will queue in order of priority.
 *               EOS_IPC_FLAG_FIFO          The pending tasks will queue in the first-in-first-out method
 *                                         (also known as first-come-first-served (FCFS) scheduling strategy).
 *               NOTE: EOS_IPC_FLAG_FIFO is a non-real-time scheduling mode. It is strongly recommended to use
 *               EOS_IPC_FLAG_PRIO to ensure the task is real-time UNLESS your applications concern about
 *               the first-in-first-out principle, and you clearly understand that all tasks involved in
 *               this semaphore will become non-real-time tasks.
 * @return   Return the operation status. When the return value is EOS_EOK, the function is successfully executed.
 *           When the return value is any other values, it means the initialization failed.
 * @warning  This function can ONLY be called in the task context, you can use EOS_DEBUG_IN_THREAD_CONTEXT to
 *           check the context.
 *           In addition, this function is generally called by the following functions:
 *           eos_sem_take(),  eos_mutex_take(),
 */
eos_inline eos_err_t _ipc_list_suspend(eos_list_t *list,
                                        ek_task_handle_t task,
                                        eos_u8_t flag)
{
    /* suspend task */
    eos_task_suspend((eos_task_handle_t)task);

    switch (flag)
    {
    case EOS_IPC_FLAG_FIFO:
        eos_list_inseeos_before(list, &(task->tlist));
        break; /* EOS_IPC_FLAG_FIFO */

    case EOS_IPC_FLAG_PRIO:
        {
            struct eos_list_node *n;
            ek_task_handle_t stask;

            /* find a suitable position */
            for (n = list->next; n != list; n = n->next)
            {
                stask = eos_list_entry(n, ek_task_t, tlist);

                /* find out */
                if (task->current_priority < stask->current_priority)
                {
                    /* insert this task before the stask */
                    eos_list_inseeos_before(&(stask->tlist), &(task->tlist));
                    break;
                }
            }

            /*
             * not found a suitable position,
             * append to the end of suspend_task list
             */
            if (n == list)
                eos_list_inseeos_before(list, &(task->tlist));
        }
        break;/* EOS_IPC_FLAG_PRIO */

    default:
        EOS_ASSERT(0);
        break;
    }

    return EOS_EOK;
}

/**
 * @brief    This function will resume a task.
 * @note     This function will resume the first task in the list of a IPC object.
 *           1. remove the task from suspend queue of a IPC object.
 *           2. put the task into system ready queue.
 *           By contrast, the eos_ipc_list_resume_all() function will resume all suspended tasks
 *           in the list of a IPC object.
 * @param    list is a pointer to a suspended task list of the IPC object.
 * @return   Return the operation status. When the return value is EOS_EOK, the function is successfully executed.
 *           When the return value is any other values, it means this operation failed.
 * @warning  This function is generally called by the following functions:
 *              eos_sem_release(),
 *              eos_mutex_release(),
 */
eos_inline eos_err_t _ipc_list_resume(eos_list_t *list)
{
    ek_task_handle_t task;

    /* get task entry */
    task = eos_list_entry(list->next, ek_task_t, tlist);

    /* resume it */
    eos_task_resume((eos_task_handle_t)task);

    return EOS_EOK;
}


/**
 * @brief   This function will resume all suspended tasks in the IPC object list,
 *          including the suspended list of IPC object, and private list of mailbox etc.
 * @note    This function will resume all tasks in the IPC object list.
 *          By contrast, the eos_ipc_list_resume() function will resume a suspended task in the list of a IPC object.
 * @param   list is a pointer to a suspended task list of the IPC object.
 * @return   Return the operation status. When the return value is EOS_EOK, the function is successfully executed.
 *           When the return value is any other values, it means this operation failed.
 */
eos_inline eos_err_t _ipc_list_resume_all(eos_list_t *list)
{
    ek_task_handle_t task;
    register eos_ubase_t temp;

    /* wakeup all suspended tasks */
    while (!eos_list_isempty(list))
    {
        /* disable interrupt */
        temp = eos_hw_interrupt_disable();

        /* get next suspended task */
        task = eos_list_entry(list->next, ek_task_t, tlist);
        /* set error code to EOS_ERROR */
        task->error = EOS_ERROR;

        /*
         * resume task
         * In eos_task_resume function, it will remove current task from
         * suspended list
         */
        eos_task_resume((eos_task_handle_t)task);

        /* enable interrupt */
        eos_hw_interrupt_enable(temp);
    }

    return EOS_EOK;
}

#ifdef EOS_USING_SEMAPHORE
/**
 * @brief    This function will initialize a static semaphore object.
 * @note     For the static semaphore object, its memory space is allocated by the compiler during compiling,
 *           and shall placed on the read-write data segment or on the uninitialized data segment.
 *           By contrast, the eos_sem_create() function will allocate memory space automatically and initialize
 *           the semaphore.
 * @see      eos_sem_create()
 * @param    sem is a pointer to the semaphore to initialize. It is assumed that storage for the semaphore will be
 *           allocated in your application.
 * @param    name is a pointer to the name you would like to give the semaphore.
 * @param    value is the initial value for the semaphore.
 *           If used to share resources, you should initialize the value as the number of available resources.
 *           If used to signal the occurrence of an event, you should initialize the value as 0.
 * @param    flag is the semaphore flag, which determines the queuing way of how multiple tasks wait
 *           when the semaphore is not available.
 *           The semaphore flag can be ONE of the following values:
 *               EOS_IPC_FLAG_PRIO          The pending tasks will queue in order of priority.
 *               EOS_IPC_FLAG_FIFO          The pending tasks will queue in the first-in-first-out method
 *                                         (also known as first-come-first-served (FCFS) scheduling strategy).
 *               NOTE: EOS_IPC_FLAG_FIFO is a non-real-time scheduling mode. It is strongly recommended to
 *               use EOS_IPC_FLAG_PRIO to ensure the task is real-time UNLESS your applications concern about
 *               the first-in-first-out principle, and you clearly understand that all tasks involved in
 *               this semaphore will become non-real-time tasks.
 * @return   Return the operation status. When the return value is EOS_EOK, the initialization is successful.
 *           If the return value is any other values, it represents the initialization failed.
 * @warning  This function can ONLY be called from tasks.
 */
eos_err_t eos_sem_init(eos_sem_handle_t sem_,
                        const char *name,
                        eos_u32_t value,
                        eos_u8_t flag)
{
    ek_sem_handle_t sem = (ek_sem_handle_t)sem_;

    EOS_ASSERT(sem != EOS_NULL);
    EOS_ASSERT(value < 0x10000U);
    EOS_ASSERT((flag == EOS_IPC_FLAG_FIFO) || (flag == EOS_IPC_FLAG_PRIO));

    /* initialize object */
    eos_object_init(&(sem->super.super), EOS_Object_Semaphore, name);

    /* initialize ipc object */
    _ipc_object_init(&(sem->super));

    /* set initial value */
    sem->value = (eos_u16_t)value;

    /* set super */
    sem->super.super.flag = flag;

    return EOS_EOK;
}

/**
 * @brief    This function will detach a static semaphore object.
 * @note     This function is used to detach a static semaphore object which is initialized by eos_sem_init() function.
 *           By contrast, the eos_sem_delete() function will delete a semaphore object.
 *           When the semaphore is successfully detached, it will resume all suspended tasks in the semaphore list.
 * @see      eos_sem_delete()
 * @param    sem is a pointer to a semaphore object to be detached.
 * @return   Return the operation status. When the return value is EOS_EOK, the initialization is successful.
 *           If the return value is any other values, it means that the semaphore detach failed.
 * @warning  This function can ONLY detach a static semaphore initialized by the eos_sem_init() function.
 *           If the semaphore is created by the eos_sem_create() function, you MUST NOT USE this function to detach it,
 *           ONLY USE the eos_sem_delete() function to complete the deletion.
 */
eos_err_t eos_sem_detach(eos_sem_handle_t sem_)
{
    ek_sem_handle_t sem = (ek_sem_handle_t)sem_;

    /* parameter check */
    EOS_ASSERT(sem != EOS_NULL);
    EOS_ASSERT(eos_object_get_type(&sem->super.super) == EOS_Object_Semaphore);
    EOS_ASSERT(eos_object_is_systemobject(&sem->super.super));

    /* wakeup all suspended tasks */
    _ipc_list_resume_all(&(sem->super.suspend_task));

    /* detach semaphore object */
    eos_object_detach(&(sem->super.super));

    return EOS_EOK;
}

/**
 * @brief    This function will take a semaphore, if the semaphore is unavailable, the task shall wait for
 *           the semaphore up to a specified time.
 * @note     When this function is called, the count value of the sem->value will decrease 1 until it is equal to 0.
 *           When the sem->value is 0, it means that the semaphore is unavailable. At this time, it will suspend the
 *           task preparing to take the semaphore.
 *           On the contrary, the eos_sem_release() function will increase the count value of sem->value by 1 each time.
 * @see      eos_sem_trytake()
 * @param    sem is a pointer to a semaphore object.
 * @param    time is a timeout period (unit: an OS tick). If the semaphore is unavailable, the task will wait for
 *           the semaphore up to the amount of time specified by this parameter.
 *           NOTE:
 *           If use Macro EOS_WAITING_FOREVER to set this parameter, which means that when the
 *           message is unavailable in the queue, the task will be waiting forever.
 *           If use macro EOS_WAITING_NO to set this parameter, which means that this
 *           function is non-blocking and will return immediately.
 * @return   Return the operation status. ONLY When the return value is EOS_EOK, the operation is successful.
 *           If the return value is any other values, it means that the semaphore take failed.
 * @warning  This function can ONLY be called in the task context. It MUST NOT BE called in interrupt context.
 */
eos_err_t eos_sem_take(eos_sem_handle_t sem_, eos_s32_t time)
{
    ek_sem_handle_t sem = (ek_sem_handle_t)sem_;
    register eos_base_t temp;
    ek_task_handle_t task;

    /* parameter check */
    EOS_ASSERT(sem != EOS_NULL);
    EOS_ASSERT(eos_object_get_type(&sem->super.super) == EOS_Object_Semaphore);

    /* disable interrupt */
    temp = eos_hw_interrupt_disable();

    if (sem->value > 0)
    {
        /* semaphore is available */
        sem->value --;

        /* enable interrupt */
        eos_hw_interrupt_enable(temp);
    }
    else
    {
        /* no waiting, return with timeout */
        if (time == 0)
        {
            eos_hw_interrupt_enable(temp);

            return EOS_ETIMEOUT;
        }
        else
        {
            /* current context checking */

            /* semaphore is unavailable, push to suspend list */
            /* get current task */
            task = (ek_task_handle_t)eos_task_self();

            /* reset task error number */
            task->error = EOS_EOK;

            /* suspend task */
            _ipc_list_suspend(&(sem->super.suspend_task),
                                task,
                                sem->super.super.flag);

            /* has waiting time, start task timer */
            if (time > 0)
            {

                /* reset the timeout of task timer and start it */
                eos_timer_control((eos_timer_handle_t)&(task->task_timer),
                                 EOS_TIMER_CTRL_SET_TIME,
                                 &time);
                eos_timer_start((eos_timer_handle_t)&(task->task_timer));
            }

            /* enable interrupt */
            eos_hw_interrupt_enable(temp);

            /* do schedule */
            eos_schedule();

            if (task->error != EOS_EOK)
            {
                return task->error;
            }
        }
    }

    return EOS_EOK;
}

/**
 * @brief    This function will try to take a semaphore, if the semaphore is unavailable, the task returns immediately.
 * @note     This function is very similar to the eos_sem_take() function, when the semaphore is not available,
 *           the eos_sem_trytake() function will return immediately without waiting for a timeout.
 *           In other words, eos_sem_trytake(sem) has the same effect as eos_sem_take(sem, 0).
 * @see      eos_sem_take()
 * @param    sem is a pointer to a semaphore object.
 * @return   Return the operation status. ONLY When the return value is EOS_EOK, the operation is successful.
 *           If the return value is any other values, it means that the semaphore take failed.
 */
eos_err_t eos_sem_trytake(eos_sem_handle_t sem)
{
    return eos_sem_take(sem, EOS_WAITING_NO);
}

/**
 * @brief    This function will release a semaphore. If there is task suspended on the semaphore, it will get resumed.
 * @note     If there are tasks suspended on this semaphore, the first task in the list of this semaphore object
 *           will be resumed, and a task scheduling (eos_schedule) will be executed.
 *           If no tasks are suspended on this semaphore, the count value sem->value of this semaphore will increase by 1.
 * @param    sem is a pointer to a semaphore object.
 * @return   Return the operation status. When the return value is EOS_EOK, the operation is successful.
 *           If the return value is any other values, it means that the semaphore release failed.
 */
eos_err_t eos_sem_release(eos_sem_handle_t sem_)
{
    ek_sem_handle_t sem = (ek_sem_handle_t)sem_;

    register eos_base_t temp;
    register eos_bool_t need_schedule;

    /* parameter check */
    EOS_ASSERT(sem != EOS_NULL);
    EOS_ASSERT(eos_object_get_type(&sem->super.super) == EOS_Object_Semaphore);


    need_schedule = EOS_False;

    /* disable interrupt */
    temp = eos_hw_interrupt_disable();

    if (!eos_list_isempty(&sem->super.suspend_task))
    {
        /* resume the suspended task */
        _ipc_list_resume(&(sem->super.suspend_task));
        need_schedule = EOS_True;
    }
    else
    {
        if (sem->value < EOS_SEM_VALUE_MAX)
        {
            sem->value ++; /* increase value */
        }
        else
        {
            eos_hw_interrupt_enable(temp); /* enable interrupt */
            return EOS_EFULL; /* value overflowed */
        }
    }

    /* enable interrupt */
    eos_hw_interrupt_enable(temp);

    /* resume a task, re-schedule */
    if (need_schedule == EOS_True)
    {
        eos_schedule();
    }

    return EOS_EOK;
}

eos_err_t eos_sem_reset(eos_sem_handle_t sem_, eos_ubase_t value)
{
    ek_sem_handle_t sem = (ek_sem_handle_t)sem_;

    EOS_ASSERT(sem != EOS_NULL);
    EOS_ASSERT(eos_object_get_type(&sem->super.super) == EOS_Object_Semaphore);

    eos_ubase_t level;

    /* disable interrupt */
    level = eos_hw_interrupt_disable();

    /* resume all waiting task */
    _ipc_list_resume_all(&sem->super.suspend_task);

    /* set new value */
    sem->value = (eos_u16_t)value;

    /* enable interrupt */
    eos_hw_interrupt_enable(level);

    eos_schedule();

    return EOS_EOK;
}
#endif /* EOS_USING_SEMAPHORE */

#ifdef EOS_USING_MUTEX

/**
 * @brief    Initialize a static mutex object.
 * @note     For the static mutex object, its memory space is allocated by the compiler during compiling,
 *           and shall placed on the read-write data segment or on the uninitialized data segment.
 *           By contrast, the eos_mutex_create() function will automatically allocate memory space
 *           and initialize the mutex.
 * @see      eos_mutex_create()
 * @param    mutex is a pointer to the mutex to initialize. It is assumed that storage for the mutex will be
 *           allocated in your application.
 * @param    name is a pointer to the name that given to the mutex.
 * @param    flag is the mutex flag, which determines the queuing way of how multiple tasks wait
 *           when the mutex is not available.
 *           NOTE: This parameter has been obsoleted. It can be EOS_IPC_FLAG_PRIO, EOS_IPC_FLAG_FIFO or EOS_NULL.
 * @return   Return the operation status. When the return value is EOS_EOK, the initialization is successful.
 *           If the return value is any other values, it represents the initialization failed.
 * @warning  This function can ONLY be called from tasks.
 */
eos_err_t eos_mutex_init(eos_mutex_handle_t mutex_, const char *name, eos_u8_t flag)
{
    ek_mutex_handle_t mutex = (ek_mutex_handle_t)mutex_;

    /* flag parameter has been obsoleted */
    EOS_UNUSED(flag);

    /* parameter check */
    EOS_ASSERT(mutex != EOS_NULL);

    /* initialize object */
    eos_object_init(&(mutex->super.super), EOS_Object_Mutex, name);

    /* initialize ipc object */
    _ipc_object_init(&(mutex->super));

    mutex->value = 1;
    mutex->owner = EOS_NULL;
    mutex->prio_bkp = 0xFF;
    mutex->hold  = 0;

    /* flag can only be EOS_IPC_FLAG_PRIO. EOS_IPC_FLAG_FIFO cannot solve the unbounded priority inversion problem */
    mutex->super.super.flag = EOS_IPC_FLAG_PRIO;

    return EOS_EOK;
}

/**
 * @brief    This function will detach a static mutex object.
 * @note     This function is used to detach a static mutex object which is initialized by eos_mutex_init() function.
 *           By contrast, the eos_mutex_delete() function will delete a mutex object.
 *           When the mutex is successfully detached, it will resume all suspended tasks in the mutex list.
 * @see      eos_mutex_delete()
 * @param    mutex is a pointer to a mutex object to be detached.
 * @return   Return the operation status. When the return value is EOS_EOK, the initialization is successful.
 *           If the return value is any other values, it means that the mutex detach failed.
 * @warning  This function can ONLY detach a static mutex initialized by the eos_mutex_init() function.
 *           If the mutex is created by the eos_mutex_create() function, you MUST NOT USE this function to detach it,
 *           ONLY USE the eos_mutex_delete() function to complete the deletion.
 */
eos_err_t eos_mutex_detach(eos_mutex_handle_t mutex_)
{
    ek_mutex_handle_t mutex = (ek_mutex_handle_t)mutex_;

    /* parameter check */
    EOS_ASSERT(mutex != EOS_NULL);
    EOS_ASSERT(eos_object_get_type(&mutex->super.super) == EOS_Object_Mutex);
    EOS_ASSERT(eos_object_is_systemobject(&mutex->super.super));

    /* wakeup all suspended tasks */
    _ipc_list_resume_all(&(mutex->super.suspend_task));

    /* detach mutex object */
    eos_object_detach(&(mutex->super.super));

    return EOS_EOK;
}

/**
 * @brief    This function will take a mutex, if the mutex is unavailable, the task shall wait for
 *           the mutex up to a specified time.
 * @note     When this function is called, the count value of the mutex->value will decrease 1 until it is equal to 0.
 *           When the mutex->value is 0, it means that the mutex is unavailable. At this time, it will suspend the
 *           task preparing to take the mutex.
 *           On the contrary, the eos_mutex_release() function will increase the count value of mutex->value by 1 each time.
 * @see      eos_mutex_trytake()
 * @param    mutex is a pointer to a mutex object.
 * @param    time is a timeout period (unit: an OS tick). If the mutex is unavailable, the task will wait for
 *           the mutex up to the amount of time specified by the argument.
 *           NOTE: Generally, we set this parameter to EOS_WAITING_FOREVER, which means that when the mutex is unavailable,
 *           the task will be waitting forever.
 * @return   Return the operation status. ONLY When the return value is EOS_EOK, the operation is successful.
 *           If the return value is any other values, it means that the mutex take failed.
 * @warning  This function can ONLY be called in the task context. It MUST NOT BE called in interrupt context.
 */
eos_err_t eos_mutex_take(eos_mutex_handle_t mutex_, eos_s32_t time)
{
    register eos_base_t temp;
    ek_task_handle_t task;
    ek_mutex_handle_t mutex = (ek_mutex_handle_t)mutex_;

    /* this function must not be used in interrupt even if time = 0 */
    /* current context checking */

    /* parameter check */
    EOS_ASSERT(mutex != EOS_NULL);
    EOS_ASSERT(eos_object_get_type(&mutex->super.super) == EOS_Object_Mutex);

    /* get current task */
    task = (ek_task_handle_t)eos_task_self();

    /* disable interrupt */
    temp = eos_hw_interrupt_disable();

    /* reset task error */
    task->error = EOS_EOK;

    if (mutex->owner == task)
    {
        if (mutex->hold < EOS_MUTEX_HOLD_MAX)
        {
            /* it's the same task */
            mutex->hold ++;
        }
        else
        {
            eos_hw_interrupt_enable(temp); /* enable interrupt */
            return EOS_EFULL; /* value overflowed */
        }
    }
    else
    {
        /* The value of mutex is 1 in initial status. Therefore, if the
         * value is great than 0, it indicates the mutex is avaible.
         */
        if (mutex->value > 0)
        {
            /* mutex is available */
            mutex->value --;

            /* set mutex owner and original priority */
            mutex->owner = task;
            mutex->prio_bkp = task->current_priority;
            if (mutex->hold < EOS_MUTEX_HOLD_MAX)
            {
                mutex->hold ++;
            }
            else
            {
                eos_hw_interrupt_enable(temp); /* enable interrupt */
                return EOS_EFULL; /* value overflowed */
            }
        }
        else
        {
            /* no waiting, return with timeout */
            if (time == 0)
            {
                /* set error as timeout */
                task->error = EOS_ETIMEOUT;

                /* enable interrupt */
                eos_hw_interrupt_enable(temp);

                return EOS_ETIMEOUT;
            }
            else
            {
                /* mutex is unavailable, push to suspend list */

                /* change the owner task priority of mutex */
                if (task->current_priority < mutex->owner->current_priority)
                {
                    /* change the owner task priority */
                    eos_task_control((eos_task_handle_t)(mutex->owner),
                                      EOS_TASK_CTRL_CHANGE_PRIORITY,
                                      &task->current_priority);
                }

                /* suspend current task */
                _ipc_list_suspend(&(mutex->super.suspend_task),
                                    task,
                                    mutex->super.super.flag);

                /* has waiting time, start task timer */
                if (time > 0)
                {
                    /* reset the timeout of task timer and start it */
                    eos_timer_control((eos_timer_handle_t)&(task->task_timer),
                                        EOS_TIMER_CTRL_SET_TIME,
                                        &time);
                    eos_timer_start((eos_timer_handle_t)&(task->task_timer));
                }

                /* enable interrupt */
                eos_hw_interrupt_enable(temp);

                /* do schedule */
                eos_schedule();

                if (task->error != EOS_EOK)
                {
                    /* return error */
                    return task->error;
                }
                else
                {
                    /* the mutex is taken successfully. */
                    /* disable interrupt */
                    temp = eos_hw_interrupt_disable();
                }
            }
        }
    }

    /* enable interrupt */
    eos_hw_interrupt_enable(temp);


    return EOS_EOK;
}

/**
 * @brief    This function will try to take a mutex, if the mutex is unavailable, the task returns immediately.
 * @note     This function is very similar to the eos_mutex_take() function, when the mutex is not available,
 *           except that eos_mutex_trytake() will return immediately without waiting for a timeout
 *           when the mutex is not available.
 *           In other words, eos_mutex_trytake(mutex) has the same effect as eos_mutex_take(mutex, 0).
 * @see      eos_mutex_take()
 * @param    mutex is a pointer to a mutex object.
 * @return   Return the operation status. ONLY When the return value is EOS_EOK, the operation is successful.
 *           If the return value is any other values, it means that the mutex take failed.
 */
eos_err_t eos_mutex_trytake(eos_mutex_handle_t mutex)
{
    return eos_mutex_take(mutex, EOS_WAITING_NO);
}

/**
 * @brief    This function will release a mutex. If there is task suspended on the mutex, the task will be resumed.
 * @note     If there are tasks suspended on this mutex, the first task in the list of this mutex object
 *           will be resumed, and a task scheduling (eos_schedule) will be executed.
 *           If no tasks are suspended on this mutex, the count value mutex->value of this mutex will increase by 1.
 * @param    mutex is a pointer to a mutex object.
 * @return   Return the operation status. When the return value is EOS_EOK, the operation is successful.
 *           If the return value is any other values, it means that the mutex release failed.
 */
eos_err_t eos_mutex_release(eos_mutex_handle_t mutex_)
{
    ek_mutex_handle_t mutex = (ek_mutex_handle_t)mutex_;

    register eos_base_t temp;
    ek_task_handle_t task;
    eos_bool_t need_schedule;

    /* parameter check */
    EOS_ASSERT(mutex != EOS_NULL);
    EOS_ASSERT(eos_object_get_type(&mutex->super.super) == EOS_Object_Mutex);

    need_schedule = EOS_False;

    /* get current task */
    task = (ek_task_handle_t)eos_task_self();

    /* disable interrupt */
    temp = eos_hw_interrupt_disable();

    /* mutex only can be released by owner */
    if (task != mutex->owner)
    {
        task->error = EOS_ERROR;

        /* enable interrupt */
        eos_hw_interrupt_enable(temp);

        return EOS_ERROR;
    }

    /* decrease hold */
    mutex->hold --;
    /* if no hold */
    if (mutex->hold == 0)
    {
        /* change the owner task to original priority */
        if (mutex->prio_bkp != mutex->owner->current_priority)
        {
            eos_task_control((eos_task_handle_t)(mutex->owner),
                              EOS_TASK_CTRL_CHANGE_PRIORITY,
                              &(mutex->prio_bkp));
        }

        /* wakeup suspended task */
        if (!eos_list_isempty(&mutex->super.suspend_task))
        {
            /* get suspended task */
            task = eos_list_entry(mutex->super.suspend_task.next,
                                   ek_task_t,
                                   tlist);

            /* set new owner and priority */
            mutex->owner = (ek_task_handle_t)task;
            mutex->prio_bkp = task->current_priority;

            if (mutex->hold < EOS_MUTEX_HOLD_MAX)
            {
                mutex->hold ++;
            }
            else
            {
                eos_hw_interrupt_enable(temp); /* enable interrupt */
                return EOS_EFULL; /* value overflowed */
            }

            /* resume task */
            _ipc_list_resume(&(mutex->super.suspend_task));

            need_schedule = EOS_True;
        }
        else
        {
            if (mutex->value < EOS_MUTEX_VALUE_MAX)
            {
                /* increase value */
                mutex->value ++;
            }
            else
            {
                eos_hw_interrupt_enable(temp); /* enable interrupt */
                return EOS_EFULL; /* value overflowed */
            }

            /* clear owner */
            mutex->owner             = EOS_NULL;
            mutex->prio_bkp = 0xff;
        }
    }

    /* enable interrupt */
    eos_hw_interrupt_enable(temp);

    /* perform a schedule */
    if (need_schedule == EOS_True)
        eos_schedule();

    return EOS_EOK;
}
#endif /* EOS_USING_MUTEX */

#ifndef EOS_USING_IDLE_HOOK
#endif /* EOS_USING_IDLE_HOOK */

#ifndef IDLE_THREAD_STACK_SIZE
#if defined (EOS_USING_IDLE_HOOK) || defined(EOS_USING_HEAP)
#define IDLE_THREAD_STACK_SIZE  256
#else
#define IDLE_THREAD_STACK_SIZE  128
#endif /* (EOS_USING_IDLE_HOOK) || defined(EOS_USING_HEAP) */
#endif /* IDLE_THREAD_STACK_SIZE */

#define _CPUS_NR                1

static eos_list_t _eos_task_defunct = EOS_LIST_OBJECT_INIT(_eos_task_defunct);

static ek_task_t idle[_CPUS_NR];
static eos_u32_t eos_task_stack[_CPUS_NR][IDLE_THREAD_STACK_SIZE / 4];

#ifndef SYSTEM_THREAD_STACK_SIZE
#endif

#ifdef EOS_USING_IDLE_HOOK
#ifndef EOS_IDLE_HOOK_LIST_SIZE
#define EOS_IDLE_HOOK_LIST_SIZE  4
#endif /* EOS_IDLE_HOOK_LIST_SIZE */

static void (*idle_hook_list[EOS_IDLE_HOOK_LIST_SIZE])(void);

/**
 * @brief This function sets a hook function to idle task loop. When the system performs
 *        idle loop, this hook function should be invoked.
 * @param hook the specified hook function.
 * @return EOS_EOK: set OK.
 *         EOS_EFULL: hook list is full.
 * @note the hook function must be simple and never be blocked or suspend.
 */
eos_err_t eos_task_idle_sethook(void (*hook)(void))
{
    eos_base_t level;
    eos_err_t ret = EOS_EFULL;

    /* disable interrupt */
    level = eos_hw_interrupt_disable();

    for (eos_u32_t i = 0; i < EOS_IDLE_HOOK_LIST_SIZE; i++)
    {
        if (idle_hook_list[i] == EOS_NULL)
        {
            idle_hook_list[i] = hook;
            ret = EOS_EOK;
            break;
        }
    }
    /* enable interrupt */
    eos_hw_interrupt_enable(level);

    return ret;
}

#endif /* EOS_USING_IDLE_HOOK */

/**
 * @brief Enqueue a task to defunct queue.
 * @note It must be called between eos_hw_interrupt_disable and eos_hw_interrupt_enable
 */
void eos_task_defunct_enqueue(eos_task_handle_t task)
{
    ek_task_handle_t task_ = (ek_task_handle_t)task;
    eos_list_inseeos_after(&_eos_task_defunct, &task_->tlist);
}

/**
 * @brief Dequeue a task from defunct queue.
 */
eos_task_handle_t eos_task_defunct_dequeue(void)
{
    register eos_base_t lock;
    ek_task_handle_t task = EOS_NULL;
    eos_list_t *l = &_eos_task_defunct;

    if (l->next != l)
    {
        task = eos_list_entry(l->next,
                ek_task_t,
                tlist);
        lock = eos_hw_interrupt_disable();
        eos_list_remove(&(task->tlist));
        eos_hw_interrupt_enable(lock);
    }

    return (eos_task_handle_t)task;
}

/**
 * @brief This function will perform system background job when system idle.
 */
static void eos_defunct_execute(void)
{
    /* Loop until there is no dead task. So one call to eos_defunct_execute
     * will do all the cleanups. */
    while (1)
    {
        ek_task_handle_t task;
        void (*cleanup)(ek_task_handle_t tid);

        /* get defunct task */
        task = (ek_task_handle_t)eos_task_defunct_dequeue();
        if (task == EOS_NULL)
        {
            break;
        }
        /* invoke task cleanup */
        cleanup = task->cleanup;
        if (cleanup != EOS_NULL)
        {
            cleanup(task);
        }

        /* if it's a system object, not delete it */
        if (eos_object_is_systemobject((eos_obj_handle_t)task) == EOS_True)
        {
            /* detach this object */
            eos_object_detach((eos_obj_handle_t)task);
        }
        else
        {
        }
    }
}

static void eos_task_idle_entry(void *parameter)
{
    while (1)
    {
#ifdef EOS_USING_IDLE_HOOK
        void (*idle_hook)(void);

        for (eos_u32_t i = 0; i < EOS_IDLE_HOOK_LIST_SIZE; i++)
        {
            idle_hook = idle_hook_list[i];
            if (idle_hook != EOS_NULL)
            {
                idle_hook();
            }
        }
#endif /* EOS_USING_IDLE_HOOK */

#ifndef EOS_USING_SMP
        eos_defunct_execute();
#endif /* EOS_USING_SMP */

    }
}

/**
 * @brief This function will initialize idle task, then start it.
 * @note this function must be invoked when system init.
 */
void eos_task_idle_init(void)
{
    for (eos_u32_t i = 0; i < _CPUS_NR; i++)
    {
        ek_task_init(&idle[i],
                        "___t_idle",
                        eos_task_idle_entry,
                        EOS_NULL,
                        &eos_task_stack[i][0],
                        sizeof(eos_task_stack[i]),
                        EOS_TASK_PRIORITY_MAX - 1,
                        32);
        /* startup */
        eos_task_startup((eos_task_handle_t)&idle[i]);
    }

}

/* hard timer list */
static eos_list_t _timer_list[EOS_TIMER_SKIP_LIST_LEVEL];

#ifdef EOS_USING_TIMER_SOFT

#define EOS_SOFT_TIMER_IDLE              1
#define EOS_SOFT_TIMER_BUSY              0

#ifndef EOS_TIMER_THREAD_STACK_SIZE
#define EOS_TIMER_THREAD_STACK_SIZE     512
#endif /* EOS_TIMER_THREAD_STACK_SIZE */

#ifndef EOS_TIMER_THREAD_PRIO
#define EOS_TIMER_THREAD_PRIO           0
#endif /* EOS_TIMER_THREAD_PRIO */

/* soft timer status */
static eos_u8_t _soft_timer_status = EOS_SOFT_TIMER_IDLE;
/* soft timer list */
static eos_list_t _soft_timer_list[EOS_TIMER_SKIP_LIST_LEVEL];
static ek_task_t _timer_task;
static eos_u32_t _timer_task_stack[EOS_TIMER_THREAD_STACK_SIZE / 4];
#endif /* EOS_USING_TIMER_SOFT */

/**
 * @brief [internal] The init funtion of timer
 *        The internal called function of eos_timer_init
 * @see eos_timer_init
 * @param timer is timer object
 * @param timeout is the timeout function
 * @param parameter is the parameter of timeout function
 * @param time is the tick of timer
 * @param flag the flag of timer
 */
static void _timer_init(ek_timer_handle_t timer,
                           void (*timeout)(void *parameter),
                           void *parameter,
                           eos_u32_t time,
                           eos_u8_t flag)
{
    /* set flag */
    timer->super.flag  = flag;

    /* set deactivated */
    timer->super.flag &= ~EOS_TIMER_FLAG_ACTIVATED;

    timer->timeout_func = timeout;
    timer->parameter    = parameter;

    timer->timeout_tick = 0;
    timer->init_tick    = time;

    /* initialize timer list */
    for (eos_u32_t i = 0; i < EOS_TIMER_SKIP_LIST_LEVEL; i++)
    {
        eos_list_init(&(timer->row[i]));
    }
}


/**
 * @brief  Find the next emtpy timer ticks
 * @param timer_list is the array of time list
 * @param timeout_tick is the next timer's ticks
 * @return  Return the operation status. If the return value is EOS_EOK, the function is successfully executed.
 *          If the return value is any other values, it means this operation failed.
 */
static eos_err_t _timer_list_next_timeout(eos_list_t timer_list[], eos_u32_t *timeout_tick)
{
    ek_timer_handle_t timer;
    register eos_base_t level;

    /* disable interrupt */
    level = eos_hw_interrupt_disable();

    if (!eos_list_isempty(&timer_list[EOS_TIMER_SKIP_LIST_LEVEL - 1]))
    {
        timer = eos_list_entry(timer_list[EOS_TIMER_SKIP_LIST_LEVEL - 1].next,
                                ek_timer_t,
                                row[EOS_TIMER_SKIP_LIST_LEVEL - 1]);
        *timeout_tick = timer->timeout_tick;

        /* enable interrupt */
        eos_hw_interrupt_enable(level);

        return EOS_EOK;
    }

    /* enable interrupt */
    eos_hw_interrupt_enable(level);

    return EOS_ERROR;
}

/**
 * @brief Remove the timer
 * @param timer the point of the timer
 */
eos_inline void _timer_remove(eos_timer_handle_t timer_)
{
    ek_timer_handle_t timer = (ek_timer_handle_t)timer_;

    for (eos_u32_t i = 0; i < EOS_TIMER_SKIP_LIST_LEVEL; i++)
    {
        eos_list_remove(&timer->row[i]);
    }
}

#if EOS_DEBUG_TIMER
/**
 * @brief The number of timer
 * @param timer the head of timer
 * @return count of timer
 */
static int _timer_count_height(eos_timer_handle_t timer)
{
    int count = 0;

    for (eos_u32_t i = 0; i < EOS_TIMER_SKIP_LIST_LEVEL; i++)
    {
        if (!eos_list_isempty(&timer->row[i]))
        {
            count ++;
        }
    }
    return count;
}
/**
 * @brief dump the all timer information
 * @param timer_heads the head of timer
 */
void eos_timer_dump(eos_list_t timer_heads[])
{
    eos_list_t *list;

    for (list = timer_heads[EOS_TIMER_SKIP_LIST_LEVEL - 1].next;
         list != &timer_heads[EOS_TIMER_SKIP_LIST_LEVEL - 1];
         list = list->next)
    {
        eos_timer_handle_t timer = eos_list_entry(list,
                                               ek_timer_t,
                                               row[EOS_TIMER_SKIP_LIST_LEVEL - 1]);
        eos_kprintf("%d", _timer_count_height(timer));
    }
    eos_kprintf("\n");
}
#endif /* EOS_DEBUG_TIMER */

/**
 * @brief This function will initialize a timer
 *        normally this function is used to initialize a static timer object.
 * @param timer is the point of timer
 * @param name is a pointer to the name of the timer
 * @param timeout is the callback of timer
 * @param parameter is the param of the callback
 * @param time is timeout ticks of timer
 *             NOTE: The max timeout tick should be no more than (EOS_TICK_MAX/2 - 1).
 * @param flag is the flag of timer
 */
void eos_timer_init(eos_timer_handle_t timer_,
                   const char *name,
                   void (*timeout)(void *parameter),
                   void *parameter,
                   eos_u32_t time,
                   eos_u8_t flag)
{
    ek_timer_handle_t timer = (ek_timer_handle_t)timer_;

    /* parameter check */
    EOS_ASSERT(timer != EOS_NULL);
    EOS_ASSERT(timeout != EOS_NULL);
    EOS_ASSERT(time < EOS_TICK_MAX / 2);

    /* timer object initialization */
    eos_object_init(&(timer->super), EOS_Object_Timer, name);

    _timer_init(timer, timeout, parameter, time, flag);
}

/**
 * @brief This function will detach a timer from timer management.
 * @param timer is the timer to be detached
 * @return the status of detach
 */
eos_err_t eos_timer_detach(eos_timer_handle_t timer_)
{
    register eos_base_t level;
    ek_timer_handle_t timer = (ek_timer_handle_t)timer_;

    /* parameter check */
    EOS_ASSERT(timer != EOS_NULL);
    EOS_ASSERT(eos_object_get_type(&timer->super) == EOS_Object_Timer);
    EOS_ASSERT(eos_object_is_systemobject(&timer->super));

    /* disable interrupt */
    level = eos_hw_interrupt_disable();

    _timer_remove(timer_);
    /* stop timer */
    timer->super.flag &= ~EOS_TIMER_FLAG_ACTIVATED;

    /* enable interrupt */
    eos_hw_interrupt_enable(level);

    eos_object_detach(&(timer->super));

    return EOS_EOK;
}

/**
 * @brief This function will start the timer
 * @param timer the timer to be started
 * @return the operation status, EOS_EOK on OK, EOS_ERROR on error
 */
eos_err_t eos_timer_start(eos_timer_handle_t timer_)
{
    unsigned int row_lvl;
    eos_list_t *timer_list;
    register eos_base_t level;
    register eos_bool_t need_schedule;
    eos_list_t *row_head[EOS_TIMER_SKIP_LIST_LEVEL];
    unsigned int tst_nr;
    static unsigned int random_nr;
    ek_timer_handle_t timer = (ek_timer_handle_t)timer_;

    /* parameter check */
    EOS_ASSERT(timer != EOS_NULL);
    EOS_ASSERT(eos_object_get_type(&timer->super) == EOS_Object_Timer);

    need_schedule = EOS_False;

    /* stop timer firstly */
    level = eos_hw_interrupt_disable();
    /* remove timer from list */
    _timer_remove(timer_);
    /* change status of timer */
    timer->super.flag &= ~EOS_TIMER_FLAG_ACTIVATED;

    timer->timeout_tick = eos_tick_get() + timer->init_tick;

#ifdef EOS_USING_TIMER_SOFT
    if (timer->super.flag & EOS_TIMER_FLAG_SOFT_TIMER)
    {
        /* insert timer to soft timer list */
        timer_list = _soft_timer_list;
    }
    else
#endif /* EOS_USING_TIMER_SOFT */
    {
        /* insert timer to system timer list */
        timer_list = _timer_list;
    }

    row_head[0]  = &timer_list[0];
    for (row_lvl = 0; row_lvl < EOS_TIMER_SKIP_LIST_LEVEL; row_lvl++)
    {
        for (; row_head[row_lvl] != timer_list[row_lvl].prev;
             row_head[row_lvl]  = row_head[row_lvl]->next)
        {
            ek_timer_handle_t t;
            eos_list_t *p = row_head[row_lvl]->next;

            /* fix up the entry pointer */
            t = eos_list_entry(p, ek_timer_t, row[row_lvl]);

            /* If we have two timers that timeout at the same time, it's
             * preferred that the timer inserted early get called early.
             * So insert the new timer to the end the the some-timeout timer
             * list.
             */
            if ((t->timeout_tick - timer->timeout_tick) == 0)
            {
                continue;
            }
            else if ((t->timeout_tick - timer->timeout_tick) < EOS_TICK_MAX / 2)
            {
                break;
            }
        }
        if (row_lvl != EOS_TIMER_SKIP_LIST_LEVEL - 1)
        {
            row_head[row_lvl + 1] = row_head[row_lvl] + 1;
        }
    }

    /* Interestingly, this super simple timer insert counter works very very
     * well on distributing the list height uniformly. By means of "very very
     * well", I mean it beats the randomness of timer->timeout_tick very easily
     * (actually, the timeout_tick is not random and easy to be attacked). */
    random_nr++;
    tst_nr = random_nr;

    eos_list_inseeos_after(row_head[EOS_TIMER_SKIP_LIST_LEVEL - 1],
                         &(timer->row[EOS_TIMER_SKIP_LIST_LEVEL - 1]));
    for (row_lvl = 2; row_lvl <= EOS_TIMER_SKIP_LIST_LEVEL; row_lvl++)
    {
        if (!(tst_nr & EOS_TIMER_SKIP_LIST_MASK))
            eos_list_inseeos_after(row_head[EOS_TIMER_SKIP_LIST_LEVEL - row_lvl],
                                 &(timer->row[EOS_TIMER_SKIP_LIST_LEVEL - row_lvl]));
        else
            break;
        /* Shift over the bits we have tested. Works well with 1 bit and 2
         * bits. */
        tst_nr >>= (EOS_TIMER_SKIP_LIST_MASK + 1) >> 1;
    }

    timer->super.flag |= EOS_TIMER_FLAG_ACTIVATED;

#ifdef EOS_USING_TIMER_SOFT
    if (timer->super.flag & EOS_TIMER_FLAG_SOFT_TIMER)
    {
        /* check whether timer task is ready */
        if ((_soft_timer_status == EOS_SOFT_TIMER_IDLE) &&
           ((_timer_task.status & EOS_TASK_STAT_MASK) == EOS_TASK_SUSPEND))
        {
            /* resume timer task to check soft timer */
            eos_task_resume((eos_task_handle_t)&_timer_task);
            need_schedule = EOS_True;
        }
    }
#endif /* EOS_USING_TIMER_SOFT */

    /* enable interrupt */
    eos_hw_interrupt_enable(level);

    if (need_schedule)
    {
        eos_schedule();
    }

    return EOS_EOK;
}

/**
 * @brief This function will stop the timer
 * @param timer the timer to be stopped
 * @return the operation status, EOS_EOK on OK, EOS_ERROR on error
 */
eos_err_t eos_timer_stop(eos_timer_handle_t timer_)
{
    register eos_base_t level;
    ek_timer_handle_t timer = (ek_timer_handle_t)timer_;

    /* parameter check */
    EOS_ASSERT(timer != EOS_NULL);
    EOS_ASSERT(eos_object_get_type(&timer->super) == EOS_Object_Timer);

    if (!(timer->super.flag & EOS_TIMER_FLAG_ACTIVATED))
        return EOS_ERROR;

    /* disable interrupt */
    level = eos_hw_interrupt_disable();

    _timer_remove(timer_);
    /* change status */
    timer->super.flag &= ~EOS_TIMER_FLAG_ACTIVATED;

    /* enable interrupt */
    eos_hw_interrupt_enable(level);

    return EOS_EOK;
}

/**
 * @brief This function will get or set some options of the timer
 * @param timer the timer to be get or set
 * @param cmd the control command
 * @param arg the argument
 * @return the statu of control
 */
eos_err_t eos_timer_control(eos_timer_handle_t timer_, int cmd, void *arg)
{
    register eos_base_t level;
    ek_timer_handle_t timer = (ek_timer_handle_t)timer_;

    /* parameter check */
    EOS_ASSERT(timer != EOS_NULL);
    EOS_ASSERT(eos_object_get_type(&timer->super) == EOS_Object_Timer);

    level = eos_hw_interrupt_disable();
    switch (cmd)
    {
    case EOS_TIMER_CTRL_GET_TIME:
        *(eos_u32_t *)arg = timer->init_tick;
        break;

    case EOS_TIMER_CTRL_SET_TIME:
        EOS_ASSERT((*(eos_u32_t *)arg) < EOS_TICK_MAX / 2);
        timer->init_tick = *(eos_u32_t *)arg;
        break;

    case EOS_TIMER_CTRL_SET_ONESHOT:
        timer->super.flag &= ~EOS_TIMER_FLAG_PERIODIC;
        break;

    case EOS_TIMER_CTRL_SET_PERIODIC:
        timer->super.flag |= EOS_TIMER_FLAG_PERIODIC;
        break;

    case EOS_TIMER_CTRL_GET_STATE:
        if (timer->super.flag & EOS_TIMER_FLAG_ACTIVATED)
        {
            /*timer is start and run*/
            *(eos_u32_t *)arg = EOS_TIMER_FLAG_ACTIVATED;
        }
        else
        {
            /*timer is stop*/
            *(eos_u32_t *)arg = EOS_TIMER_FLAG_DEACTIVATED;
        }
    case EOS_TIMER_CTRL_GET_REMAIN_TIME:
        *(eos_u32_t *)arg =  timer->timeout_tick;
        break;

    default:
        break;
    }
    eos_hw_interrupt_enable(level);

    return EOS_EOK;
}

/**
 * @brief This function will check timer list, if a timeout event happens,
 *        the corresponding timeout function will be invoked.
 * @note This function shall be invoked in operating system timer interrupt.
 */
void eos_timer_check(void)
{
    ek_timer_handle_t t;
    eos_u32_t current_tick;
    register eos_base_t level;
    eos_list_t list;

    eos_list_init(&list);

    current_tick = eos_tick_get();

    /* disable interrupt */
    level = eos_hw_interrupt_disable();

    while (!eos_list_isempty(&_timer_list[EOS_TIMER_SKIP_LIST_LEVEL - 1]))
    {
        t = eos_list_entry(_timer_list[EOS_TIMER_SKIP_LIST_LEVEL - 1].next,
                          ek_timer_t, row[EOS_TIMER_SKIP_LIST_LEVEL - 1]);

        /*
         * It supposes that the new tick shall less than the half duration of
         * tick max.
         */
        if ((current_tick - t->timeout_tick) < EOS_TICK_MAX / 2)
        {
            /* remove timer from timer list firstly */
            _timer_remove((eos_timer_handle_t)t);
            if (!(t->super.flag & EOS_TIMER_FLAG_PERIODIC))
            {
                t->super.flag &= ~EOS_TIMER_FLAG_ACTIVATED;
            }
            /* add timer to temporary list  */
            eos_list_inseeos_after(&list, &(t->row[EOS_TIMER_SKIP_LIST_LEVEL - 1]));
            /* call timeout function */
            t->timeout_func(t->parameter);

            /* re-get tick */
            current_tick = eos_tick_get();

            /* Check whether the timer object is detached or started again */
            if (eos_list_isempty(&list))
            {
                continue;
            }
            eos_list_remove(&(t->row[EOS_TIMER_SKIP_LIST_LEVEL - 1]));
            if ((t->super.flag & EOS_TIMER_FLAG_PERIODIC) &&
                (t->super.flag & EOS_TIMER_FLAG_ACTIVATED))
            {
                /* start it */
                t->super.flag &= ~EOS_TIMER_FLAG_ACTIVATED;
                eos_timer_start((eos_timer_handle_t)t);
            }
        }
        else
        {
            break;
        }
    }

    /* enable interrupt */
    eos_hw_interrupt_enable(level);
}

/**
 * @brief This function will return the next timeout tick in the system.
 * @return the next timeout tick in the system
 */
eos_u32_t eos_timer_next_timeout_tick(void)
{
    eos_u32_t next_timeout = EOS_TICK_MAX;
    _timer_list_next_timeout(_timer_list, &next_timeout);
    return next_timeout;
}

#ifdef EOS_USING_TIMER_SOFT
/**
 * @brief This function will check software-timer list, if a timeout event happens, the
 *        corresponding timeout function will be invoked.
 */
void eos_soft_timer_check(void)
{
    eos_u32_t current_tick;
    ek_timer_handle_t t;
    register eos_base_t level;
    eos_list_t list;

    eos_list_init(&list);

    /* disable interrupt */
    level = eos_hw_interrupt_disable();

    while (!eos_list_isempty(&_soft_timer_list[EOS_TIMER_SKIP_LIST_LEVEL - 1]))
    {
        t = eos_list_entry(_soft_timer_list[EOS_TIMER_SKIP_LIST_LEVEL - 1].next,
                            ek_timer_t, row[EOS_TIMER_SKIP_LIST_LEVEL - 1]);

        current_tick = eos_tick_get();

        /*
         * It supposes that the new tick shall less than the half duration of
         * tick max.
         */
        if ((current_tick - t->timeout_tick) < EOS_TICK_MAX / 2)
        {
            /* remove timer from timer list firstly */
            _timer_remove((eos_timer_handle_t)t);
            if (!(t->super.flag & EOS_TIMER_FLAG_PERIODIC))
            {
                t->super.flag &= ~EOS_TIMER_FLAG_ACTIVATED;
            }
            /* add timer to temporary list  */
            eos_list_inseeos_after(&list, &(t->row[EOS_TIMER_SKIP_LIST_LEVEL - 1]));

            _soft_timer_status = EOS_SOFT_TIMER_BUSY;
            /* enable interrupt */
            eos_hw_interrupt_enable(level);

            /* call timeout function */
            t->timeout_func(t->parameter);

            /* disable interrupt */
            level = eos_hw_interrupt_disable();

            _soft_timer_status = EOS_SOFT_TIMER_IDLE;
            /* Check whether the timer object is detached or started again */
            if (eos_list_isempty(&list))
            {
                continue;
            }
            eos_list_remove(&(t->row[EOS_TIMER_SKIP_LIST_LEVEL - 1]));
            if ((t->super.flag & EOS_TIMER_FLAG_PERIODIC) &&
                (t->super.flag & EOS_TIMER_FLAG_ACTIVATED))
            {
                /* start it */
                t->super.flag &= ~EOS_TIMER_FLAG_ACTIVATED;
                eos_timer_start((eos_timer_handle_t)t);
            }
        }
        else
        {
            break; /* not check anymore */
        }
    }
    /* enable interrupt */
    eos_hw_interrupt_enable(level);
}

/**
 * @brief System timer task entry
 * @param parameter is the arg of the task
 */
static void _timer_task_entry(void *parameter)
{
    eos_u32_t next_timeout;

    while (1)
    {
        /* get the next timeout tick */
        if (_timer_list_next_timeout(_soft_timer_list, &next_timeout) != EOS_EOK)
        {
            /* no software timer exist, suspend self. */
            eos_task_suspend(eos_task_self());
            eos_schedule();
        }
        else
        {
            eos_u32_t current_tick;

            /* get current tick */
            current_tick = eos_tick_get();

            if ((next_timeout - current_tick) < EOS_TICK_MAX / 2)
            {
                /* get the delta timeout tick */
                next_timeout = next_timeout - current_tick;
                eos_task_delay(next_timeout);
            }
        }

        /* check software timer */
        eos_soft_timer_check();
    }
}
#endif /* EOS_USING_TIMER_SOFT */

/**
 * @ingroup SystemInit
 * @brief This function will initialize system timer
 */
void eos_system_timer_init(void)
{
    for (eos_u32_t i = 0; i < sizeof(_timer_list) / sizeof(_timer_list[0]); i++)
    {
        eos_list_init(_timer_list + i);
    }
}

/**
 * @ingroup SystemInit
 * @brief This function will initialize system timer task
 */
void eos_system_timer_task_init(void)
{
#ifdef EOS_USING_TIMER_SOFT
    for (eos_u32_t i = 0;
         i < sizeof(_soft_timer_list) / sizeof(_soft_timer_list[0]);
         i++)
    {
        eos_list_init(_soft_timer_list + i);
    }

    /* start software timer task */
    ek_task_init(&_timer_task,
                   "timer",
                   _timer_task_entry,
                   EOS_NULL,
                   &_timer_task_stack[0],
                   sizeof(_timer_task_stack),
                   EOS_TIMER_THREAD_PRIO,
                   10);

    /* startup */
    eos_task_startup((eos_task_handle_t)&_timer_task);
#endif /* EOS_USING_TIMER_SOFT */
}

static volatile eos_u32_t eos_tick_ = 0;

#ifndef __on_eos_tick_hook
    #define __on_eos_tick_hook()          __ON_HOOK_ARGS(eos_tick_hook, ())
#endif

/**
 * @brief    This function will return current tick from operating system startup.
 * @return   Return current tick.
 */
eos_u32_t eos_tick_get(void)
{
    /* return the global tick */
    return eos_tick_;
}

/**
 * @brief    This function will set current tick.
 * @param    tick is the value that you will set.
 */
void eos_tick_set(eos_u32_t tick)
{
    eos_base_t level;

    level = eos_hw_interrupt_disable();
    eos_tick_ = tick;
    eos_hw_interrupt_enable(level);
}

/**
 * @brief    This function will notify kernel there is one tick passed.
 *           Normally, this function is invoked by clock ISR.
 */
void eos_tick_increase(void)
{
    ek_task_handle_t task;
    eos_base_t level;

    level = eos_hw_interrupt_disable();

    /* increase the global tick */
    ++ eos_tick_;

    /* check time slice */
    task = (ek_task_handle_t)eos_task_self();

    -- task->remaining_tick;
    if (task->remaining_tick == 0)
    {
        /* change to initialized tick */
        task->remaining_tick = task->init_tick;
        task->status |= EOS_TASK_STAT_YIELD;

        eos_hw_interrupt_enable(level);
        eos_schedule();
    }
    else
    {
        eos_hw_interrupt_enable(level);
    }

    /* check timer */
    eos_timer_check();
}

/**
 * @brief    This function will calculate the tick from millisecond.
 * @param    ms is the specified millisecond.
 *              - Negative Number wait forever
 *              - Zero not wait
 *              - Max 0x7fffffff
 * @return   Return the calculated tick.
 */
eos_u32_t eos_tick_from_millisecond(eos_s32_t ms)
{
    eos_u32_t tick;

    if (ms < 0)
    {
        tick = (eos_u32_t)EOS_WAITING_FOREVER;
    }
    else
    {
        tick = EOS_TICK_PER_SECOND * (ms / 1000);
        tick += (EOS_TICK_PER_SECOND * (ms % 1000) + 999) / 1000;
    }

    /* return the calculated tick */
    return tick;
}

/**
 * @brief    This function will return the passed millisecond from boot.
 * @note     if the value of EOS_TICK_PER_SECOND is lower than 1000 or
 *           is not an integral multiple of 1000, this function will not
 *           provide the correct 1ms-based tick.
 * @return   Return passed millisecond from boot.
 */
eos_u32_t eos_tick_get_millisecond(void)
{
#if 1000 % EOS_TICK_PER_SECOND == 0u
    return eos_tick_get() * (1000u / EOS_TICK_PER_SECOND);
#else
    #warning "eos-task cannot provide a correct 1ms-based tick any longer,\
    please redefine this function in another file by using a high-precision hard-timer."
    return 0;
#endif /* 1000 % EOS_TICK_PER_SECOND == 0u */
}

eos_u8_t eos_task_get_priority(eos_task_handle_t task)
{
    return ((ek_task_handle_t)task)->current_priority;
}
