
/*
 * EventOS V0.2.0
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
 */

#ifndef EVENTOS_H_
#define EVENTOS_H_

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
EventOS Default Configuration
----------------------------------------------------------------------------- */
#include "eos_config.h"

#ifndef EOS_MAX_PRIORITY
#define EOS_MAX_PRIORITY                        8
#endif

#ifndef EOS_MAX_OBJECTS
#define EOS_MAX_OBJECTS                         256
#endif

#ifndef EOS_USE_ASSERT
#define EOS_USE_ASSERT                          1
#endif

#ifndef EOS_USE_SM_MODE
#define EOS_USE_SM_MODE                         0
#endif

#ifndef EOS_USE_PUB_SUB
#define EOS_USE_PUB_SUB                         0
#endif

#ifndef EOS_USE_TIME_EVENT
#define EOS_USE_TIME_EVENT                      0
#endif

#ifndef EOS_USE_EVENT_DATA
#define EOS_USE_EVENT_DATA                      0
#endif

#ifndef EOS_USE_EVENT_BRIDGE
#define EOS_USE_EVENT_BRIDGE                    0
#endif

/* -----------------------------------------------------------------------------
Includes
----------------------------------------------------------------------------- */
#include <stdbool.h>
#include "eos_def.h"
#include "eos_kernel.h"

/* -----------------------------------------------------------------------------
EventOS
----------------------------------------------------------------------------- */
void eos_init(void);
void eos_kernel_start(void);
void eos_task_idle_init(void);
void eos_enter_critical(void);
void eos_exit_critical(void);
eos_u16_t eos_critical_level(void);
void eos_system_timer_init(void);
void eos_system_timer_task_init(void);
eos_u32_t eos_tick_get(void);
void eos_tick_set(eos_u32_t tick);
void eos_tick_increase(void);
eos_u32_t eos_tick_get_ms(void);
void eos_interrupt_enter(void);
void eos_interrupt_leave(void);
eos_u8_t eos_interrupt_get_nest(void);
eos_base_t eos_hw_interrupt_disable(void);
void eos_hw_interrupt_enable(eos_base_t level);

/* -----------------------------------------------------------------------------
Task
----------------------------------------------------------------------------- */
/**
 * clock & timer macros
 */
#define EOS_TIMER_FLAG_DEACTIVATED       0x0             /**< timer is deactive */
#define EOS_TIMER_FLAG_ACTIVATED         0x1             /**< timer is active */
#define EOS_TIMER_FLAG_ONE_SHOT          0x0             /**< one shot timer */
#define EOS_TIMER_FLAG_PERIODIC          0x2             /**< periodic timer */

#define EOS_TIMER_FLAG_HARD_TIMER        0x0             /**< hard timer,the timer's callback function will be called in tick isr. */
#define EOS_TIMER_FLAG_SOFT_TIMER        0x4             /**< soft timer,the timer's callback function will be called in timer task. */

/* 1 or 3 */
#ifndef EOS_TIMER_SKIP_LIST_MASK
#define EOS_TIMER_SKIP_LIST_MASK         0x3
#endif

typedef struct eos_semaphore
{
#if (EOS_USE_3RD_KERNEL == 0)
    ek_sem_t sem;
#else
    eos_u32_t sem;
#endif
} eos_sem_t;

typedef struct eos_semaphore *eos_sem_handle_t;

/**
 * timer structure
 */
typedef struct eos_timer
{
#if (EOS_USE_3RD_KERNEL == 0)
    ek_timer_t timer;
#endif

    eos_u32_t timer_handle;
} eos_timer_t;

typedef struct eos_timer *eos_timer_handle_t;

/**
 * task control command definitions
 */
#define EOS_TASK_CTRL_STARTUP          0x00                /**< Startup task. */
#define EOS_TASK_CTRL_CLOSE            0x01                /**< Close task. */
#define EOS_TASK_CTRL_CHANGE_PRIORITY  0x02                /**< Change task priority. */
#define EOS_TASK_CTRL_INFO             0x03                /**< Get task information. */
#define EOS_TASK_CTRL_BIND_CPU         0x04                /**< Set task bind cpu. */

typedef struct eos_task
{
#if (EOS_USE_3RD_KERNEL == 0)
    ek_task_t task_;
#endif

    eos_u32_t task_handle;
    eos_sem_t sem;
    eos_u16_t index;
    bool event_recv_disable;
    bool wait_specific_event;
    const char *event_wait;
} eos_task_t;

typedef eos_task_t *eos_task_handle_t;

/*
 * Definition of the event class.
 */
typedef struct eos_event
{
    const char *topic;                      // The event topic.
    eos_u32_t eid                    : 16;   // The event ID.
    eos_u32_t size                   : 16;   // The event content's size.
} eos_event_t;

/*
 * task interface
 */
eos_err_t eos_task_init(eos_task_t *task,
                        const char *name,
                        void (*entry)(void *parameter),
                        void *parameter,
                        void *stack_start,
                        eos_u32_t stack_size,
                        eos_u8_t priority);
eos_err_t eos_task_detach(eos_task_handle_t task);
eos_err_t eos_task_exit(void);
eos_task_handle_t eos_task_self(void);
eos_err_t eos_task_startup(eos_task_handle_t task);
eos_err_t eos_task_close(eos_task_handle_t task);
eos_err_t eos_task_yield(void);
eos_err_t eos_task_delay(eos_u32_t tick);
eos_err_t eos_task_delay_until(eos_u32_t *tick, eos_u32_t inc_tick);
eos_err_t eos_task_delay_ms(eos_s32_t ms);
eos_err_t eos_task_delay_no_event(eos_u32_t tick);
eos_err_t eos_task_suspend(eos_task_handle_t task);
eos_err_t eos_task_resume(eos_task_handle_t task);
eos_task_state_t eos_task_get_state(eos_task_handle_t task);
eos_err_t eos_task_set_priority(eos_task_handle_t task, eos_u8_t priority);
eos_u8_t eos_task_get_priority(eos_task_handle_t task);
bool eos_task_wait_specific_event(eos_event_t * const e_out,
                                    const char *topic, eos_s32_t time_ms);
bool eos_task_wait_event(eos_event_t * const e_out, eos_s32_t time_ms);

/* -----------------------------------------------------------------------------
Timer
----------------------------------------------------------------------------- */
void eos_timer_init(eos_timer_handle_t timer,
                    void (*timeout)(void *parameter),
                    void *parameter,
                    eos_u32_t time,
                    eos_u8_t flag);
eos_err_t eos_timer_detach(eos_timer_handle_t timer);
eos_err_t eos_timer_start(eos_timer_handle_t timer);
eos_err_t eos_timer_stop(eos_timer_handle_t timer);
eos_bool_t eos_timer_active(eos_timer_handle_t timer);
eos_u32_t eos_timer_remaining_time(eos_timer_handle_t timer);
eos_err_t eos_timer_set_time(eos_timer_handle_t timer, eos_u32_t time);
eos_u32_t eos_timer_get_time(eos_timer_handle_t timer);
eos_err_t eos_timer_reset(eos_timer_handle_t timer);

/* -----------------------------------------------------------------------------
Semaphore
----------------------------------------------------------------------------- */
#define EOS_WAIT_FOREVER                 (-1)
#define EOS_WAIT_NO                      0               /**< Non-block. */

eos_err_t eos_sem_init(eos_sem_handle_t sem, eos_u32_t value);
eos_err_t eos_sem_detach(eos_sem_handle_t sem);
eos_err_t eos_sem_take(eos_sem_handle_t sem, eos_s32_t time);
eos_err_t eos_sem_trytake(eos_sem_handle_t sem);
eos_err_t eos_sem_release(eos_sem_handle_t sem);
eos_err_t eos_sem_reset(eos_sem_handle_t sem, eos_ubase_t value);

/* -----------------------------------------------------------------------------
Mutex
----------------------------------------------------------------------------- */
typedef struct eos_mutex
{
#if (EOS_USE_3RD_KERNEL == 0)
    ek_mutex_t mutex;
#else
    eos_u32_t mutex;
#endif
} eos_mutex_t;

typedef struct eos_mutex *eos_mutex_handle_t;

#ifdef EOS_USING_MUTEX
/*
 * mutex interface
 */
eos_err_t eos_mutex_init(eos_mutex_handle_t mutex);
eos_err_t eos_mutex_detach(eos_mutex_handle_t mutex);
eos_err_t eos_mutex_take(eos_mutex_handle_t mutex, eos_s32_t time);
eos_err_t eos_mutex_trytake(eos_mutex_handle_t mutex);
eos_err_t eos_mutex_release(eos_mutex_handle_t mutex);
#endif

/* Event interface ---------------------------------------------------------- */
void eos_event_send(const char *task, const char *topic);
void eos_event_send_delay(const char *task,
                            const char *topic,
                            eos_u32_t time_delay_ms);
void eos_event_send_period(const char *task,
                            const char *topic,
                            eos_u32_t time_period_ms);

void eos_event_publish(const char *topic);
void eos_event_publish_delay(const char *topic, eos_u32_t time_delay_ms);
void eos_event_publish_period(const char *topic, eos_u32_t time_period_ms);

void eos_event_time_cancel(const char *topic);

void eos_event_sub(const char *topic);
void eos_event_unsub(const char *topic);

bool eos_event_topic(eos_event_t const * const e, const char *topic);

/* -----------------------------------------------------------------------------
Database
----------------------------------------------------------------------------- */
#define EOS_DB_ATTRIBUTE_LINK_EVENT      ((eos_u8_t)0x40U)
#define EOS_DB_ATTRIBUTE_PERSISTENT      ((eos_u8_t)0x20U)
#define EOS_DB_ATTRIBUTE_VALUE           ((eos_u8_t)0x01U)
#define EOS_DB_ATTRIBUTE_STREAM          ((eos_u8_t)0x02U)

void eos_db_init(void *const memory, eos_u32_t size);
void eos_db_register(const char *topic, eos_u32_t size, eos_u8_t attribute);
void eos_db_block_read(const char *topic, void * const data);
void eos_db_block_write(const char *topic, void * const data);
eos_s32_t eos_db_stream_read(const char *topic, void *const buffer, eos_u32_t size);
void eos_db_stream_write(const char *topic, void *const buffer, eos_u32_t size);

/* -----------------------------------------------------------------------------
Reactor
----------------------------------------------------------------------------- */
struct eos_reactor;
typedef void (* eos_event_handler)( struct eos_reactor *const me,
                                    eos_event_t const * const e);

/*
 * Definition of the Reactor class.
 */
typedef struct eos_reactor
{
    eos_task_t super;
    eos_event_handler event_handler;
} eos_reactor_t;

void eos_reactor_init(eos_reactor_t * const me,
                        const char *name,
                        eos_u8_t priority,
                        void *stack, eos_u32_t size);
void eos_reactor_start(eos_reactor_t * const me, eos_event_handler event_handler);

#define EOS_HANDLER_CAST(handler)       ((eos_event_handler)(handler))

/* -----------------------------------------------------------------------------
State machine
----------------------------------------------------------------------------- */
/*
 * Definition of the EventOS reture value.
 */
typedef enum eos_ret
{
    EOS_Ret_Null = 0,
    EOS_Ret_Handled,
    EOS_Ret_Super,
    EOS_Ret_Tran,
} eos_ret_t;

#if (EOS_USE_SM_MODE != 0)
struct eos_sm;
typedef eos_ret_t (* eos_state_handler)(struct eos_sm *const me,
                                        eos_event_t const * const e);
#endif

#if (EOS_USE_SM_MODE != 0)
typedef struct eos_sm
{
    eos_task_t super;
    volatile eos_state_handler state;
} eos_sm_t;
#endif

#if (EOS_USE_SM_MODE != 0)
void eos_sm_init(   eos_sm_t * const me,
                    const char *name,
                    eos_u8_t priority,
                    void *stack, eos_u32_t size);
void eos_sm_start(eos_sm_t * const me, eos_state_handler state_init);

eos_ret_t eos_tran(eos_sm_t * const me, eos_state_handler state);
eos_ret_t eos_super(eos_sm_t * const me, eos_state_handler state);
eos_ret_t eos_state_top(eos_sm_t * const me, eos_event_t const * const e);

#define EOS_TRAN(target)            eos_tran((eos_sm_t * )me, (eos_state_handler)target)
#define EOS_SUPER(super)            eos_super((eos_sm_t * )me, (eos_state_handler)super)
#define EOS_STATE_CAST(state)       ((eos_state_handler)(state))
#endif

/* -----------------------------------------------------------------------------
Assert
----------------------------------------------------------------------------- */
#ifndef EOS_USE_ASSERT

#define EOS_TAG(name_)
#define EOS_ASSERT(test_)                       ((void)0)
#define EOS_ASSERT_ID(id_, test_)               ((void)0)
#define EOS_ASSERT_NAME(id_, test_)             ((void)0)

#else

/* User defined module name. */
#define EOS_TAG(name_)                                                         \
    static char const ___tag_name[] = name_;

/* General assert */
#define EOS_ASSERT(test_) ((test_)                                             \
    ? (void)0 : eos_port_assert(___tag_name, EOS_NULL, (eos_u32_t)__LINE__))

/* General assert with ID */
#define EOS_ASSERT_ID(id_, test_) ((test_)                                     \
    ? (void)0 : eos_port_assert(___tag_name, EOS_NULL, (eos_u32_t)(id_)))

/* General assert with name string or event topic. */
#define EOS_ASSERT_NAME(test_, name_) ((test_)                                 \
    ? (void)0 : eos_port_assert(___tag_name, name_, (eos_u32_t)(__LINE__)))

#endif

/* -----------------------------------------------------------------------------
Trace
----------------------------------------------------------------------------- */
#if (EOS_USE_STACK_USAGE != 0)
eos_u8_t eos_task_stack_usage(eos_u8_t priority);
#endif

#if (EOS_USE_CPU_USAGE != 0)
eos_u8_t eos_task_cpu_usage(eos_u8_t priority);
/*  CPU usage monitor function. It shoulde be put into one isr function whose
    frequency is 10-20 times of the systick isr. */
void eos_cpu_usage_monitor(void);
#endif

/* -----------------------------------------------------------------------------
Port
----------------------------------------------------------------------------- */
void eos_port_assert(const char *tag, const char *name, eos_u32_t id);

#ifdef __cplusplus
}
#endif

#endif
