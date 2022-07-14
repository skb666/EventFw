#include "test.h"
#include <stdint.h>
#include "eventos.h"
#include "bsp.h"

EOS_TAG("Test03")

#if (TEST_EN_03 != 0)

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
    uint32_t stream_count;
    uint32_t e_sm;
    uint32_t e_reactor;

    uint32_t send_give1_count;
    uint32_t send_give2_count;
    
    uint32_t isr_count;
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

eos_task_t *p_high = EOS_NULL;

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
void test_init(void)
{
    eos_db_register("Event_One", 5000, EOS_DB_ATTRIBUTE_STREAM);

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
                       EOS_NULL);
    }
    
    p_high = &task_high;

    timer_init(1);
}

void eos_sm_count(void)
{
    eos_test.e_sm ++;
}

void eos_reactor_count(void)
{
    eos_test.e_reactor ++;
}

extern eos_task_t *volatile eos_current;

void timer_isr_1ms(void)
{
    eos_interrupt_enter();
    bool sent = false;
    
    if (eos_test.isr_func_enable != 0)
    {
        if (task_high.state == 1)
        sent = true;
        eos_test.isr_count ++;
        eos_db_stream_write("Event_One", "1", 1);
        eos_event_send("TaskValue", "Event_One");
    }
    
    eos_interrupt_exit();
    if (sent)
    EOS_ASSERT(task_high.state == 1);
}

void eos_idle_count(void)
{
    eos_test.idle_count ++;
}

/* public function ---------------------------------------------------------- */
static void task_func_e_give1(void *parameter)
{
    (void)parameter;
    
    while (1)
    {
        eos_test.time = eos_time();
        eos_test.send_count ++;
        eos_test.send_give1_count ++;
        eos_test.send_speed = eos_test.send_count / eos_test.time;
        
        eos_db_stream_write("Event_One", "1", 1);
        eos_event_send("TaskValue", "Event_One");
    }
}

static void task_func_e_give2(void *parameter)
{
    (void)parameter;
    
    while (1)
    {
        eos_test.time = eos_time();
        eos_test.send_count ++;
        eos_test.send_give2_count ++;
        eos_test.send_speed = eos_test.send_count / eos_test.time;
        
        eos_db_stream_write("Event_One", "1", 1);
        eos_event_send("TaskValue", "Event_One");
    }
}

uint32_t count_time = 0;
static void task_func_e_value(void *parameter)
{
    eos_event_t e;
    uint8_t buffer[32];
    (void)parameter;
    
    int32_t ret = 0;
    while (1)
    {
        if (eos_task_wait_event(&e, 10000) == false)
        {
            eos_test.error = 1;
            continue;
        }
        
        if (eos_event_topic(&e, "Event_One"))
        {
            eos_test.e_one ++;
            ret = eos_db_stream_read("Event_One", buffer, 32);
            if (ret > 0)
            {
                eos_test.stream_count += ret;
            }
        }
    }
}


uint32_t count_enter_isr = 0;
static void task_func_high(void *parameter)
{
    (void)parameter;
    uint32_t isr_count_bkp = 0;
    
    while (1)
    {
        EOS_ASSERT(eos_current->state == 1);
        eos_test.send_count ++;
        eos_test.high_count ++;
        isr_count_bkp = eos_test.isr_count;
        eos_db_stream_write("Event_One", "1", 1);
        if (isr_count_bkp != eos_test.isr_count)
        {
            count_enter_isr ++;
        }
        if (eos_current->state != 1)
        {
            EOS_ASSERT(eos_current->state == 1);
        }
        
        eos_event_send("TaskValue", "Event_One");
        EOS_ASSERT(eos_current->state == 1);
        eos_delay_ms(1);
        EOS_ASSERT(eos_current->state == 1);
    }
}

static void task_func_middle(void *parameter)
{
    (void)parameter;
    
    while (1)
    {
        eos_test.send_count ++;
        eos_test.middle_count += 2;
        eos_db_stream_write("Event_One", "1", 1);
        eos_event_send("TaskValue", "Event_One");
        eos_delay_ms(2);
    }
}

#endif
