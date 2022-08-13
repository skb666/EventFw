#include "test.h"
#include <stdint.h>
#include <stdio.h>
#include "eventos.h"
#include "bsp.h"

#if (TEST_EN_SEM != 0)

/* private data structure --------------------------------------------------- */
typedef struct e_value
{
    uint32_t count;
    uint32_t value;
} e_value_t;

typedef struct eos_test
{
    uint32_t error;
    uint8_t isr_func_enable;
    
    uint32_t time;
    uint32_t send_speed;

    uint32_t send_count;
    uint32_t high_count;
    uint32_t middle_count;
    uint32_t e_one;
    
    uint32_t send_give1_count;
    uint32_t send_give2_count;
    uint32_t send_isr;
    
    uint32_t idle_count;
} eos_test_t;

typedef struct task_test
{
    eos_task_t *task;
    const char *name;
    uint8_t prio;
    void *stack;
    uint32_t stack_size;
    void (* func)(void *parameter);
} task_test_info_t;

static void task_func_e_give1(void *parameter);
static void task_func_e_give2(void *parameter);
static void task_func_e_value(void *parameter);
static void task_func_high(void *parameter);
static void task_func_middle(void *parameter);

/* private data ------------------------------------------------------------- */
static uint64_t stack_e_give1[64];
static eos_task_t task_e_give1;
static uint64_t stack_e_give2[64];
static eos_task_t task_e_give2;
static uint64_t stack_e_value[64];
static eos_task_t task_e_value;
static uint64_t stack_high[64];
static eos_task_t task_high;
static uint64_t stack_middle[64];
static eos_task_t task_middle;

eos_test_t eos_test;

eos_sem_t sem_test;

static const task_test_info_t task_test_info[] =
{
    {
        &task_e_give1, "TaskGive1", TaskPrio_Give1,
        stack_e_give1, sizeof(stack_e_give1),
        task_func_e_give1
    },
//    {
//        &task_e_give2, "TaskGive2", TaskPrio_Give2,
//        stack_e_give2, sizeof(stack_e_give2),
//        task_func_e_give2
//    },
    {
        &task_e_value, "TaskValue", TaskPrio_Value,
        stack_e_value, sizeof(stack_e_value),
        task_func_e_value
    },
//    {
//        &task_high, "TaskHigh", TaskPrio_High,
//        stack_high, sizeof(stack_high),
//        task_func_high
//    },
//    {
//        &task_middle, "TaskMiddle", TaskPrio_Middle,
//        stack_middle, sizeof(stack_middle),
//        task_func_middle
//    },
};

/* public function ---------------------------------------------------------- */
void test_init(void)
{
    for (uint32_t i = 0;
         i < (sizeof(task_test_info) / sizeof(task_test_info_t));
         i ++)
    {
        eos_task_init(task_test_info[i].task,
                       task_test_info[i].name,
                       task_test_info[i].func,
                       EOS_NULL,
                       task_test_info[i].stack,
                       task_test_info[i].stack_size,
                       task_test_info[i].prio,
                       10);
        eos_task_startup(task_test_info[i].task);
    }

    eos_sem_init(&sem_test, "sem_test", 0, EOS_IPC_FLAG_FIFO);

    timer_init(1);
}

void eos_sm_count(void)
{

}

void eos_reactor_count(void)
{

}

void eos_idle_count(void)
{
    eos_test.idle_count ++;
}

void timer_isr_1ms(void)
{
    eos_interrupt_enter();
    
    if (eos_test.isr_func_enable != 0)
    {
        eos_sem_release(&sem_test);
        eos_test.send_count ++;
        eos_test.send_isr ++;
    }
    
    eos_interrupt_leave();
}

/* public function ---------------------------------------------------------- */
bool send = false;
static void task_func_e_give1(void *parameter)
{
    (void)parameter;
    
    while (1)
    {
        eos_test.time = eos_tick_get_millisecond();
        eos_test.send_count ++;
        eos_test.send_give1_count ++;
        if (eos_test.time != 0)
        {
            eos_test.send_speed = eos_test.send_count / eos_test.time;
        }
        
        eos_sem_release(&sem_test);
    }
}

static void task_func_e_give2(void *parameter)
{
    (void)parameter;
    
    while (1)
    {
        eos_test.time = eos_tick_get_millisecond();
        eos_test.send_count ++;
        eos_test.send_give2_count ++;
        if (eos_test.time != 0)
        {
            eos_test.send_speed = eos_test.send_count / eos_test.time;
        }
        
        eos_sem_release(&sem_test);
    }
}

static void task_func_e_value(void *parameter)
{
    (void)parameter;
    
    while (1)
    {
        eos_sem_take(&sem_test, EOS_TIME_FOREVER);
        eos_test.e_one ++;
    }
}

static void task_func_high(void *parameter)
{
    (void)parameter;
    
    while (1)
    {
        eos_test.send_count ++;
        eos_test.high_count ++;
        eos_sem_release(&sem_test);
        eos_task_mdelay(1);
    }
}

static void task_func_middle(void *parameter)
{
    (void)parameter;
    
    while (1)
    {
        eos_test.send_count ++;
        eos_test.middle_count += 2;
        eos_sem_release(&sem_test);
        eos_task_mdelay(2);
    }
}

#endif
