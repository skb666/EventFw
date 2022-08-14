
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

#include "eos_def.h"
#include "eos_kernel.h"
#include "eventos_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------------
EventOS Default Configuration
----------------------------------------------------------------------------- */
#ifndef EOS_MAX_PRIORITY
#define EOS_MAX_PRIORITY                        8       // 默认最多8个优先级
#endif

#ifndef EOS_MAX_OBJECTS
#define EOS_MAX_OBJECTS                         256     // 默认最多256个对象
#endif

#ifndef EOS_USE_ASSERT
#define EOS_USE_ASSERT                          1       // 默认打开断言
#endif

#ifndef EOS_USE_SM_MODE
#define EOS_USE_SM_MODE                         0       // 默认关闭状态机
#endif

#ifndef EOS_USE_PUB_SUB
#define EOS_USE_PUB_SUB                         0       // 默认关闭发布-订阅机制
#endif

#ifndef EOS_USE_TIME_EVENT
#define EOS_USE_TIME_EVENT                      0       // 默认关闭时间事件
#endif

#ifndef EOS_USE_EVENT_DATA
#define EOS_USE_EVENT_DATA                      0       // 默认关闭时间事件
#endif

#ifndef EOS_USE_EVENT_BRIDGE
#define EOS_USE_EVENT_BRIDGE                    0       // 默认关闭事件桥
#endif

#include <stdbool.h>

/* -----------------------------------------------------------------------------
EventOS
----------------------------------------------------------------------------- */
// EventOS initialization.
void eos_init(void);
// Run EventOS.
void eos_run(void);
// System tick function.
void eos_tick(void);
// 进入中断
void eos_interrupt_enter(void);
// 退出中断
void eos_interrupt_exit(void);
#if (EOS_USE_PREEMPTIVE != 0)
// 禁止任务切换
void eos_sheduler_lock(void);
// 关闭禁止任务切换
void eos_sheduler_unlock(void);
#endif

void eos_kernel_start(void);

void eos_task_idle_init(void);

void eos_enter_critical(void);
void eos_exit_critical(void);
eos_u16_t eos_critical_level(void);

eos_base_t eos_hw_interrupt_disable(void);
void eos_hw_interrupt_enable(eos_base_t level);

void eos_system_timer_init(void);
void eos_system_timer_task_init(void);

eos_u32_t eos_tick_get(void);
void eos_tick_set(eos_u32_t tick);
void eos_tick_increase(void);
eos_u32_t eos_tick_from_millisecond(eos_s32_t ms);
eos_u32_t eos_tick_get_millisecond(void);

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


/* -----------------------------------------------------------------------------
Semaphore
----------------------------------------------------------------------------- */
typedef struct eos_semaphore
{
#if (EOS_USE_3RD_KERNEL == 0)
    ek_sem_t sem;
#else
    eos_u32_t sem;
#endif
} eos_sem_t;

typedef struct eos_semaphore *eos_sem_handle_t;

#ifdef EOS_USING_SEMAPHORE
/*
 * semaphore interface
 */
eos_err_t eos_sem_init(eos_sem_handle_t sem,
                       const char *name,
                       eos_u32_t value,
                       eos_u8_t flag);
eos_err_t eos_sem_detach(eos_sem_handle_t sem);

eos_err_t eos_sem_take(eos_sem_handle_t sem, eos_s32_t time);
eos_err_t eos_sem_trytake(eos_sem_handle_t sem);
eos_err_t eos_sem_release(eos_sem_handle_t sem);
eos_err_t eos_sem_reset(eos_sem_handle_t sem, eos_ubase_t value);
#endif

/* -----------------------------------------------------------------------------
Task
----------------------------------------------------------------------------- */
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

    eos_u16_t t_id;                          // task ID
    eos_u16_t index;
    bool event_recv_disable;
    bool wait_specific_event;
    const char *event_wait;
} eos_task_t;

typedef eos_task_t *eos_task_handle_t;

/*
 * task interface
 */
eos_err_t eos_task_init(eos_task_t *task,
                        const char *name,
                        void (*entry)(void *parameter),
                        void *parameter,
                        void *stack_start,
                        eos_u32_t stack_size,
                        eos_u8_t priority,
                        eos_u32_t tick);
eos_err_t eos_task_detach(eos_task_handle_t task);
eos_task_handle_t eos_task_self(void);
eos_err_t eos_task_startup(eos_task_handle_t task);
eos_err_t eos_task_yield(void);
eos_err_t eos_task_delay(eos_u32_t tick);
eos_err_t eos_task_delay_until(eos_u32_t *tick, eos_u32_t inc_tick);
eos_err_t eos_task_mdelay(eos_s32_t ms);
eos_err_t eos_task_control(eos_task_handle_t task, int cmd, void *arg);
eos_err_t eos_task_suspend(eos_task_handle_t task);
eos_err_t eos_task_resume(eos_task_handle_t task);
eos_task_state_t eos_task_get_state(eos_task_handle_t task);
eos_u8_t eos_task_get_priority(eos_task_handle_t task);

// 启动任务，main函数或者任务函数中调用。
void eos_task_start(eos_task_t * const me,
                    const char *name,
                    eos_func_t func,
                    eos_u8_t priority,
                    void *stack_addr,
                    eos_u32_t stack_size,
                    void *parameter);
// 退出当前任务，任务函数中调用。
void eos_task_exit(void);
// 任务等待某特定事件，其他事件均忽略。
bool eos_task_wait_specific_event(  eos_event_t * const e_out,
                                    const char *topic, eos_s32_t time_ms);
// 任务阻塞式等待事件
bool eos_task_wait_event(eos_event_t * const e_out, eos_s32_t time_ms);

/* defunct */
void eos_task_defunct_enqueue(eos_task_handle_t task);
eos_task_handle_t eos_task_defunct_dequeue(void);

/* -----------------------------------------------------------------------------
Timer
----------------------------------------------------------------------------- */
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
eos_err_t eos_mutex_init(eos_mutex_handle_t mutex, const char *name, eos_u8_t flag);
eos_err_t eos_mutex_detach(eos_mutex_handle_t mutex);

eos_err_t eos_mutex_take(eos_mutex_handle_t mutex, eos_s32_t time);
eos_err_t eos_mutex_trytake(eos_mutex_handle_t mutex);
eos_err_t eos_mutex_release(eos_mutex_handle_t mutex);
#endif

/* -----------------------------------------------------------------------------
Event
----------------------------------------------------------------------------- */
// 事件的直接发送 -----------------------------------------
// 直接发送事件。允许在中断中调用。
void eos_event_send(const char *task, const char *topic);
// 延迟发送事件。
void eos_event_send_delay(const char *task, const char *topic, eos_u32_t time_delay_ms);
// 周期发送事件。
void eos_event_send_period(const char *task, const char *topic, eos_u32_t time_period_ms);

// 事件的发布 --------------------------------------------
// 发布主题事件。允许在中断中调用。
void eos_event_publish(const char *topic);
// 延时发布某事件。允许在中断中调用。
void eos_event_publish_delay(const char *topic, eos_u32_t time_delay_ms);
// 周期发布某事件。允许在中断中调用。
void eos_event_publish_period(const char *topic, eos_u32_t time_period_ms);
// 取消某延时或者周期事件的发布。允许在中断中调用。
void eos_event_time_cancel(const char *topic);

// 事件的订阅 --------------------------------------------
// 事件订阅，仅在任务函数、状态函数或者事件回调函数中使用。
void eos_event_sub(const char *topic);
// 事件取消订阅，仅在任务函数、状态函数或者事件回调函数中使用。
void eos_event_unsub(const char *topic);

// 事件的接收 --------------------------------------------
// 事件接收。仅在任务函数、状态函数或者事件回调函数中使用。
bool eos_event_topic(eos_event_t const * const e, const char *topic);

/* -----------------------------------------------------------------------------
Database
----------------------------------------------------------------------------- */
#define EOS_DB_ATTRIBUTE_LINK_EVENT      ((eos_u8_t)0x40U)
#define EOS_DB_ATTRIBUTE_PERSISTENT      ((eos_u8_t)0x20U)
#define EOS_DB_ATTRIBUTE_VALUE           ((eos_u8_t)0x01U)
#define EOS_DB_ATTRIBUTE_STREAM          ((eos_u8_t)0x02U)

// 事件数据库的初始化
void eos_db_init(void *const memory, eos_u32_t size);
// 事件数据库的注册。
void eos_db_register(const char *topic, eos_u32_t size, eos_u8_t attribute);
// 块数据的读取。
void eos_db_block_read(const char *topic, void * const data);
// 块数据的写入。允许在中断中调用。
void eos_db_block_write(const char *topic, void * const data);
// 流数据的读取。
eos_s32_t eos_db_stream_read(const char *topic, void *const buffer, eos_u32_t size);
// 流数据的写入。允许在中断中调用。
void eos_db_stream_write(const char *topic, void *const buffer, eos_u32_t size);

/* -----------------------------------------------------------------------------
Reactor
----------------------------------------------------------------------------- */
// 事件处理句柄的定义
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

void eos_reactor_init(  eos_reactor_t * const me,
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
    EOS_Ret_Null = 0,                       // 无效值
    EOS_Ret_Handled,                        // 已处理，不产生跳转
    EOS_Ret_Super,                          // 到超状态
    EOS_Ret_Tran,                           // 跳转
} eos_ret_t;

#if (EOS_USE_SM_MODE != 0)
// 状态函数句柄的定义
struct eos_sm;
typedef eos_ret_t (* eos_state_handler)(struct eos_sm *const me,
                                        eos_event_t const * const e);
#endif

#if (EOS_USE_SM_MODE != 0)
// 状态机类
typedef struct eos_sm
{
    eos_task_t super;
    volatile eos_state_handler state;
} eos_sm_t;
#endif

#if (EOS_USE_SM_MODE != 0)
// 状态机初始化函数
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
#define EOS_ASSERT_INFO(test_, ...)             ((void)0)

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
        
/* Assert with printed information. */
#define EOS_ASSERT_INFO(test_, ...) ((test_)                                   \
    ? (void)0 : elog_assert_info(___tag_name, __VA_ARGS__))

#endif

/* -----------------------------------------------------------------------------
Log
----------------------------------------------------------------------------- */
#if (EOS_USE_LOG != 0)
#include "elog.h"

#define EOS_PRINT(...)            elog_printf(__VA_ARGS__)
#define EOS_DEBUG(...)            elog_debug(___tag_name, __VA_ARGS__)
#define EOS_INFO(...)             elog_info(___tag_name, __VA_ARGS__)
#define EOS_WARN(...)             elog_warn(___tag_name, __VA_ARGS__)
#define EOS_ERROR(...)            elog_error(___tag_name, __VA_ARGS__)
#endif

/* -----------------------------------------------------------------------------
Trace
----------------------------------------------------------------------------- */
#if (EOS_USE_STACK_USAGE != 0)
// 任务的堆栈使用率
eos_u8_t eos_task_stack_usage(eos_u8_t priority);
#endif

#if (EOS_USE_CPU_USAGE != 0)
// 任务的CPU使用率
eos_u8_t eos_task_cpu_usage(eos_u8_t priority);
// 监控函数，放进一个单独的定时器中断函数，中断频率为SysTick的10-20倍。
void eos_cpu_usage_monitor(void);
#endif

/* -----------------------------------------------------------------------------
Port
----------------------------------------------------------------------------- */
void eos_port_task_switch(void);
void eos_port_assert(const char *tag, const char *name, eos_u32_t id);

/* -----------------------------------------------------------------------------
Hook
----------------------------------------------------------------------------- */
// 空闲回调函数
void eos_hook_idle(void);

// 结束EventOS的运行的时候，所调用的回调函数。
void eos_hook_stop(void);

// 启动EventOS的时候，所调用的回调函数
void eos_hook_start(void);

#ifdef __cplusplus
}
#endif

#endif
