#include "test.h"
#include <stdint.h>
#include "eventos.h"
#include "bsp.h"

#define EOS_MAX_TASK_TEST                   (8)

#if (TEST_EN_11 != 0)

/* private data structure --------------------------------------------------- */
typedef struct e_value
{
    uint32_t count;
    uint32_t value;
} e_value_t;

typedef struct eos_test
{
    uint32_t count[EOS_MAX_TASK_TEST];
    
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


static void task_func(void *parameter);

/* private data ------------------------------------------------------------- */
const char *task_name[EOS_MAX_TASK_TEST] =
{
    "Task01", "Task02", "Task03", "Task04", "Task05", 
    "Task06", "Task07", "Task08", 
};
typedef struct eos_task_info
{
    uint64_t stack[64];
    eos_task_t task;
} eos_task_info_t;

eos_task_info_t task_info[EOS_MAX_TASK_TEST];
eos_test_t eos_test;

eos_task_t * p_task[EOS_MAX_TASK_TEST];

/* public function ---------------------------------------------------------- */
void test_init(void)
{
    for (uint32_t i = 0; i < EOS_MAX_TASK_TEST; i ++)
    {
        eos_task_start(&task_info[i].task,
                       task_name[i],
                       task_func,
                       1,
                       task_info[i].stack,
                       sizeof(task_info[i].stack),
                       &eos_test.count[i]);
        p_task[i] = &task_info[i].task;
    }
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
    
    eos_interrupt_exit();
}

/* public function ---------------------------------------------------------- */
static void task_func(void *parameter)
{
    uint32_t *count = (uint32_t *)parameter;
    
    while (1)
    {
        (*count) ++;
    }
}

#endif
