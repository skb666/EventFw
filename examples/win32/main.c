/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-12-18     zylx         first version
 */

#include <eventos.h>
#include <stdint.h>

EOS_TAG("Main")

#define RT_MAIN_THREAD_STACK_SIZE           1024
#define RT_MAIN_THREAD_PRIORITY             8

static uint64_t main_stack[RT_MAIN_THREAD_STACK_SIZE];
struct eos_task main_task;

static void eos_application_init(void);
static void main_thread_entry(void *parameter);

int main(void)
{
    eos_hw_interrupt_disable();
    eos_system_timer_init();
    eos_kernel_init();
    
    eos_application_init();
    
    eos_system_timer_task_init();
    eos_task_idle_init();

    eos_kernel_start();
}

/**
 * @brief  This function will create and start the main thread, but this thread
 *         will not run until the scheduler starts.
 */
void eos_application_init(void)
{
    eos_task_handle_t tid;
    eos_err_t result;

    tid = &main_task;
    result = eos_task_init(tid, "main", main_thread_entry, EOS_NULL,
                            main_stack, sizeof(main_stack), RT_MAIN_THREAD_PRIORITY, 20);
    EOS_ASSERT(result == EOS_EOK);

    /* if not define RT_USING_HEAP, using to eliminate the warning */
    (void)result;

    eos_task_startup(tid);
}

uint32_t count_cpu = 0;

/**
 * @brief  The system main thread. In this thread will call the rt_components_init()
 *         for initialization of RT-Thread Components and call the user's programming
 *         entry main().
 */

struct eos_mutex mutex;
eos_mutex_t p_mutex = &mutex;
static void main_thread_entry(void *parameter)
{
    eos_mutex_init(p_mutex, "mutex", 0);
    eos_mutex_take(p_mutex, 1000);
    eos_mutex_release(p_mutex);
    eos_mutex_trytake(p_mutex);
    eos_mutex_release(p_mutex);

    while (1)
    {
        count_cpu ++;
        if (count_cpu % 100 == 0) 
        printf("count_cpu: %d.\n", count_cpu);
        eos_task_mdelay(10);
    }
}
