#include "test.h"
#include <stdint.h>
#include "eos.h"
#include "bsp.h"

#if (TEST_EN_01_1 != 0)

/* private data structure --------------------------------------------------- */
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

extern void eos_event_send_id(uint32_t task_id, const char *topic);
extern uint32_t eos_get_task_id(const char *task);

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

static const task_test_info_t task_test_info[] =
{
    {
        &task_e_give1, "TaskGive1", TaskPrio_Give1,
        stack_e_give1, sizeof(stack_e_give1),
        task_func_e_give1
    },
    {
        &task_e_give2, "TaskGive2", TaskPrio_Give2,
        stack_e_give2, sizeof(stack_e_give2),
        task_func_e_give2
    },
    {
        &task_e_value, "TaskValue", TaskPrio_Value,
        stack_e_value, sizeof(stack_e_value),
        task_func_e_value
    },
    {
        &task_high, "TaskHigh", TaskPrio_High,
        stack_high, sizeof(stack_high),
        task_func_high
    },
    {
        &task_middle, "TaskMiddle", TaskPrio_Middle,
        stack_middle, sizeof(stack_middle),
        task_func_middle
    },
};

/* public function ---------------------------------------------------------- */
uint32_t task_id = 0;
void test_init(void)
{
    eos_enter_critical();

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
                       task_test_info[i].prio);
        eos_task_startup(task_test_info[i].task);
    }

    task_id = eos_get_task_id("TaskValue");
    
    eos_exit_critical();

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
        eos_event_send_id(task_id, "Event_One");
    }
    
    eos_interrupt_leave();
}

/* public function ---------------------------------------------------------- */
static void task_func_e_give1(void *parameter)
{
    (void)parameter;
    
    while (1)
    {
        eos_test.time = eos_tick_get_ms();
        eos_test.send_count ++;
        eos_test.send_give1_count ++;
        eos_test.send_speed = eos_test.send_count / eos_test.time;
        
        eos_event_send_id(task_id, "Event_One");
    }
}

static void task_func_e_give2(void *parameter)
{
    (void)parameter;
    
    while (1)
    {
        eos_test.time = eos_tick_get_ms();
        eos_test.send_count ++;
        eos_test.send_give2_count ++;
        eos_test.send_speed = eos_test.send_count / eos_test.time;
        
        eos_event_send_id(task_id, "Event_One");
    }
}

static void task_func_e_value(void *parameter)
{
    (void)parameter;
    
    while (1)
    {
        eos_event_t e;
        if (eos_task_wait_event(&e, 10000) == false)
        {
            eos_test.error = 1;
            continue;
        }

        if (eos_event_topic(&e, "Event_One"))
        {
            eos_test.e_one ++;
        }
    }
}

static void task_func_high(void *parameter)
{
    (void)parameter;
    
    while (1)
    {
        eos_test.send_count ++;
        eos_test.high_count ++;
        eos_event_send_id(task_id, "Event_One");
        eos_task_delay_ms(1);
    }
}

static void task_func_middle(void *parameter)
{
    (void)parameter;
    
    while (1)
    {
        eos_test.send_count ++;
        eos_test.middle_count += 2;
        eos_event_send_id(task_id, "Event_One");
        eos_task_delay_ms(2);
    }
}

#endif
