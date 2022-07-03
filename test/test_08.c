#include "test.h"
#include <stdint.h>
#include "eventos.h"
#include "bsp.h"

#if (TEST_EN_08 != 0)

/* private data structure --------------------------------------------------- */
typedef struct eos_test
{
    uint32_t error;
    uint8_t isr_func_enable;
    uint8_t event_one_sub;

    uint32_t time;
    uint32_t send_speed;

    uint32_t send_count;
    uint32_t high_count;
    uint32_t middle_count;
    uint32_t e_one;
    uint32_t e_sm;
    uint32_t e_reactor;
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

static void task_func(void *parameter);

/* private data ------------------------------------------------------------- */
static uint64_t stack_e_give[64];
static eos_task_t task_e_give;
static uint64_t stack_e_value[64];
static eos_task_t task_e_value;
static uint64_t stack_high[64];
static eos_task_t task_high;
static uint64_t stack_middle[64];
static eos_task_t task_middle;
uint32_t count_task[4];

eos_test_t eos_test;

static const task_test_info_t task_test_info[] =
{
    {
        &task_e_give, "TaskGive", TaskPrio_Give,
        stack_e_give, sizeof(stack_e_give),
        task_func
    },
    {
        &task_e_value, "TaskValue", TaskPrio_Value,
        stack_e_value, sizeof(stack_e_value),
        task_func
    },
    {
        &task_high, "TaskHigh", TaskPrio_High,
        stack_high, sizeof(stack_high),
        task_func
    },
    {
        &task_middle, "TaskMiddle", TaskPrio_Middle,
        stack_middle, sizeof(stack_middle),
        task_func
    },
};

/* public function ---------------------------------------------------------- */
void test_init(void)
{
    for (uint32_t i = 0;
         i < (sizeof(task_test_info) / sizeof(task_test_info_t));
         i ++)
    {
        eos_task_start(task_test_info[i].task,
                       task_test_info[i].name,
                       task_test_info[i].func,
                       task_test_info[i].prio,
                       task_test_info[i].stack,
                       task_test_info[i].stack_size,
                       &count_task[i]);
    }
}

void eos_sm_count(void)
{

}

void eos_reactor_count(void)
{

}

void timer_isr_1ms(void)
{
}

/* public function ---------------------------------------------------------- */
static void task_func(void *parameter)
{
    uint32_t *count = (uint32_t *)parameter;
    while (1)
    {
        count[0] ++;
        eos_delay_ms(1);
    }
}

#endif
