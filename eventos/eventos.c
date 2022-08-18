/*
 * EventOS
 * Copyright (c) 2021, EventOS Team, <event-os@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the 'Software'), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS 
 * OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.event-os.cn
 * https://github.com/event-os/eventos
 * https://gitee.com/event-os/eventos
 * 
 * Change Logs:
 * Date           Author        Notes
 * 2021-11-23     DogMing       V0.0.2
 * 2021-03-20     DogMing       V0.2.0
 */

/* include --------------------------------------------------------------------- */
#include "eventos.h"
#include <string.h>

EOS_TAG("EventOS")

#ifdef __cplusplus
extern "C" {
#endif

/* eos define ------------------------------------------------------------------ */
enum
{
    EosObj_Actor = 0,                           // prefix A
    EosObj_Event,                               // prefix E
    EosObj_Timer,                               // prefix T
    EosObj_Max
};

enum
{
    EosEventGiveType_Send = 0,
    EosEventGiveType_Publish,
};

/* Event atrribute ---------------------------------------------------------- */
#if ((EOS_MAX_TASKS % 8) == 0)
#define EOS_MAX_OWNER                       (EOS_MAX_TASKS >> 3)
#else
#define EOS_MAX_OWNER                       (EOS_MAX_TASKS >> 3 + 1)
#endif

#if ((EOS_MAX_OBJECTS % 8) == 0)
#define EOS_MAX_TASK_OCCUPY                 (EOS_MAX_OBJECTS >> 3)
#else
#define EOS_MAX_TASK_OCCUPY                 (EOS_MAX_OBJECTS >> 3 + 1)
#endif

/* Timer atrribute ---------------------------------------------------------- */
#define EOS_TIMER_ATTRIBUTE_SEND            ((eos_u8_t)0x01U)
#define EOS_TIMER_ATTRIBUTE_PUBLISH         ((eos_u8_t)0x00U)

/* Event atrribute ---------------------------------------------------------- */
#define EOS_EVENT_ATTRIBUTE_GLOBAL          ((eos_u8_t)0x80U)
#define EOS_EVENT_ATTRIBUTE_UNBLOCKED       ((eos_u8_t)0x20U)
#define EOS_EVENT_ATTRIBUTE_TOPIC           ((eos_u8_t)0x00U)
#define EOS_EVENT_ATTRIBUTE_VALUE           EOS_DB_ATTRIBUTE_VALUE
#define EOS_EVENT_ATTRIBUTE_STREAM          EOS_DB_ATTRIBUTE_STREAM
#define EOS_EVENT_ATTRIBUTE_MASK            ((eos_u8_t)0x03U)

/* Task attribute ----------------------------------------------------------- */
#define EOS_TASK_ATTRIBUTE_TASK             ((eos_u8_t)0x00U)
#define EOS_TASK_ATTRIBUTE_REACTOR          ((eos_u8_t)0x01U)
#define EOS_TASK_ATTRIBUTE_SM               ((eos_u8_t)0x02U)

#if (EOS_USE_TIME_EVENT != 0)
#define EOS_MS_NUM_30DAY                    (2592000000U)
#define EOS_MS_NUM_15DAY                    (1296000000U)

typedef struct eos_owner
{
    eos_u8_t data[EOS_MAX_OWNER];
} eos_owner_t;
#endif

typedef struct eos_heap_block
{
    struct eos_heap_block *next;
    eos_u32_t is_free                            : 1;
    eos_u32_t size;
} eos_heap_block_t;

typedef struct eos_heap_tag
{
    eos_u8_t *data;
    eos_heap_block_t *list;
    eos_u32_t size;                              /* total size */
    eos_u32_t error_id                           : 3;
} eos_heap_t;

typedef struct eos_event_data
{
    struct eos_event_data *next;
    struct eos_event_data *last;
    eos_owner_t e_owner;
    eos_u32_t time;
    eos_u16_t id;
    eos_u8_t type;
} eos_event_data_t;

enum
{
    Stream_OK                       = 0,
    Stream_Empty                    = -1,
    Stream_Full                     = -2,
    Stream_NotEnough                = -3,
    Stream_MemCovered               = -4,
};

typedef struct eos_stream
{
    void *data;
    eos_u32_t head;
    eos_u32_t tail;
    bool empty;

    eos_u32_t capacity;
} eos_stream_t;

struct eos_object;

typedef union eos_obj_block
{
    struct
    {
        eos_event_data_t *e_item;
        eos_owner_t e_sub;
        eos_owner_t e_owner;                            /* The event owner */
    } event;
    struct
    {
        eos_task_handle_t tcb;
    } task;
    struct
    {
        eos_timer_t timer;
        eos_task_handle_t target;
    } timer;
} eos_ocb_t;

typedef struct eos_object
{
    const char *key;                                    /* Key */
    eos_ocb_t ocb;                                      /* object block */
    eos_u32_t type                   : 8;               /* Object type */
    eos_u32_t attribute              : 8;
    eos_u32_t size                   : 16;              /* Value size */
    union
    {
        void *value;                                    /* for value-event */
        eos_stream_t *stream;                           /* for stream-event */
    } data;
} eos_object_t;

typedef struct eos_tag
{
    /* Hash table */
    eos_object_t object[EOS_MAX_OBJECTS];
    eos_u16_t prime_max;
    eos_u8_t obj_task_occupy[EOS_MAX_TASK_OCCUPY];
    
    eos_u16_t t_id[EOS_MAX_TASKS];

    /* Heap */
#if (EOS_USE_EVENT_DATA != 0)
    eos_heap_t heap;
    eos_u8_t heap_data[EOS_SIZE_HEAP];
#endif
    eos_heap_t db;
    eos_event_data_t *e_queue;
} eos_t;

eos_t eos;

/* data ------------------------------------------------------------------------ */
#if (EOS_USE_SM_MODE != 0)
enum eos_event_topic
{
#if (EOS_USE_SM_MODE != 0)
    Event_Null = 0,
    Event_Enter,
    Event_Exit,
#if (EOS_USE_HSM_MODE != 0)
    Event_Init,
#endif
    Event_User,
#else
    Event_Null = 0,
    Event_User,
#endif
};

static const eos_event_t eos_event_table[Event_User] =
{
    {"Event_Null", 0, 0},
    {"Event_Enter", 0, 0},
    {"Event_Exit", 0, 0},
#if (EOS_USE_HSM_MODE != 0)
    {"Event_Init", 0, 0},
#endif
};
#endif

/* macro -------------------------------------------------------------------- */
#if (EOS_USE_SM_MODE != 0)
#define HSM_TRIG_(state_, topic_)                                              \
                ((*(state_))(me, &eos_event_table[topic_]))
#endif

/* private hash function ---------------------------------------------------- */
static eos_u32_t eos_hash_time33(char ch_type, const char *string);
static eos_u16_t eos_hash_insert(eos_u8_t obj_type, const char *string);
static eos_u16_t eos_hash_get_index(eos_u8_t obj_type, const char *string);
static bool eos_hash_existed(eos_u8_t obj_type, const char *string);

/* private event functions -------------------------------------------------- */
static eos_s8_t eos_event_give_(const char *task,
                                eos_u32_t task_id,
                                eos_u8_t give_type,
                                const char *topic);
static void eos_e_queue_delete_(eos_event_data_t const *item);
static inline void eos_event_sub_(eos_task_handle_t const me, const char *topic);

/* private actor functions -------------------------------------------------- */
static void eos_reactor_enter(eos_reactor_t *const me);
static void eos_sm_enter(eos_sm_t *const me);

/* private database functions ----------------------------------------------- */
eos_inline void eos_db_write_(eos_u8_t type, const char *key, 
                                const void *memory, eos_u32_t size);
eos_inline eos_s32_t eos_db_read_(eos_u8_t type,
                                    const char *key, 
                                    const void *memory, eos_u32_t size);

/* private sm functions ----------------------------------------------------- */
#if (EOS_USE_SM_MODE != 0)
static void eos_sm_dispath(eos_sm_t *const me, eos_event_t const * const e);
#if (EOS_USE_HSM_MODE != 0)
static eos_s32_t eos_sm_tran(eos_sm_t *const me,
                                eos_state_handler path[EOS_MAX_HSM_NEST_DEPTH]);
#endif
#endif

/* private heap functions --------------------------------------------------- */
#if (EOS_USE_EVENT_DATA != 0)
static void eos_heap_init(eos_heap_t *const me, void *data, eos_u32_t size);
static void * eos_heap_malloc(eos_heap_t *const me, eos_u32_t size);
static void eos_heap_free(eos_heap_t *const me, void * data);
#endif

/* private stream functions ------------------------------------------------- */
static eos_s32_t eos_stream_init(eos_stream_t *const me,
                                    void *memory, eos_u32_t capacity);
static eos_s32_t eos_stream_push(eos_stream_t *me, void * data, eos_u32_t size);
static eos_s32_t eos_stream_pull_pop(eos_stream_t *me,
                                        void * data, eos_u32_t size);
static bool eos_stream_full(eos_stream_t *me);
static eos_s32_t eos_stream_size(eos_stream_t *me);
static eos_s32_t eos_stream_empty_size(eos_stream_t *me);

/* private owner functions -------------------------------------------------- */
static inline bool owner_is_occupied(eos_owner_t *owner, eos_u32_t t_id);
static inline void owner_or(eos_owner_t *g_owner, eos_owner_t *owner);
static inline void owner_set_bit(eos_owner_t *owner, eos_u32_t t_id, bool status);
static inline bool owner_all_cleared(eos_owner_t *owner);

/* extern functions --------------------------------------------------------- */
extern void eos_kernel_init(void);

/* public functions --------------------------------------------------------- */
void eos_init(void)
{
    eos.e_queue = EOS_NULL;
    for (eos_u16_t i = 0; i < EOS_MAX_TASKS; i ++)
    {
        eos.t_id[i] = EOS_MAX_OBJECTS;
    }

#if (EOS_USE_EVENT_DATA != 0)
    eos_heap_init(&eos.heap, eos.heap_data, EOS_SIZE_HEAP);
#endif

    /* Find the maximum prime in the range of EOS_MAX_OBJECTS. */
    for (eos_s32_t i = EOS_MAX_OBJECTS; i > 0; i --)
    {
        bool is_prime = true;
        for (eos_u32_t j = 2; j < EOS_MAX_OBJECTS; j ++)
        {
            if (i <= j)
            {
                break;
            }
            if ((i % j) == 0)
            {
                is_prime = false;
                break;
            }
        }
        if (is_prime)
        {
            eos.prime_max = i;
            break;
        }
    }
    
    /* Initialize the hash table. */
    for (eos_u32_t i = 0; i < EOS_MAX_OBJECTS; i ++)
    {
        eos.object[i].key = (const char *)0;
    }

    eos_kernel_init();
    eos_system_timer_init();
    eos_system_timer_task_init();
    eos_task_idle_init();
}

/* -----------------------------------------------------------------------------
Task
----------------------------------------------------------------------------- */
eos_err_t eos_task_init(eos_task_t *task,
                        const char *name,
                        void (*entry)(void *parameter),
                        void *parameter,
                        void *stack_start,
                        eos_u32_t stack_size,
                        eos_u8_t priority,
                        eos_u32_t tick)
{
    register eos_base_t level = eos_hw_interrupt_disable();

    /* Get task id according to the event topic. */
    eos_u16_t t_id = eos_hash_get_index(EosObj_Actor, name);
    EOS_ASSERT(t_id == EOS_MAX_OBJECTS);
    /* Newly create one task in the hash table. */
    t_id = eos_hash_insert(EosObj_Actor, name);
    eos.object[t_id].type = EOS_TASK_ATTRIBUTE_TASK;
    eos.object[t_id].ocb.task.tcb = task;
    task->t_id = t_id;
    for (eos_u16_t i = 0; i < EOS_MAX_TASKS; i ++)
    {
        if (eos.t_id[i] == EOS_MAX_OBJECTS)
        {
            task->index = i;
            eos.t_id[i] = t_id;
        }
    }
    
    eos.obj_task_occupy[t_id / 8] |= (1 << (t_id % 8));
    
#if (EOS_USE_3RD_KERNEL == 0)
    task->task_handle = (eos_u32_t)(&task->task_);
#endif

    eos_sem_init(&task->sem, name, 0, EOS_IPC_FLAG_FIFO);
    eos_hw_interrupt_enable(level);

    return ek_task_init((ek_task_t *)task->task_handle,
                        name, entry, parameter,
                        stack_start, stack_size,
                        priority, tick);
}

static bool equeue_no_current_task_event(eos_u16_t t_id)
{
    if (eos.e_queue == EOS_NULL)
    {
        return true;
    }
    else
    {
        eos_event_data_t *e_item = eos.e_queue;
        while (e_item != EOS_NULL)
        {
            if (!owner_is_occupied(&e_item->e_owner, t_id))
            {
                e_item = e_item->next;
            }

            return false;
        }

        return true;
    }
}

bool eos_task_wait_event(eos_event_t *const e_out, eos_s32_t time_ms)
{
    EOS_ASSERT(time_ms <= EOS_MS_NUM_30DAY || time_ms == EOS_TIME_FOREVER);

    eos_task_handle_t task = eos_task_self();
    eos_err_t ret = eos_sem_take(&task->sem, time_ms);
    
    register eos_base_t level = eos_hw_interrupt_disable();
    eos_u16_t t_id = task->t_id;

    if (ret == EOS_EOK)
    {
        EOS_ASSERT(eos.e_queue != EOS_NULL);
        /* Find the first event data in e-queue and set it as handled. */
        eos_event_data_t *e_item = eos.e_queue;
        while (e_item != EOS_NULL)
        {
            if (!owner_is_occupied(&e_item->e_owner, t_id))
            {
                e_item = e_item->next;
            }
            else
            {
                /* Meet one event */
                eos_object_t *e_object = &eos.object[e_item->id];
                EOS_ASSERT(e_object->type == EosObj_Event);
                eos_u8_t type = e_object->attribute & 0x03;
                /* Event out */
                e_out->topic = e_object->key;
                e_out->eid = e_item->id;
                if (type == EOS_EVENT_ATTRIBUTE_TOPIC)
                {
                    e_out->size = 0;
                }
                else if (type == EOS_EVENT_ATTRIBUTE_VALUE)
                {
                    e_out->size = e_object->size;
                }
                else if (type == EOS_EVENT_ATTRIBUTE_STREAM)
                {
                    e_out->size = eos_stream_size(e_object->data.stream);
                }

                /* If the event data is just the current task's event. */
                owner_set_bit(&e_item->e_owner, t_id, false);
                if (owner_all_cleared(&e_item->e_owner))
                {
                    eos.object[e_item->id].ocb.event.e_item = EOS_NULL;
                    /* Delete the event data from the e-queue. */
                    eos_e_queue_delete_(e_item);

                    if (equeue_no_current_task_event(t_id))
                    {
                        eos_sem_reset(&task->sem, 0);
                    }
                }

                eos_hw_interrupt_enable(level);

                return true;
            }
        }
    }

    eos_hw_interrupt_enable(level);

    return false;
}

bool eos_task_wait_specific_event(  eos_event_t *const e_out,
                                    const char *topic, eos_s32_t time_ms)
{
    EOS_ASSERT(time_ms <= EOS_MS_NUM_30DAY || time_ms == EOS_TIME_FOREVER);

    eos_task_handle_t task = eos_task_self();
    while (1)
    {
        eos_err_t ret = eos_sem_take(&task->sem, time_ms);
        
        register eos_base_t level = eos_hw_interrupt_disable();
        eos_u16_t t_id = task->t_id;

        if (ret == EOS_EOK)
        {
            EOS_ASSERT(eos.e_queue != EOS_NULL);
            /* Find the first event data in e-queue and set it as handled. */
            eos_event_data_t *e_item = eos.e_queue;
            while (e_item != EOS_NULL)
            {
                if (!owner_is_occupied(&e_item->e_owner, t_id))
                {
                    e_item = e_item->next;
                }
                else
                {
                    bool correct_event = false;

                    /* Meet one event */
                    eos_object_t *e_object = &eos.object[e_item->id];
                    EOS_ASSERT(e_object->type == EosObj_Event);
                    if (strcmp(e_object->key, topic) == 0)
                    {
                        correct_event = true;
                    }

                    if (correct_event)
                    {
                        eos_u8_t type = e_object->attribute & 0x03;
                        /* Event out */
                        e_out->topic = e_object->key;
                        e_out->eid = e_item->id;
                        if (type == EOS_EVENT_ATTRIBUTE_TOPIC)
                        {
                            e_out->size = 0;
                        }
                        else if (type == EOS_EVENT_ATTRIBUTE_VALUE)
                        {
                            e_out->size = e_object->size;
                        }
                        else if (type == EOS_EVENT_ATTRIBUTE_STREAM)
                        {
                            e_out->size = eos_stream_size(e_object->data.stream);
                        }
                    }

                    /* If the event data is just the current task's event. */
                    owner_set_bit(&e_item->e_owner, t_id, false);
                    if (owner_all_cleared(&e_item->e_owner))
                    {
                        eos.object[e_item->id].ocb.event.e_item = EOS_NULL;
                        /* Delete the event data from the e-queue. */
                        eos_e_queue_delete_(e_item);

                        if (equeue_no_current_task_event(t_id))
                        {
                            eos_sem_reset(&task->sem, 0);
                        }
                    }

                    if (correct_event)
                    {
                        eos_hw_interrupt_enable(level);
                        return true;
                    }
                    else
                    {
                        continue;
                    }
                }
            }
        }

        eos_hw_interrupt_enable(level);
        break;
    }

    return false;
}

/* -----------------------------------------------------------------------------
Event
----------------------------------------------------------------------------- */
void eos_event_attribute_global(const char *topic)
{
    register eos_base_t level = eos_hw_interrupt_disable();

    eos_u16_t e_id;
    if (eos_hash_existed(EosObj_Event, topic) == false)
    {
        e_id = eos_hash_insert(EosObj_Event, topic);
        eos.object[e_id].type = EosObj_Event;
    }
    EOS_ASSERT(eos.object[e_id].type == EosObj_Event);
    eos.object[e_id].attribute |= EOS_EVENT_ATTRIBUTE_GLOBAL;
    
    eos_hw_interrupt_enable(level);
}

void eos_event_attribute_unblocked(const char *topic)
{
    register eos_base_t level = eos_hw_interrupt_disable();

    eos_u16_t e_id;
    if (eos_hash_existed(EosObj_Event, topic) == false)
    {
        e_id = eos_hash_insert(EosObj_Event, topic);
        eos.object[e_id].type = EosObj_Event;
    }
    EOS_ASSERT(eos.object[e_id].type == EosObj_Event);
    eos.object[e_id].attribute |= EOS_EVENT_ATTRIBUTE_UNBLOCKED;

    eos_hw_interrupt_enable(level);
}

static void eos_task_function(void *parameter)
{
    (void)parameter;

    eos_task_handle_t task_current = eos_task_self();
    eos_u8_t type = eos.object[task_current->t_id].attribute;
    /* Reactor exutes the event Enter. */
    if (type == EOS_TASK_ATTRIBUTE_REACTOR)
    {
        eos_reactor_enter((eos_reactor_t *)task_current);
    }
    /* State machine enter all initial states. */
    else if (type == EOS_TASK_ATTRIBUTE_SM)
    {
        eos_sm_enter((eos_sm_t *)task_current);
    }
    else
    {
        EOS_ASSERT(0);
    }

    while (1)
    {
        eos_event_t e;
        if (eos_task_wait_event(&e, EOS_WAITING_FOREVER))
        {
            if (type == EOS_TASK_ATTRIBUTE_SM)
            {
                eos_sm_dispath((eos_sm_t *)task_current, &e);
            }
            else if (type == EOS_TASK_ATTRIBUTE_REACTOR)
            {
                eos_reactor_t *reactor = (eos_reactor_t *)task_current;
                reactor->event_handler(reactor, &e);
            }
        }
    }
}

static void eos_e_queue_delete_(eos_event_data_t const *item)
{
    EOS_ASSERT(eos.e_queue != EOS_NULL);
    EOS_ASSERT(item != EOS_NULL);

    register eos_base_t level = eos_hw_interrupt_disable();
    
    /* If the event data is only one in queue. */
    if (item->last == EOS_NULL && item->next == EOS_NULL)
    {
        EOS_ASSERT(eos.e_queue == item);
        eos.e_queue = EOS_NULL;
    }
    /* If the event data is the first one in queue. */
    else if (item->last == EOS_NULL && item->next != EOS_NULL)
    {
        EOS_ASSERT(eos.e_queue == item);
        item->next->last = EOS_NULL;
        eos.e_queue = item->next;
    }
    /* If the event item is the last one in queue. */
    else if (item->last != EOS_NULL && item->next == EOS_NULL)
    {
        item->last->next = EOS_NULL;
    }
    /* If the event item is in the middle position of the queue. */
    else
    {
        item->last->next = item->next;
        item->next->last = item->last;
    }
    
    /* free the event data. */
    eos_heap_free(&eos.heap, (void *)item);

    eos_hw_interrupt_enable(level);
}

/* 关于Reactor -------------------------------------------------------------- */
void eos_reactor_init(  eos_reactor_t *const me,
                        const char *name,
                        eos_u8_t priority,
                        void *stack, eos_u32_t size)
{
    eos_task_init(&me->super,
                    name,
                    eos_task_function,
                    EOS_NULL,
                    stack, size,
                    priority,
                    10);

    eos.object[me->super.t_id].type = EosObj_Actor;
    eos.object[me->super.t_id].attribute = EOS_TASK_ATTRIBUTE_REACTOR;
}

void eos_reactor_start(eos_reactor_t *const me, eos_event_handler event_handler)
{
    me->event_handler = event_handler;
    
    eos_event_give_(eos.object[me->super.t_id].key,
                    EOS_MAX_OBJECTS,
                    EosEventGiveType_Send, "Event_Null");
    
    eos_task_startup(&me->super);
}

/* state machine ------------------------------------------------------------ */
#if (EOS_USE_SM_MODE != 0)
void eos_sm_init(   eos_sm_t *const me,
                    const char *name,
                    eos_u8_t priority,
                    void *stack, eos_u32_t size)
{
    eos_task_init(&me->super,
                    name,
                    eos_task_function,
                    EOS_NULL,
                    stack, size,
                    priority,
                    10);
    eos.object[me->super.t_id].type = EosObj_Actor;
    eos.object[me->super.t_id].attribute = EOS_TASK_ATTRIBUTE_SM;
    me->state = eos_state_top;
}

void eos_sm_start(eos_sm_t *const me, eos_state_handler state_init)
{
    me->state = state_init;
    
    eos_event_give_(eos.object[me->super.t_id].key,
                    EOS_MAX_OBJECTS,
                    EosEventGiveType_Send, "Event_Null");

    eos_task_startup(&me->super);
}
#endif

static void eos_reactor_enter(eos_reactor_t *const me)
{
    eos_event_t e =
    {
        "Event_Enter", 0, 0,
    };
    me->event_handler(me, &e);
}

static void eos_sm_enter(eos_sm_t *const me)
{
#if (EOS_USE_HSM_MODE != 0)
    eos_state_handler path[EOS_MAX_HSM_NEST_DEPTH];
#endif
    eos_state_handler t;

    /* 进入初始状态，执行TRAN动作。这也意味着，进入初始状态，必须无条件执行Tran动作。 */
    t = me->state;
    eos_ret_t ret = t(me, &eos_event_table[Event_Null]);
    EOS_ASSERT(ret == EOS_Ret_Tran);
#if (EOS_USE_HSM_MODE == 0)
    ret = me->state(me, &eos_event_table[Event_Enter]);
    EOS_ASSERT(ret != EOS_Ret_Tran);
#else
    t = eos_state_top;
    /* 由初始状态转移，引发的各层状态的进入 */
    /* 每一个循环，都代表着一个Event_Init的执行 */
    eos_s32_t ip = 0;
    ret = EOS_Ret_Null;
    do
    {
        /* 由当前层，探测需要进入的各层父状态 */
        path[0] = me->state;
        /* 一层一层的探测，一直探测到原状态 */
        HSM_TRIG_(me->state, Event_Null);
        while (me->state != t)
        {
            ++ ip;
            EOS_ASSERT(ip < EOS_MAX_HSM_NEST_DEPTH);
            path[ip] = me->state;
            HSM_TRIG_(me->state, Event_Null);
        }
        me->state = path[0];

        /* Enter states in every layer. */
        do {
            HSM_TRIG_(path[ip --], Event_Enter);
        } while (ip >= 0);

        t = path[0];

        ret = HSM_TRIG_(t, Event_Init);
    } while (ret == EOS_Ret_Tran);

    me->state = t;
#endif
}

/* -----------------------------------------------------------------------------
Event
----------------------------------------------------------------------------- */
static eos_s8_t eos_event_give_(const char *task, eos_u32_t task_id,
                                eos_u8_t give_type,
                                const char *topic)
{
    eos_s8_t ret = 0;
    eos_sem_handle_t sem = EOS_NULL;
    register eos_base_t level;
    eos_u16_t e_id;
    eos_u8_t e_type;
    
    level = eos_hw_interrupt_disable();

    /* Get the task id in the object hash table. */
    eos_u16_t t_id;
    eos_task_handle_t tcb;
    if (give_type == EosEventGiveType_Send)
    {
        EOS_ASSERT((task != EOS_NULL && task_id == EOS_MAX_OBJECTS) ||
                   (task == EOS_NULL && task_id < EOS_MAX_OBJECTS));

        if (task_id == EOS_MAX_OBJECTS)
        {
            t_id = eos_hash_get_index(EosObj_Actor, task);
            EOS_ASSERT_NAME(t_id != EOS_MAX_OBJECTS, topic);
            EOS_ASSERT(eos.object[t_id].type == EosObj_Actor);
        }
        else
        {
            t_id = task_id;
        }
        
        tcb = eos.object[t_id].ocb.task.tcb;
        if (tcb->event_recv_disable == true)
        {
            goto exit;
        }
    }
    else if (give_type == EosEventGiveType_Publish)
    {
        EOS_ASSERT(task == EOS_NULL && task_id == EOS_MAX_OBJECTS);
    }
    else
    {
        EOS_ASSERT(0);
    }
    
    /* Get event id according to the event topic. */
    e_id = eos_hash_get_index(EosObj_Event, topic);
    if (e_id == EOS_MAX_OBJECTS)
    {
        /* Newly create one event in the hash table. */
        e_id = eos_hash_insert(EosObj_Event, topic);
        eos.object[e_id].type = EosObj_Event;
        eos.object[e_id].attribute &= (~0x03);
        e_type = EOS_EVENT_ATTRIBUTE_TOPIC;
    }
    else
    {
        /* Get the type of the event. */
        e_type = eos.object[e_id].attribute & 0x03;
        EOS_ASSERT(eos.object[e_id].type == EosObj_Event);
    }
    
    eos_owner_t g_owner;
    memset(&g_owner, 0, sizeof(eos_owner_t));

    /* The send-type event. */
    if (give_type == EosEventGiveType_Send)
    {
        /* If the current task is waiting for a specific event, but not the
           current event. */
        if (tcb->wait_specific_event == true &&
            strcmp(topic, tcb->event_wait) != 0)
        {
            goto exit;
        }
        g_owner.data[tcb->t_id / 8] = (1 << (tcb->t_id % 8));
    }
    /* The publish-type event. */
    else if (give_type == EosEventGiveType_Publish)
    {
        memcpy(&g_owner, &eos.object[e_id].ocb.event.e_sub, sizeof(eos_owner_t));
        
        /* The suspended task does not receive any event. */
        for (eos_u32_t i = 0; i < EOS_MAX_TASK_OCCUPY; i ++)
        {
            if (eos.obj_task_occupy[i] != 0)
            {
                for (eos_u8_t j = 0; j < 8; j ++)
                {
                    if ((eos.obj_task_occupy[i] & (1 << j)) != 0)
                    {
                        eos_u16_t _t_id = i * 8 + j;
                        eos_object_t *obj = &eos.object[_t_id];

                        if (obj->ocb.task.tcb->event_recv_disable == true)
                        {
                            owner_set_bit(&g_owner, _t_id, false);
                        }
                    }
                }
            }
        }
        if (owner_all_cleared(&g_owner) == true)
        {
            goto exit;
        }
    }

    /* If the event type is topic-type. */
    if (e_type == EOS_EVENT_ATTRIBUTE_TOPIC)
    {
        eos_event_data_t *data
            = eos_heap_malloc(&eos.heap, sizeof(eos_event_data_t));
        EOS_ASSERT(data != EOS_NULL);
        data->id = e_id;
        memset(&data->e_owner, 0, sizeof(eos_owner_t));
        owner_or(&data->e_owner, &g_owner);
        data->time = eos_tick_get_millisecond();

        /* Attach the event data to the event queue. */
        if (eos.e_queue == EOS_NULL)
        {
            eos.e_queue = data;
            data->next = EOS_NULL;
            data->last = EOS_NULL;
        }
        else
        {
            eos_event_data_t *edata = eos.e_queue;
            while (edata->next != EOS_NULL)
            {
                edata = edata->next;
            }
            data->next = EOS_NULL;
            edata->next = data;
            data->last = edata;
        }
    }
    /* If the event is value-type or stream-type. */
    else if (e_type == EOS_EVENT_ATTRIBUTE_VALUE ||
             e_type == EOS_EVENT_ATTRIBUTE_STREAM)
    {
        if (eos.object[e_id].ocb.event.e_item == EOS_NULL)
        {
            eos_event_data_t *data;

            /* Apply one data for the event. */
            data = eos_heap_malloc(&eos.heap, sizeof(eos_event_data_t));
            EOS_ASSERT_NAME(data != EOS_NULL, topic);
            memset(&data->e_owner, 0, sizeof(eos_owner_t));
            owner_or(&data->e_owner, &g_owner);
            data->id = e_id;
            data->time = eos_tick_get_millisecond();
            eos.object[e_id].ocb.event.e_item = data;
            
            /* Attach the event data to the event queue. */
            if (eos.e_queue == EOS_NULL)
            {
                eos.e_queue = data;
                data->next = EOS_NULL;
                data->last = EOS_NULL;
            }
            else
            {
                eos_event_data_t *edata = eos.e_queue;
                while (edata->next != EOS_NULL)
                {
                    edata = edata->next;
                }
                data->next = EOS_NULL;
                edata->next = data;
                data->last = edata;
            }
        }
        else
        {
            owner_or(&eos.object[e_id].ocb.event.e_item->e_owner, &g_owner);
            eos.object[e_id].ocb.event.e_item->time = eos_tick_get_millisecond();
        }
    }
    /* Event has no other type. */
    else
    {
        EOS_ASSERT(0);
    }

    /* Check if the related tasks are waiting for the specific event or not. */
    for (eos_u32_t i = 0; i < EOS_MAX_TASK_OCCUPY; i ++)
    {
        if (eos.obj_task_occupy[i] != 0)
        {
            for (eos_u8_t j = 0; j < 8; j ++)
            {
                if ((eos.obj_task_occupy[i] & (1 << j)) != 0)
                {
                    eos_u16_t _t_id = i * 8 + j;
                    eos_object_t *obj = &eos.object[_t_id];

                    if (eos_interrupt_get_nest() == 0)
                    {
                        if (owner_is_occupied(&g_owner, _t_id) &&
                            obj->ocb.task.tcb != eos_task_self())
                        {
                            if (!obj->ocb.task.tcb->wait_specific_event)
                            {
                                sem = &obj->ocb.task.tcb->sem;
                                eos_sem_release(sem);
                                eos_hw_interrupt_enable(level);
                                level = eos_hw_interrupt_disable();
                            }
                            else
                            {
                                if (strcmp(topic, obj->ocb.task.tcb->event_wait) == 0)
                                {
                                    sem = &obj->ocb.task.tcb->sem;
                                    eos_sem_release(sem);
                                    eos_hw_interrupt_enable(level);
                                    level = eos_hw_interrupt_disable();
                                }
                            }
                        }
                    }
                    else
                    {
                        if (owner_is_occupied(&g_owner, _t_id))
                        {
                            if (!obj->ocb.task.tcb->wait_specific_event)
                            {
                                sem = &obj->ocb.task.tcb->sem;
                                eos_sem_release(sem);
                            }
                            else
                            {
                                if (strcmp(topic, obj->ocb.task.tcb->event_wait) == 0)
                                {
                                    sem = &obj->ocb.task.tcb->sem;
                                    eos_sem_release(sem);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

exit:
    eos_hw_interrupt_enable(level);

    return ret;
}

eos_u32_t eos_get_task_id(const char *task)
{
    eos_u16_t t_id = eos_hash_get_index(EosObj_Actor, task);
    EOS_ASSERT_NAME(t_id != EOS_MAX_OBJECTS, task);
    EOS_ASSERT(eos.object[t_id].type == EosObj_Actor);

    return t_id;
}

void eos_event_send(const char *task, const char *topic)
{
    eos_event_give_(task, EOS_MAX_OBJECTS, EosEventGiveType_Send, topic);
}

void eos_event_send_id(eos_u32_t task_id, const char *topic)
{
    eos_event_give_(NULL, task_id, EosEventGiveType_Send, topic);
}

void eos_event_publish(const char *topic)
{
    eos_event_give_(EOS_NULL, EOS_MAX_OBJECTS, EosEventGiveType_Publish, topic);
}

static inline void eos_event_sub_(eos_task_handle_t const me, const char *topic)
{
    register eos_base_t level = eos_hw_interrupt_disable();

    /* Find the object by the event topic. */
    eos_u16_t index;
    index = eos_hash_get_index(EosObj_Event, topic);
    if (index == EOS_MAX_OBJECTS)
    {
        index = eos_hash_insert(EosObj_Event, topic);
        eos.object[index].type = EosObj_Event;
        eos.object[index].attribute &=~ EOS_EVENT_ATTRIBUTE_MASK;
    }
    else
    {
        EOS_ASSERT(eos.object[index].type == EosObj_Event);

        /* The stream event can only be subscribed by one task. */
        eos_u8_t e_type = eos.object[index].attribute & 0x03;
        if (e_type == EOS_EVENT_ATTRIBUTE_STREAM)
        {
            EOS_ASSERT(owner_all_cleared(&eos.object[index].ocb.event.e_sub));
        }
    }

    /* Write the subscribing information into the object data. */
    owner_set_bit(&eos.object[index].ocb.event.e_sub,
                    eos_task_self()->t_id,
                    true);

    eos_hw_interrupt_enable(level);
}

void eos_event_sub(const char *topic)
{
    eos_event_sub_(eos_task_self(), topic);
}

void eos_event_unsub(const char *topic)
{
    register eos_base_t level = eos_hw_interrupt_disable();

    /* Find the matching object by the topic. */
    eos_u16_t index = eos_hash_get_index(EosObj_Event, topic);
    EOS_ASSERT(index != EosObj_Event);
    EOS_ASSERT(eos.object[index].type == EosObj_Event);
    EOS_ASSERT((eos.object[index].attribute & 0x03) != EOS_EVENT_ATTRIBUTE_STREAM);

    /* Clear the subscirbe flag. */
    owner_set_bit(&eos.object[index].ocb.event.e_sub,
                  eos_task_self()->t_id,
                  false);

    eos_hw_interrupt_enable(level);
}

static void timer_func_timeout(void *parameter)
{
    eos_object_t *obj = (eos_object_t *)parameter;

    if (obj->ocb.timer.target != EOS_NULL)
    {
        eos_u16_t t_id = obj->ocb.timer.target->t_id;
        const char *task = eos.object[t_id].key;
        eos_event_send(task, obj->key);
    }
    else
    {
        eos_event_publish(obj->key);
    }
}

#if (EOS_USE_TIME_EVENT != 0)
static void eos_event_time(const char *task,
                            const char *topic,
                            eos_u32_t time_ms, bool oneshoot)
{
    /* Get event id according the topic. */
    eos_u16_t t_id;
    if (task != EOS_NULL)
    {
        t_id = eos_hash_get_index(EosObj_Actor, task);
        EOS_ASSERT(t_id != EOS_MAX_OBJECTS);
    }

    /* Ensure the event is topic-type. */
    eos_u16_t e_id = eos_hash_get_index(EosObj_Event, topic);
    eos_u8_t e_type;
    if (e_id == EOS_MAX_OBJECTS)
    {
        e_id = eos_hash_insert(EosObj_Event, topic);
        eos.object[e_id].type = EosObj_Event;
        eos.object[e_id].attribute = EOS_EVENT_ATTRIBUTE_TOPIC;
    }
    else
    {
        EOS_ASSERT(eos.object[e_id].type == EosObj_Event);

        /* The stream event can only be subscribed by one task. */
        e_type = eos.object[e_id].attribute & EOS_EVENT_ATTRIBUTE_MASK;
        EOS_ASSERT(e_type == EOS_EVENT_ATTRIBUTE_TOPIC);
    }

    /* Timer ID */
    eos_u16_t tim_id = eos_hash_get_index(EosObj_Timer, topic);
    EOS_ASSERT(tim_id == EOS_MAX_OBJECTS);
    tim_id = eos_hash_insert(EosObj_Timer, topic);
    eos.object[tim_id].type = EosObj_Timer;
    eos.object[tim_id].attribute = 0;
    eos.object[tim_id].attribute |= EOS_TIMER_ATTRIBUTE_SEND;
    if (task != EOS_NULL)
    {
        eos.object[tim_id].ocb.timer.target = eos.object[t_id].ocb.task.tcb;
    }
    else
    {
        eos.object[tim_id].ocb.timer.target = EOS_NULL;
    }

    /* Initialize the timer. */
    eos_timer_t *timer = &eos.object[tim_id].ocb.timer.timer;
    eos_u8_t flag_timer;
    if (oneshoot)
    {
        flag_timer = EOS_TIMER_FLAG_ONE_SHOT;
    }
    else
    {
        flag_timer = EOS_TIMER_FLAG_PERIODIC;
    }
    eos_timer_init(timer, topic,
                    timer_func_timeout,
                    &eos.object[tim_id],
                    time_ms, flag_timer);
    eos_timer_start(timer);
}

void eos_event_publish_delay(const char *topic, eos_u32_t time_ms)
{
    eos_event_time(EOS_NULL, topic, time_ms, true);
}

void eos_event_publish_period(const char *topic, eos_u32_t time_ms_period)
{
    eos_event_time(EOS_NULL, topic, time_ms_period, false);
}

void eos_event_send_delay(const char *task,
                          const char *topic, eos_u32_t time_delay_ms)
{
    eos_event_time(task, topic, time_delay_ms, true);
}

void eos_event_send_period(const char *task,
                           const char *topic, eos_u32_t time_period_ms)
{
    eos_event_time(task, topic, time_period_ms, false);
}

void eos_event_time_cancel(const char *topic)
{
    /* Timer ID */
    eos_u16_t tim_id = eos_hash_get_index(EosObj_Timer, topic);
    EOS_ASSERT(tim_id != EOS_MAX_OBJECTS);

    /* Initialize the timer. */
    eos_timer_t *timer = &eos.object[tim_id].ocb.timer.timer;
    eos.object[tim_id].key = EOS_NULL;
    eos_timer_stop(timer);
}
#endif

bool eos_event_topic(eos_event_t const * const e, const char *topic)
{
    if (strcmp(e->topic, topic) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/* -----------------------------------------------------------------------------
Database
----------------------------------------------------------------------------- */
void eos_db_init(void *const memory, eos_u32_t size)
{
    eos_heap_init(&eos.db, memory, size);
}

eos_u8_t eos_db_get_attribute(const char *key)
{
    /* Get event id according the topic. */
    eos_u16_t e_id = eos_hash_get_index(EosObj_Event, key);
    EOS_ASSERT(e_id != EOS_MAX_OBJECTS);
    EOS_ASSERT(eos.object[e_id].type == EosObj_Event);
    
    /* Return the key's attribute. */
    return eos.object[e_id].attribute;
}

void eos_db_set_attribute(const char *key, eos_u8_t attribute)
{
    /* Check the argument. */
    eos_u8_t temp8 = EOS_EVENT_ATTRIBUTE_VALUE | EOS_EVENT_ATTRIBUTE_STREAM;
    EOS_ASSERT((attribute & temp8) != temp8);

    /* Get event id according the topic. */
    eos_u16_t e_id = eos_hash_get_index(EosObj_Event, key);
    EOS_ASSERT(e_id != EOS_MAX_OBJECTS);
    EOS_ASSERT(eos.object[e_id].type == EosObj_Event);
    
    /* Set the key's attribute. */
    eos.object[e_id].attribute = attribute;
}

void eos_db_register(const char *key, eos_u32_t size, eos_u8_t attribute)
{
    /* Check the argument. */
    eos_u8_t temp8 = EOS_EVENT_ATTRIBUTE_VALUE | EOS_EVENT_ATTRIBUTE_STREAM;
    EOS_ASSERT((attribute & temp8) != temp8);

    /* Check the event key's attribute. */
    register eos_base_t level = eos_hw_interrupt_disable();
    eos_u16_t e_id = eos_hash_get_index(EosObj_Event, key);
    if (e_id == EOS_MAX_OBJECTS)
    {
        e_id = eos_hash_insert(EosObj_Event, key);
        eos.object[e_id].type = EosObj_Event;
    }
    EOS_ASSERT(eos.object[e_id].type == EosObj_Event);
    /* The event's type convertion must be topic -> value or topic -> stream. */
    EOS_ASSERT((eos.object[e_id].attribute & temp8) == (attribute & temp8) ||
               (eos.object[e_id].attribute & temp8) == 0);

    /* Apply the the memory for event. */
    eos.object[e_id].attribute = attribute;
    if ((attribute & EOS_DB_ATTRIBUTE_VALUE) != 0)
    {
        /* Apply a memory for the db key. */
        void *data = eos_heap_malloc(&eos.db, size);
        EOS_ASSERT(data != EOS_NULL);

        eos.object[e_id].data.value = data;
        eos.object[e_id].size = size;
    }
    else if ((attribute & EOS_DB_ATTRIBUTE_STREAM) != 0)
    {
        /* Apply a memory for the db key. */
        void *data = eos_heap_malloc(&eos.db, (size + sizeof(eos_stream_t)));
        EOS_ASSERT(data != EOS_NULL);

        eos.object[e_id].data.stream = (eos_stream_t *)data;
        eos.object[e_id].size = size;

        eos_stream_init(eos.object[e_id].data.stream,
                        (void *)((eos_u32_t)data + sizeof(eos_stream_t)),
                        eos.object[e_id].size);

        eos_owner_t *e_sub = &eos.object[e_id].ocb.event.e_sub;
        memset(e_sub, 0, sizeof(eos_owner_t));
    }

    eos_hw_interrupt_enable(level);
}

void eos_db_block_read(const char *key, void * const data)
{
    eos_db_read_(EOS_DB_ATTRIBUTE_VALUE, key, data, 0);
}

void eos_db_block_write(const char *key, void * const data)
{
    eos_db_write_(EOS_DB_ATTRIBUTE_VALUE, key, data, 0);
}

eos_s32_t eos_db_stream_read(const char *key, void *const buffer, eos_u32_t size)
{
    return eos_db_read_(EOS_DB_ATTRIBUTE_STREAM, key, buffer, size);
}

void eos_db_stream_write(const char *key, void *const buffer, eos_u32_t size)
{
    eos_db_write_(EOS_DB_ATTRIBUTE_STREAM, key, buffer, size);
}

/* private db function ------------------------------------------------------ */
eos_inline void eos_db_write_(eos_u8_t type, const char *key, 
                                const void *memory, eos_u32_t size)
{
    /* If in interrupt service function, disable the interrupt. */
    register eos_base_t level = eos_hw_interrupt_disable();

    /* Get event id according the topic. */
    eos_u16_t e_id = eos_hash_get_index(EosObj_Event, key);
    EOS_ASSERT(e_id != EOS_MAX_OBJECTS);
    EOS_ASSERT(eos.object[e_id].type == EosObj_Event);
    eos_u8_t attribute = eos.object[e_id].attribute;
    EOS_ASSERT((attribute & type) != 0);
    eos_u32_t bits = (1 << eos_task_get_priority(eos_task_self()));

    eos_u32_t size_remain;
    /* Value type event key. */
    if (type == EOS_EVENT_ATTRIBUTE_VALUE)
    {
        /* Update the event's value. */
        for (eos_u32_t i = 0; i < eos.object[e_id].size; i ++)
        {
            ((eos_u8_t *)(eos.object[e_id].data.value))[i] = ((eos_u8_t *)memory)[i];
        }
    }
    /* Stream type event key. */
    else if (type == EOS_EVENT_ATTRIBUTE_STREAM)
    {
        /* Check if the remaining memory is enough or not. */
        eos_stream_t *queue = eos.object[e_id].data.stream;
        size_remain = eos_stream_empty_size(queue);
        EOS_ASSERT(size_remain >= size);
        /* Push all data to the queue. */
        eos_stream_push(queue, (void *)memory, size);
    }

    eos_hw_interrupt_enable(level);
}

eos_inline eos_s32_t eos_db_read_(eos_u8_t type,
                                    const char *key, 
                                    const void *memory, eos_u32_t size)
{
    /* If in interrupt service function, disable the interrupt. */
    register eos_base_t level = eos_hw_interrupt_disable();

    /* Get event id according the topic. */
    eos_u16_t e_id = eos_hash_get_index(EosObj_Event, key);
    EOS_ASSERT(e_id != EOS_MAX_OBJECTS);
    EOS_ASSERT(eos.object[e_id].type == EosObj_Event);
    eos_u8_t attribute = eos.object[e_id].attribute;
    EOS_ASSERT((attribute & type) != 0);
    eos_u32_t bits = (1 << eos_task_get_priority(eos_task_self()));

    /* Value type. */
    eos_s32_t ret_size = 0;
    if (type == EOS_EVENT_ATTRIBUTE_VALUE)
    {
        /* Update the event's value. */
        for (eos_u32_t i = 0; i < eos.object[e_id].size; i ++)
        {
            ((eos_u8_t *)memory)[i] =
                ((eos_u8_t *)(eos.object[e_id].data.value))[i];
        }

        ret_size = size;
    }
    /* Stream type. */
    else if (type == EOS_EVENT_ATTRIBUTE_STREAM)
    {
        /* Check if the remaining memory is enough or not. */
        eos_stream_t *queue = eos.object[e_id].data.stream;
        /* Push all data to the queue. */
        ret_size = eos_stream_pull_pop(queue, (void *)memory, size);
    }

    eos_hw_interrupt_enable(level);

    return ret_size;
}

/* private sm function ------------------------------------------------------ */
#if (EOS_USE_SM_MODE != 0)
eos_ret_t eos_tran(eos_sm_t *const me, eos_state_handler state)
{
    me->state = state;

    return EOS_Ret_Tran;
}

eos_ret_t eos_super(eos_sm_t *const me, eos_state_handler state)
{
    me->state = state;

    return EOS_Ret_Super;
}

eos_ret_t eos_state_top(eos_sm_t *const me, eos_event_t const * const e)
{
    (void)me;
    (void)e;

    return EOS_Ret_Null;
}
#endif

#if (EOS_USE_SM_MODE != 0)
static void eos_sm_dispath(eos_sm_t *const me, eos_event_t const * const e)
{
#if (EOS_USE_HSM_MODE != 0)
    eos_state_handler path[EOS_MAX_HSM_NEST_DEPTH];
#endif
    eos_ret_t r;

    EOS_ASSERT(e != (eos_event_t *)0);

#if (EOS_USE_HSM_MODE == 0)
    eos_state_handler s = me->state;
    eos_state_handler t;
    
    r = s(me, e);
    if (r == EOS_Ret_Tran)
    {
        t = me->state;
        r = s(me, &eos_event_table[Event_Exit]);
        EOS_ASSERT(r == EOS_Ret_Handled || r == EOS_Ret_Super);
        r = t(me, &eos_event_table[Event_Enter]);
        EOS_ASSERT(r == EOS_Ret_Handled || r == EOS_Ret_Super);
        me->state = t;
    }
    else
    {
        me->state = s;
    }
#else
    eos_state_handler t = me->state;
    eos_state_handler s;

    do {
        s = me->state;
        r = (*s)(me, e);
    } while (r == EOS_Ret_Super);

    if (r != EOS_Ret_Tran)
    {
        me->state = t;
        return;
    }

    path[0] = me->state;
    path[1] = t;
    path[2] = s;

    /* exit current state to transition source s... */
    while (t != s)
    {
        /* exit handled? */
        if (HSM_TRIG_(t, Event_Exit) == EOS_Ret_Handled)
        {
            (void)HSM_TRIG_(t, Event_Null); /* find superstate of t */
        }
        t = me->state; /* stateTgt_ holds the superstate */
    }

    eos_s32_t ip = eos_sm_tran(me, path); /* take the HSM transition */

    /* retrace the entry path in reverse (desired) order... */
    for (; ip >= 0; --ip)
    {
        HSM_TRIG_(path[ip], Event_Enter); /* enter path[ip] */
    }
    t = path[0];    /* stick the target into register */
    me->state = t; /* update the next state */

    while (HSM_TRIG_(t, Event_Init) == EOS_Ret_Tran)
    {
        ip = 0;
        path[0] = me->state;
        (void)HSM_TRIG_(me->state, Event_Null);
        while (me->state != t)
        {
            ip ++;
            path[ip] = me->state;
            (void)HSM_TRIG_(me->state, Event_Null);
        }
        me->state = path[0];

        EOS_ASSERT(ip < EOS_MAX_HSM_NEST_DEPTH);

        /* retrace the entry path in reverse (correct) order... */
        do {
            HSM_TRIG_(path[ip --], Event_Enter);
        } while (ip >= 0);

        t = path[0];
    }

    me->state = t;
#endif
}

#if (EOS_USE_HSM_MODE != 0)
static eos_s32_t eos_sm_tran(eos_sm_t *const me,
                           eos_state_handler path[EOS_MAX_HSM_NEST_DEPTH])
{
    /* transition entry path index */
    eos_s32_t ip = -1;
    eos_s32_t iq; /* helper transition entry path index */
    eos_state_handler t = path[0];
    eos_state_handler s = path[2];
    eos_ret_t r;

    /* (a) 跳转到自身 s == t */
    if (s == t)
    {
        HSM_TRIG_(s, Event_Exit);  /* exit the source */
        return 0; /* cause entering the target */
    }

    (void)HSM_TRIG_(t, Event_Null); /* superstate of target */
    t = me->state;

    /* (b) check source == target->super */
    if (s == t)
        return 0; /* cause entering the target */

    (void)HSM_TRIG_(s, Event_Null); /* superstate of src */

    /* (c) check source->super == target->super */
    if (me->state == t)
    {
        HSM_TRIG_(s, Event_Exit);  /* exit the source */
        return 0; /* cause entering the target */
    }

    /* (d) check source->super == target */
    if (me->state == path[0])
    {
        HSM_TRIG_(s, Event_Exit); /* exit the source */
        return -1;
    }

    /* (e) check rest of source == target->super->super.. */
    /* and store the entry path along the way */

    /* indicate that the LCA was not found */
    iq = 0;

    /* enter target and its superstate */
    ip = 1;
    path[1] = t; /* save the superstate of target */
    t = me->state; /* save source->super */

    /* find target->super->super */
    r = HSM_TRIG_(path[1], Event_Null);
    while (r == EOS_Ret_Super)
    {
        ++ ip;
        path[ip] = me->state; /* store the entry path */
        if (me->state == s)
        { /* is it the source? */
            /* indicate that the LCA was found */
            iq = 1;

            /* entry path must not overflow */
            EOS_ASSERT(ip < EOS_MAX_HSM_NEST_DEPTH);
            --ip;  /* do not enter the source */
            r = EOS_Ret_Handled; /* terminate the loop */
        }
        /* it is not the source, keep going up */
        else
            r = HSM_TRIG_(me->state, Event_Null);
    }

    /* LCA found yet? */
    if (iq == 0)
    {
        /* entry path must not overflow */
        EOS_ASSERT(ip < EOS_MAX_HSM_NEST_DEPTH);

        HSM_TRIG_(s, Event_Exit); /* exit the source */

        /* (f) check the rest of source->super */
        /*                  == target->super->super... */
        iq = ip;
        r = EOS_Ret_Null; /* indicate LCA NOT found */
        do {
            /* is this the LCA? */
            if (t == path[iq])
            {
                r = EOS_Ret_Handled; /* indicate LCA found */
                /* do not enter LCA */
                ip = iq - 1;
                /* cause termination of the loop */
                iq = -1;
            }
            else
                -- iq; /* try lower superstate of target */
        } while (iq >= 0);

        /* LCA not found yet? */
        if (r != EOS_Ret_Handled)
        {
            /* (g) check each source->super->... */
            /* for each target->super... */
            r = EOS_Ret_Null; /* keep looping */
            do {
                /* exit t unhandled? */
                if (HSM_TRIG_(t, Event_Exit) == EOS_Ret_Handled)
                {
                    (void)HSM_TRIG_(t, Event_Null);
                }
                t = me->state; /*  set to super of t */
                iq = ip;
                do {
                    /* is this LCA? */
                    if (t == path[iq])
                    {
                        /* do not enter LCA */
                        ip = iq - 1;
                        /* break out of inner loop */
                        iq = -1;
                        r = EOS_Ret_Handled; /* break outer loop */
                    }
                    else
                        --iq;
                } while (iq >= 0);
            } while (r != EOS_Ret_Handled);
        }
    }

    return ip;
}
#endif
#endif

/* private heap function ---------------------------------------------------- */
void eos_heap_init(eos_heap_t *const me, void *data, eos_u32_t size)
{
    EOS_ASSERT(data != EOS_NULL);
    EOS_ASSERT(size > sizeof(eos_heap_block_t));

    eos_u32_t mod = ((eos_u32_t)data % 4);
    if (mod != 0)
    {
        data = (void *)((eos_u32_t)data + 4 - mod);
        size = size - 4;
    }
    
    /* block start */
    me->data = data;
    me->list = (eos_heap_block_t *)me->data;
    me->size = size;
    me->error_id = 0;

    /* the 1st free block */
    eos_heap_block_t *block_1st;
    block_1st = (eos_heap_block_t *)me->data;
    block_1st->next = EOS_NULL;
    block_1st->size = size - (eos_u32_t)sizeof(eos_heap_block_t);
    block_1st->is_free = 1;
}

void * eos_heap_malloc(eos_heap_t *const me, eos_u32_t size)
{
    eos_heap_block_t *block;
    
    if (size == 0)
    {
        me->error_id = 1;
        return EOS_NULL;
    }

    eos_u32_t mod = (size % 4);
    if (mod != 0)
    {
        size = size + 4 - mod;
    }

    /* Find the first free block in the block-list. */
    block = (eos_heap_block_t *)me->list;
    do
    {
        if (block->is_free == 1 &&
            block->size > (size + sizeof(eos_heap_block_t)))
        {
            break;
        }
        else
        {
            block = block->next;
        }
    } while (block != EOS_NULL);

    if (block == EOS_NULL)
    {
        me->error_id = 2;
        return EOS_NULL;
    }

    /* If the block's byte number is NOT enough to create a new block. */
    if (block->size <= (eos_u32_t)((eos_u32_t)size + sizeof(eos_heap_block_t)))
    {
        block->is_free = 0;
    }
    /* Divide the block into two blocks. */
    else
    {
        eos_heap_block_t *new_block
            = (eos_heap_block_t *)((eos_u32_t)block + size + sizeof(eos_heap_block_t));
        new_block->size = block->size - size - sizeof(eos_heap_block_t);
        new_block->is_free = 1;
        new_block->next = EOS_NULL;

        /* Update the list. */
        new_block->next = block->next;
        block->next = new_block;
        block->size = size;
        block->is_free = 0;
    }

    me->error_id = 0;

    return (void *)((eos_u32_t)block + (eos_u32_t)sizeof(eos_heap_block_t));
}

void eos_heap_free(eos_heap_t *const me, void * data)
{
    eos_heap_block_t *block_crt =
        (eos_heap_block_t *)((eos_u32_t)data - sizeof(eos_heap_block_t));

    /* Search for this block in the block-list. */
    eos_heap_block_t *block = me->list;
    eos_heap_block_t *block_last = EOS_NULL;
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
    } while (block != EOS_NULL);

    /* Not found. */
    if (block == EOS_NULL)
    {
        me->error_id = 4;
        return;
    }

    block->is_free = 1;
    /* Check the block can be combined with the front one. */
    if (block_last != (eos_heap_block_t *)NULL && block_last->is_free == 1)
    {
        block_last->size += (block->size + sizeof(eos_heap_block_t));
        block_last->next = block->next;
        block = block_last;
    }

    /* Check the block can be combined with the later one. */
    if (block->next != (eos_heap_block_t *)EOS_NULL && block->next->is_free == 1)
    {
        block->size += (block->next->size + (eos_u32_t)sizeof(eos_heap_block_t));
        block->next = block->next->next;
        block->is_free = 1;
    }

    me->error_id = 0;
}

/* private hash function ---------------------------------------------------- */
static char ch_type[EosObj_Max] = { 'A', 'E', 'T' };

static eos_u32_t eos_hash_time33(char ch_type, const char *string)
{
    eos_u32_t hash = 5381;
    hash += (hash << 5) + ch_type;
    while (*string)
    {
        hash += (hash << 5) + (*string ++);
    }

    return (eos_u32_t)(hash & (0x7fffffff));
}

static eos_u16_t eos_hash_insert(eos_u8_t obj_type, const char *string)
{
    eos_u16_t index = 0;

    /* Calculate the hash value of the string. */
    eos_u32_t hash = eos_hash_time33(ch_type[obj_type], string);
    eos_u16_t index_init = hash % eos.prime_max;

    for (eos_u16_t i = 0; i < (EOS_MAX_OBJECTS / 2 + 1); i ++)
    {
        for (eos_s8_t j = -1; j <= 1; j += 2)
        {
            index = index_init + i * j + 2 * (eos_s16_t)EOS_MAX_OBJECTS;
            index %= EOS_MAX_OBJECTS;

            /* Find the empty object. */
            if (eos.object[index].key == (const char *)0)
            {
                eos.object[index].key = string;
                goto exit;
            }
            if (strcmp(eos.object[index].key, string) == 0 &&
                eos.object[index].type == obj_type)
            {
                goto exit;
            }
        }

        /* Ensure the finding speed is not slow. */
        if (i >= EOS_MAX_HASH_SEEK_TIMES)
        {
            /* If this assert is trigged, you need to enlarge the hash table size. */
            EOS_ASSERT(0);
        }
    }

exit:
    return index;
}

static eos_u16_t eos_hash_get_index(eos_u8_t obj_type, const char *string)
{
    eos_u16_t index = 0;

    /* Calculate the hash value of the string. */
    eos_u32_t hash = eos_hash_time33(ch_type[obj_type], string);
    eos_u16_t index_init = hash % eos.prime_max;

    for (eos_u16_t i = 0; i < (EOS_MAX_OBJECTS / 2 + 1); i ++)
    {
        for (eos_s8_t j = -1; j <= 1; j += 2)
        {
            index = index_init + i * j + 2 * (eos_s16_t)EOS_MAX_OBJECTS;
            index %= EOS_MAX_OBJECTS;

            if (eos.object[index].key != (const char *)0 &&
                eos.object[index].type == obj_type &&
                strcmp(eos.object[index].key, string) == 0)
            {
                goto exit;
            }
        }

        /* Ensure the finding speed is not slow. */
        if (i >= EOS_MAX_HASH_SEEK_TIMES)
        {
            index = EOS_MAX_OBJECTS;
            goto exit;
        }
    }

exit:
    return index;
}

static bool eos_hash_existed(eos_u8_t obj_type, const char *string)
{
    bool ret = false;

    /* Calculate the hash value of the string. */
    eos_u32_t hash = eos_hash_time33(ch_type[obj_type], string);
    eos_u16_t index_init = hash % eos.prime_max;

    for (eos_u16_t i = 0; i < (EOS_MAX_OBJECTS / 2 + 1); i ++)
    {
        for (eos_s8_t j = -1; j <= 1; j += 2)
        {
            eos_u16_t index = index_init + i * j + 2 * (eos_s16_t)EOS_MAX_OBJECTS;
            index %= EOS_MAX_OBJECTS;

            if (eos.object[index].key != (const char *)0 &&
                eos.object[index].type == obj_type &&
                strcmp(eos.object[index].key, string) == 0)
            {
                ret = true;
                goto exit;
            }
        }

        /* Ensure the finding speed is not slow. */
        if (i >= EOS_MAX_HASH_SEEK_TIMES)
        {
            goto exit;
        }
    }

exit:
    return ret;
}

/* private stream function -------------------------------------------------- */
static eos_s32_t eos_stream_init(eos_stream_t *const me, void *memory, eos_u32_t capacity)
{
    me->data = memory;
    me->capacity = capacity;
    me->head = 0;
    me->tail = 0;
    me->empty = true;

    return Stream_OK;
}

static eos_s32_t eos_stream_push(eos_stream_t *const me, void * data, eos_u32_t size)
{
    if (eos_stream_full(me))
    {
        return Stream_Full;
    }
    if (eos_stream_empty_size(me) < size)
    {
        return Stream_NotEnough;
    }

    eos_u8_t *stream = (eos_u8_t *)me->data;
    for (int i = 0; i < size; i ++)
    {
        stream[me->head] = ((eos_u8_t *)data)[i];
        me->head = ((me->head + 1) % me->capacity);
    }
    me->empty = false;

    return Stream_OK;
}

static eos_s32_t eos_stream_pull_pop(eos_stream_t *const me, void * data, eos_u32_t size)
{
    if (me->empty)
    {
        return 0;
    }

    eos_u32_t size_stream = eos_stream_size(me);
    size = (size_stream < size) ? size_stream : size;

    eos_u8_t *stream = (eos_u8_t *)me->data;
    for (int i = 0; i < size; i ++)
    {
        ((eos_u8_t *)data)[i] = stream[me->tail];
        me->tail = (me->tail + 1) % me->capacity;
    }
    
    if (me->tail == me->head)
    {
        me->tail = 0;
        me->head = 0;
        me->empty = true;
    }

    return size;
}

static bool eos_stream_full(eos_stream_t *const me)
{
    eos_s32_t size = me->head - me->tail;
    if (size < 0)
    {
        size += me->capacity;
    }

    return (size == 0 && me->empty == false) ? true : false;
}

static eos_s32_t eos_stream_size(eos_stream_t *const me)
{
    if (me->empty == true)
    {
        return 0;
    }

    eos_s32_t size = me->head - me->tail;
    if (size <= 0)
    {
        size += me->capacity;
    }

    return size;
}

static eos_s32_t eos_stream_empty_size(eos_stream_t *const me)
{
    return me->capacity - eos_stream_size(me);
}

/* private owner function --------------------------------------------------- */
static inline bool owner_is_occupied(eos_owner_t *owner, eos_u32_t t_id)
{
    if (owner->data[t_id >> 3] & (1 << (t_id % 8)))
    {
        return true;
    }
    else
    {
        return false;
    }
}

static inline void owner_or(eos_owner_t *g_owner, eos_owner_t *owner)
{
    for (eos_u32_t i = 0; i < EOS_MAX_OWNER; i ++)
    {
        g_owner->data[i] |= owner->data[i];
    }
}

static inline void owner_set_bit(eos_owner_t *owner, eos_u32_t t_id, bool status)
{
    if (status == true)
    {
        owner->data[t_id >> 3] |= (1 << (t_id % 8));
    }
    else
    {
        owner->data[t_id >> 3] &= ~(1 << (t_id % 8));
    }
}

static inline bool owner_all_cleared(eos_owner_t *owner)
{
    bool all_cleared = true;
    for (eos_u32_t i = 0; i < EOS_MAX_OWNER; i ++)
    {
        if (owner->data[i] != 0)
        {
            all_cleared = false;
            break;
        }
    }

    return all_cleared;
}

#ifdef __cplusplus
}
#endif
