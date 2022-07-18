#include "eventos.h"
#include "eventos_config.h"

EOS_TAG("E-Kernel")

enum
{
    EosTaskState_Ready = 0,
    EosTaskState_Running,
    EosTaskState_Suspend,
    EosTaskState_Delay,
    EosTaskState_DelayNoEvent,
    EosTaskState_WaitEvent,
    EosTaskState_WaitSpecificEvent,
    EosTaskState_WaitMutex,

    EosTaskState_Max
};

#if ((EOS_MAX_TASKS % 8) == 0)
#define EOS_MAX_OWNER                       (EOS_MAX_TASKS >> 3)
#else
#define EOS_MAX_OWNER                       (EOS_MAX_TASKS >> 3 + 1)
#endif

typedef struct eos_owner
{
    uint8_t data[EOS_MAX_OWNER];
} eos_owner_t;

typedef struct eos_kernel
{
    uint32_t task_id_count;

    /* Task */
    eos_object_t *task[EOS_MAX_PRIORITY];
    uint32_t t_prio_ready;
    
    uint8_t sheduler_lock                   : 1;
    uint8_t sheduler                        : 1;

    /* Timer */
    eos_timer_t *timers;
    uint32_t timer_out_min;
    uint32_t cpu_usage_count;

    uint32_t time;
} eos_kernel_t;

/* eos task ----------------------------------------------------------------- */
eos_task_t *volatile eos_current;
eos_task_t *volatile eos_next;

eos_kernel_t ek;

static uint8_t stack_idle[1024];
static eos_task_t task_idle;


void ek_init(void)
{
    /* Initialize the task lists */
    for (uint32_t i = 0; i < EOS_MAX_PRIORITY; i ++)
    {
        ek.task[i] = EOS_NULL;
    }
    
    eos_current = EOS_NULL;
    eos_next = &task_idle;
    
    eos_task_start( &task_idle,
                    "task_idle",
                    task_func_idle, 0, stack_idle, sizeof(stack_idle),
                    EOS_NULL);
}

uint32_t eos_time(void)
{
    return (uint32_t)ek.time;
}

void eos_tick(void)
{
    uint32_t time = ek.time;
    time += EOS_TICK_MS;
    eos.time = time;

    bool sheduler = false;
    eos_interrupt_disable();
    if (eos_current != &task_idle)
    {
        eos_current->timeslice_count ++;
        if (eos_current->timeslice_count >= EOS_TIMESLICE)
        {
            eos_current->timeslice_count = 0;
            eos_object_t *obj_current = ek.task[eos_current->priority];
            if (obj_current->ocb.task.next != EOS_NULL)
            {
                sheduler = true;
            }
        }
    }
    if (sheduler == true)
    {
        eos_sheduler();
    }
    eos_interrupt_enable();

#if (EOS_USE_PREEMPTIVE != 0)
    eos_task_delay_handle();
#endif
}

/* 进入中断 */
void eos_interrupt_enter(void)
{
    if (eos.running == true)
    {
        eos_interrupt_nest ++;
    }
}

/* 退出中断 */
void eos_interrupt_exit(void)
{
    eos_interrupt_disable();
    if (eos.running == true)
    {
        eos_interrupt_nest --;
        EOS_ASSERT(eos_interrupt_nest >= 0);
        if (eos_interrupt_nest == 0 && ek.sheduler == 1)
        {
            eos_sheduler();
        }
    }
    eos_interrupt_enable();
}

void eos_run(void)
{
    eos_hook_start();
    eos.running = true;
    
    eos_interrupt_disable();
    eos_sheduler();
    eos_interrupt_enable();
}

#if (EOS_USE_PREEMPTIVE != 0)
void eos_sheduler_lock(void)
{
    ek.sheduler_lock = 1; 
}

void eos_sheduler_unlock(void)
{
    ek.sheduler_lock = 0;
    eos_interrupt_disable();
    eos_sheduler();
    eos_interrupt_enable();
}
#endif


void eos_task_exit(void)
{
    eos_interrupt_disable();
    uint8_t i = eos_current->priority;
    
    // delete the task from the task list.
    eos_object_t *obj = &eos.object[eos_current->id];
    obj->key = EOS_NULL;
    
    if (obj->ocb.task.next == EOS_NULL && obj->ocb.task.prev == EOS_NULL)
    {
        ek.task[i] = EOS_NULL;
    }
    else if (obj->ocb.task.next != EOS_NULL && obj->ocb.task.prev == EOS_NULL)
    {
        ek.task[i] = obj->ocb.task.next;
        ek.task[i]->ocb.task.prev = EOS_NULL;
    }
    else if (obj->ocb.task.next == EOS_NULL && obj->ocb.task.prev != EOS_NULL)
    {
        obj->ocb.task.prev->ocb.task.next = EOS_NULL;
    }
    else
    {
        obj->ocb.task.prev->ocb.task.next = obj->ocb.task.next;
        obj->ocb.task.next->ocb.task.prev = obj->ocb.task.prev;
    }

    eos_object_t *list = ek.task[i];
    ek.t_prio_ready &= ~(1 << i);
    while (list != EOS_NULL)
    {
        if (list->ocb.task.tcb->state == EosTaskState_Ready)
        {
            ek.task[i] = list;
            ek.t_prio_ready |= (1 << i);
            break;
        }

        list = list->ocb.task.next;
        if (list == ek.task[i])
        {
            break;
        }
    }

    eos_sheduler();
    eos_interrupt_enable();
}

static inline void eos_delay_ms_private(uint32_t time_ms, bool no_event)
{
    /* Never block the current task forever. */
    EOS_ASSERT(time_ms <= EOS_MS_NUM_30DAY);
    /* Never call eos_delay_ms and eos_delay_ticks in ISR functions. */
    EOS_ASSERT(eos_interrupt_nest == 0);
    /* Never call eos_delay_ms and eos_delay_ticks in the idle task. */
    EOS_ASSERT(eos_current != &task_idle);
    eos_interrupt_disable();
    /* The state of current task must be running. */
    if (eos_current->state != EosTaskState_Running)
    {
        EOS_ASSERT(eos_current->state == EosTaskState_Running);
    }

    uint32_t bit;
    eos_current->timeout = eos.time + time_ms;
    eos_current->state = no_event ?
                         EosTaskState_DelayNoEvent :
                         EosTaskState_Delay;
    bit = (1U << (eos_current->priority));
    eos_object_t *list = ek.task[eos_current->priority];
    ek.t_prio_ready &= ~bit;
    while (list != EOS_NULL)
    {
        if (list->ocb.task.tcb->state == EosTaskState_Ready)
        {
            ek.task[eos_current->priority] = list;
            ek.t_prio_ready |= (1 << eos_current->priority);
            break;
        }

        list = list->ocb.task.next;
        if (list == ek.task[eos_current->priority])
        {
            break;
        }
    }

    eos_sheduler();
    eos_interrupt_enable();
}

void eos_delay_ms(uint32_t time_ms)
{
    eos_delay_ms_private(time_ms, false);
}

void eos_delay_no_event(uint32_t time_ms)
{
    eos_delay_ms_private(time_ms, true);
}

void eos_task_suspend(const char *task)
{
    eos_interrupt_disable();

    uint16_t index = eos_hash_get_index(task);
    EOS_ASSERT(index != EOS_MAX_OBJECTS);

    eos_object_t *obj = &eos.object[index];
    EOS_ASSERT(obj->type == EosObj_Actor);

    obj->ocb.task.tcb->state = EosTaskState_Suspend;
    uint8_t priority = obj->ocb.task.tcb->priority;

    uint32_t bits = (1U << priority);
    eos_object_t *list = ek.task[priority];
    ek.t_prio_ready &= ~bits;
    while (list != EOS_NULL)
    {
        if (list->ocb.task.tcb->state == EosTaskState_Ready)
        {
            ek.task[priority] = list;
            ek.t_prio_ready |= bits;
            break;
        }

        list = list->ocb.task.next;
        if (list == ek.task[priority])
        {
            break;
        }
    }

    eos_sheduler();
    eos_interrupt_enable();
}

void eos_task_resume(const char *task)
{
    eos_interrupt_disable();
    uint16_t index = eos_hash_get_index(task);
    EOS_ASSERT(index != EOS_MAX_OBJECTS);

    eos_object_t *obj = &eos.object[index];
    EOS_ASSERT(obj->type == EosObj_Actor);

    obj->ocb.task.tcb->state = EosTaskState_Ready;
    ek.t_prio_ready |= (1 << obj->ocb.task.tcb->priority);

    eos_sheduler();
    eos_interrupt_enable();
}


void eos_task_yield(void)
{
    eos_interrupt_disable();
    eos_sheduler();
    eos_interrupt_enable();
}

void eos_task_delete(const char *task)
{
    eos_interrupt_disable();

    uint16_t e_id = eos_hash_get_index(task);
    /* Ensure the topic is existed in hash table. */
    EOS_ASSERT(e_id != EOS_MAX_OBJECTS);
    /* Ensure the object is task-type. */
    EOS_ASSERT(eos.object[e_id].type == EosObj_Actor);
    
    uint8_t i = eos.object[e_id].ocb.task.tcb->priority;
    // delete the task from the task list.
    eos_object_t *obj = &eos.object[eos_current->id];
    obj->key = EOS_NULL;
    
    if (obj->ocb.task.next == EOS_NULL && obj->ocb.task.prev == EOS_NULL)
    {
        ek.task[i] = EOS_NULL;
    }
    else if (obj->ocb.task.next != EOS_NULL && obj->ocb.task.prev == EOS_NULL)
    {
        ek.task[i] = obj->ocb.task.next;
        ek.task[i]->ocb.task.prev = EOS_NULL;
    }
    else if (obj->ocb.task.next == EOS_NULL && obj->ocb.task.prev != EOS_NULL)
    {
        obj->ocb.task.prev->ocb.task.next = EOS_NULL;
    }
    else
    {
        obj->ocb.task.prev->ocb.task.next = obj->ocb.task.next;
        obj->ocb.task.next->ocb.task.prev = obj->ocb.task.prev;
    }

    eos_object_t *list = ek.task[i];
    ek.t_prio_ready &= ~(1 << i);
    while (list != EOS_NULL)
    {
        if (list->ocb.task.tcb->state == EosTaskState_Ready)
        {
            ek.task[i] = list;
            ek.t_prio_ready |= (1 << i);
            break;
        }

        list = list->ocb.task.next;
        if (list == ek.task[i])
        {
            break;
        }
    }

    eos_interrupt_enable();
}


static inline void eos_task_delay_handle(void)
{
    bool sheduler = false;
    
    /* check all the tasks are timeout or not */
    eos_interrupt_disable();
    for (int8_t i = (EOS_MAX_PRIORITY - 1); i > 0; i --)
    {
        eos_object_t *list = ek.task[i];

        while (list != EOS_NULL)
        {
            if (list->ocb.task.tcb->state == EosTaskState_Delay ||
                list->ocb.task.tcb->state == EosTaskState_DelayNoEvent)
            {
                if (eos.time >= list->ocb.task.tcb->timeout)
                {
                    list->ocb.task.tcb->state = EosTaskState_Ready;
                    sheduler = true;
                    ek.t_prio_ready |= (1 << i);
                }
            }

            list = list->ocb.task.next;
            if (list == ek.task[i])
            {
                break;
            }
        }
    }

    if (sheduler == true)
    {
        eos_sheduler();
    }
    eos_interrupt_enable();
}

static void task_func_idle(void *parameter)
{
    (void)parameter;

    while (1)
    {
#if (EOS_USE_STACK_USAGE != 0)
        /* Calculate the stack usage of the tasks. */
        uint8_t usage = 0;
        uint32_t *stack;
        uint32_t size_used = 0;
        for (uint8_t i = 0; i < EOS_MAX_PRIORITY; i ++)
        {
            if (ek.task[i] != (eos_object_t *)0)
            {
                size_used = 0;
                stack = (uint32_t *)(ek.task[i]->ocb.task.tcb->stack);
                for (uint32_t m = 0; m < (ek.task[i]->ocb.task.tcb->size / 4); m ++)
                {
                    if (stack[m] == 0xDEADBEEFU)
                    {
                        size_used += 4;
                    }
                    else
                    {
                        break;
                    }
                }
                usage = 100 - (size_used * 100 / ek.task[i]->ocb.task.tcb->size);
                ek.task[i]->ocb.task.tcb->usage = usage;
            }
        }
#endif

#if (EOS_USE_PREEMPTIVE == 0)
        eos_task_delay_handle();
#endif

        eos_timer_poll();

        eos_interrupt_disable();
#if (EOS_USE_TIME_EVENT != 0)
        eos_evttimer();
#endif
        if (eos.time >= EOS_MS_NUM_15DAY)
        {
            /* Adjust all task daley timing. */
            for (uint32_t i = 1; i < EOS_MAX_PRIORITY; i ++)
            {
                // TODO
#if 0
                if (ek.task[i] != (void *)0 && ((ek.task_delay & (1 << i)) != 0))
                {
                    if (ek.task[i]->ocb.task.tcb->timeout != EOS_TIME_FOREVER)
                    {
                        ek.task[i]->ocb.task.tcb->timeout -= eos.time;
                    }
                }
#endif
            }
            /* Adjust all timer's timing. */
            eos_timer_t *list = ek.timers;
            while (list != (eos_timer_t *)0)
            {
                if (list->running != 0)
                {
                    list->time_out -= eos.time;
                }
                list = list->next;
            }
            ek.timer_out_min -= eos.time;
            /* Adjust all event timer's */
            eos.timeout_min -= eos.time;
            for (uint32_t i = 0; i < eos.timer_count; i ++)
            {
                EOS_ASSERT(eos.etimer[i].timeout >= eos.time);
                eos.etimer[i].timeout -= eos.time;
            }
            eos.time_offset += eos.time;
            eos.time = 0;
        }
        eos_interrupt_enable();
        eos_hook_idle();
    }
}

static void eos_sheduler(void)
{
    // EOS_ASSERT(critical_count != 0);

    uint32_t isr_count = critical_count;
    
#if (EOS_USE_PREEMPTIVE != 0)
    if (ek.sheduler_lock == 1)
    {
        return;
    }
#endif
    
    if (eos.running == EOS_False)
    {
        return;
    }
    
    if (eos_interrupt_nest > 0)
    {
        ek.sheduler = 1;
        return;
    }

    /* eos_next = ... */
    task_idle.state = EosTaskState_Ready;
    eos_next = &task_idle;
    for (int8_t i = (EOS_MAX_PRIORITY - 1); i > 0; i --)
    {
        /* If the task is existent. */
        if ((ek.t_prio_ready & (1 << i)) != 0 && ek.task[i] != EOS_NULL)
        {
            eos_object_t *list = ek.task[i];
            if (list->ocb.task.tcb->state == EosTaskState_Ready ||
                list->ocb.task.tcb->state == EosTaskState_Running)
            {
                eos_next = list->ocb.task.tcb;
                break;
            }
        }
    }

    /* Trigger PendSV, if needed */
    if (eos_next != eos_current)
    {
        if (eos_current != EOS_NULL)
        {
            if (eos_current != &task_idle)
            {
                eos_object_t *list = ek.task[eos_current->priority];
                if (list->ocb.task.next != list)
                {
                    list = list->ocb.task.next;
                    while (1)
                    {
                        EOS_ASSERT(list->ocb.task.tcb->state != EosTaskState_Running);
                        if (list->ocb.task.tcb->state == EosTaskState_Ready)
                        {
                            ek.task[eos_current->priority] = list;
                            break;
                        }
                        list = list->ocb.task.next;
                        if (list == ek.task[eos_current->priority])
                        {
                            break;
                        }
                    }
                }
            }
            
            
            if (eos_current->state == EosTaskState_Running)
            {
                eos_current->state = EosTaskState_Ready;
                ek.t_prio_ready |= (1 << eos_current->priority);
            }
            EOS_ASSERT(eos_current->state != EosTaskState_Running);
        }

        eos_next->state = EosTaskState_Running;
        eos_next->timeslice_count = 0;
        EOS_ASSERT(eos_next->state == EosTaskState_Running);

        eos_port_task_switch();
    }
}

void eos_task_start(eos_task_t *const me,
                    const char *name,
                    eos_func_t func,
                    uint8_t priority,
                    void *stack_addr,
                    uint32_t stack_size,
                    void *parameter)
{
    EOS_ASSERT(priority < EOS_MAX_PRIORITY);
    eos_interrupt_disable();

    uint16_t index = eos_task_init(me, name, priority, stack_addr, stack_size);
    eos.object[index].ocb.task.tcb = me;
    eos.object[index].type = EosObj_Actor;

    eos_task_start_private(me, func, me->priority,
                           stack_addr, stack_size, parameter);
    me->state = EosTaskState_Ready;

    eos_sheduler();
    eos_interrupt_enable();
}


/* -----------------------------------------------------------------------------
Timer
----------------------------------------------------------------------------- */
/* 启动软定时器，允许在中断中调用。 */
void eos_timer_start(   eos_timer_t *const me,
                        const char *name,
                        uint32_t time_ms,
                        bool oneshoot,
                        eos_func_t callback)
{
    EOS_ASSERT(time_ms <= EOS_MS_NUM_30DAY);

    /* Check the timer's name is not same with others. */
    eos_interrupt_disable();
    uint16_t index = eos_hash_get_index(name);
    EOS_ASSERT(index == EOS_MAX_OBJECTS);

    /* Timer data. */
    me->time = time_ms;
    me->time_out = eos.time + time_ms;
    me->callback = callback;
    me->oneshoot = oneshoot == false ? 0 : 1;
    me->running = 1;

    /* Add in the hash table. */
    index = eos_hash_insert(name);
    eos.object[index].type = EosObj_Timer;
    eos.object[index].ocb.timer = me;
    /* Add the timer to the list. */
    me->next = ek.timers;
    ek.timers = me;
    
    if (ek.timer_out_min > me->time_out)
    {
        ek.timer_out_min = me->time_out;
    }

    eos_interrupt_enable();
}

/* 删除软定时器，允许在中断中调用。 */
void eos_timer_delete(const char *name)
{
    /* Check the timer is existent or not. */
    eos_interrupt_disable();
    uint16_t index = eos_hash_get_index(name);
    EOS_ASSERT(index != EOS_MAX_OBJECTS);

    eos_timer_t *current = eos.object[index].ocb.timer;
    eos.object[index].key = (const char *)0;
    eos_timer_t *list = ek.timers;
    eos_timer_t *last = (eos_timer_t *)0;
    while (list != (eos_timer_t *)0)
    {
        if (list == current)
        {
            if (last == (eos_timer_t *)0)
            {
                ek.timers = list->next;
            }
            else
            {
                last->next = list->next;
            }

            eos_interrupt_enable();
            
            return;
        }
        last = list;
        list = list->next;
    }

    /* not found. */
    EOS_ASSERT(0);
}

/* 暂停软定时器，允许在中断中调用。 */
void eos_timer_pause(const char *name)
{
    /* Check the timer is existent or not. */
    eos_interrupt_disable();
    uint16_t index = eos_hash_get_index(name);
    EOS_ASSERT(index != EOS_MAX_OBJECTS);
    eos_timer_t *timer = eos.object[index].ocb.timer;
    timer->running = 0;
    timer->time_out -= eos_time();

    /* Recalculate the minimum value of the timers. */
    eos_timer_t *list = ek.timers;
    uint32_t time_out_min = UINT32_MAX;
    while (list != (eos_timer_t *)0)
    {
        if (list->running != 0 && time_out_min > list->time_out)
        {
            time_out_min = list->time_out;
        }
        list = list->next;
    }
    ek.timer_out_min = time_out_min;

    eos_interrupt_enable();
}

/* 继续软定时器，允许在中断中调用。 */
void eos_timer_continue(const char *name)
{
    eos_interrupt_disable();
    uint16_t index = eos_hash_get_index(name);
    EOS_ASSERT(index != EOS_MAX_OBJECTS);
    eos_timer_t *timer = eos.object[index].ocb.timer;
    timer->running = 1;
    timer->time_out += eos_time();

    if (ek.timer_out_min > timer->time_out)
    {
        ek.timer_out_min = timer->time_out;
    }
    eos_interrupt_enable();
}

/* 重启软定时器的定时，允许在中断中调用。 */
void eos_timer_reset(const char *name)
{
    eos_interrupt_disable();
    uint16_t index = eos_hash_get_index(name);
    EOS_ASSERT(index != EOS_MAX_OBJECTS);
    eos_timer_t *timer = eos.object[index].ocb.timer;
    timer->running = 1;
    timer->time_out = eos.time + timer->time;
    eos_interrupt_enable();
}

static void eos_timer_poll(void)
{
    eos_interrupt_disable();
    if (ek.timer_out_min > eos_time())
    {
        eos_interrupt_enable();
        return;
    }
    eos_timer_t *timer = ek.timers;
    uint32_t time_out_min = UINT32_MAX;
    while (timer != EOS_NULL)
    {
        if (timer->running && timer->time_out <= eos_time())
        {
            eos_interrupt_enable();
            timer->callback(NULL);
            eos_interrupt_disable();
            if (timer->oneshoot)
            {
                timer->running = false;
            }
            else
            {
                timer->time_out += timer->time;
            }
        }
        if (time_out_min > timer->time_out)
        {
            time_out_min = timer->time_out;
        }

        timer = timer->next;
    }

    ek.timer_out_min = time_out_min;
    eos_interrupt_enable();
}


/* -----------------------------------------------------------------------------
Trace
----------------------------------------------------------------------------- */
#if (EOS_USE_STACK_USAGE != 0)
/* 任务的堆栈使用率 */
uint8_t eos_task_stack_usage(uint8_t priority)
{
    EOS_ASSERT(priority < EOS_MAX_PRIORITY);
    EOS_ASSERT(ek.task[priority] != (eos_object_t *)0);

    return ek.task[priority]->ocb.task.tcb->usage;
}
#endif

#if (EOS_USE_CPU_USAGE != 0)
/* 任务的CPU使用率 */
uint8_t eos_task_cpu_usage(uint8_t priority)
{
    EOS_ASSERT(priority < EOS_MAX_PRIORITY);
    EOS_ASSERT(ek.task[priority] != (eos_object_t *)0);

    return ek.task[priority]->ocb.task.tcb->cpu_usage;
}

/* 监控函数，放进一个单独的定时器中断函数，中断频率为SysTick的10-20倍。 */
void eos_cpu_usage_monitor(void)
{
    uint8_t usage;

    /* Calculate the CPU usage. */
    ek.cpu_usage_count ++;
    eos_current->cpu_usage_count ++;
    if (ek.cpu_usage_count >= 10000)
    {
        for (uint8_t i = 0; i < EOS_MAX_PRIORITY; i ++)
        {
            if (ek.task[i] != (eos_object_t *)0)
            {
                usage = ek.task[i]->ocb.task.tcb->cpu_usage_count * 100 /
                        ek.cpu_usage_count;
                ek.task[i]->ocb.task.tcb->cpu_usage = usage;
                ek.task[i]->ocb.task.tcb->cpu_usage_count = 0;
            }
        }
        
        ek.cpu_usage_count = 0;
    }
}
#endif

void eos_task_blcok_for_event(uint32_t time_ms)
{
    uint32_t bits = (1U << eos_current->priority);
    if (time_ms == EOS_TIME_FOREVER)
    {
        eos_current->timeout = EOS_TIME_FOREVER;
    }
    else
    {
        eos_current->timeout = eos_time() + time_ms;
    }
    
    eos_current->state = EosTaskState_WaitEvent;
    eos_current->event_wait = EOS_NULL;

    eos_object_t *list = ek.task[eos_current->priority];
    ek.t_prio_ready &= ~bits;
    while (list != EOS_NULL)
    {
        if (list->ocb.task.tcb->state == EosTaskState_Ready)
        {
            ek.task[eos_current->priority] = list;
            ek.t_prio_ready |= bits;
            break;
        }

        list = list->ocb.task.next;
        if (list == ek.task[eos_current->priority])
        {
            break;
        }
    }

    eos_sheduler();
    eos_interrupt_enable();
    eos_interrupt_disable();
}

uint16_t eos_task_current_tid(void)
{
    return eos_current->t_id;
}

/* -----------------------------------------------------------------------------
Mutex
----------------------------------------------------------------------------- */
void eos_mutex_take(const char *name)
{
    eos_interrupt_disable();

    if (eos.running != 0)
    {
        /* Get the mutex id according to the mutex name. */
        uint16_t m_id = eos_hash_get_index(name);
        if (m_id == EOS_MAX_OBJECTS)
        {
            /* Newly create one event in the hash table. */
            m_id = eos_hash_insert(name);
            eos.object[m_id].type = EosObj_Mutex;
            eos.object[m_id].ocb.mutex.t_id = EOS_MAX_OBJECTS;
        }
        else
        {
            /* Ensure the object's type is mutex. */
            EOS_ASSERT(eos.object[m_id].type == EosObj_Mutex);
        }
        uint32_t bits = (1 << eos_current->priority);

        /* The mutex is accessed by other tasks. */
        if (eos.object[m_id].ocb.mutex.t_id != EOS_MAX_OBJECTS)
        {
            /* Set the flag bit in mutex to suspend the current task. */
            owner_set_bit(&eos.object[m_id].ocb.mutex.e_owner, eos_current->t_id, true);
            eos_current->state = EosTaskState_WaitMutex;

            /* Excute eos kernel sheduler. */
            eos_sheduler();
        }
        /* No task is accessing the mutex. */
        else
        {
            /* Set the current the task id of the current mutex. */
            eos.object[m_id].ocb.mutex.t_id = eos_current->id;
        }
    }

    eos_interrupt_enable();
}

void eos_mutex_release(const char *name)
{
    eos_interrupt_disable();

    if (eos.running != 0)
    {
        /* Get the mutex id according to the mutex name. */
        uint16_t m_id = eos_hash_get_index(name);
        EOS_ASSERT(m_id != EOS_MAX_OBJECTS);

        /* Ensure the object's type is mutex. */
        EOS_ASSERT(eos.object[m_id].type == EosObj_Mutex);

        eos.object[m_id].ocb.mutex.t_id = EOS_MAX_OBJECTS;

        /* The mutex is accessed by other higher-priority tasks. */
        if (owner_not_cleared(&eos.object[m_id].ocb.mutex.e_owner))
        {
            bool found = false;

            // Find the highest-priority task in mutex-block state.
            for (int8_t i = (EOS_MAX_PRIORITY - 1); i > 0; i --)
            {
                eos_object_t *list = ek.task[i];
                while (list != EOS_NULL)
                {
                    if (owner_is_occupied(&eos.object[m_id].ocb.event.e_owner,
                                          list->ocb.task.tcb->t_id))
                    {
                        list->ocb.task.tcb->state = EosTaskState_Ready;
                        /* Clear the flag in event mutex and gobal mutex. */
                        owner_set_bit(&eos.object[m_id].ocb.event.e_owner,
                                      list->ocb.task.tcb->t_id, false);
                        found = true;
                        break;
                    }

                    list = list->ocb.task.next;
                    if (list == ek.task[i])
                    {
                        break;
                    }
                }
                if (found)
                {
                    break;
                }
            }

            /* Excute eos kernel sheduler. */
            eos_sheduler();
        }
    }

    eos_interrupt_enable();
}
