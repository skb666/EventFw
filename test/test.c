#include "test.h"
#include <stdint.h>
#include "eventos.h"

EOS_TAG("Test")

/* private defines ---------------------------------------------------------- */
enum
{
    Task_EventSend = 0,
    Task_TopicEvent,
    Task_ValueEvent,
    Task_StreamEvent,
    Task_BroadcastEvent,
};

/* private data structure --------------------------------------------------- */
typedef struct e_value
{
    uint32_t count;
    uint32_t value;
} e_value_t;

typedef struct eos_test
{
    uint32_t error;
    
    uint32_t time;

    uint32_t send_count;
    uint32_t e_one;
    uint32_t e_delay;
    uint32_t e_two;
    uint32_t e_specific;
    uint32_t e_value;
    uint32_t e_value_link;
    uint32_t e_stream;
    uint32_t e_stream_link;
    uint32_t e_broadcast;
    uint32_t e_broadcast_value;
    uint32_t e_sm;
    uint32_t e_reactor;

    e_value_t e_value_recv;
    e_value_t e_value_link_recv;
    e_value_t e_value_broadcast;
    
    uint8_t flag_send_delay;
    uint8_t flag_send;

    char buffer[32];
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


static void task_func_e_give(void *parameter);
static void task_func_e_value(void *parameter);
static void task_func_e_specific(void *parameter);
static void task_func_e_stream(void *parameter);
static void task_func_e_broadcast(void *parameter);

/* private data ------------------------------------------------------------- */
static uint64_t stack_e_give[64];
static eos_task_t task_e_give;
static uint64_t stack_e_value[64];
static eos_task_t task_e_value;
static uint64_t stack_e_specific[64];
static eos_task_t task_e_specific;
static uint64_t stack_e_stream[64];
static eos_task_t task_e_stream;
static uint64_t stack_e_broadcast[64];
static eos_task_t task_e_broadcast;

eos_test_t eos_test;

static const task_test_info_t task_test_info[] =
{
    {
        &task_e_give, "TaskGive", TaskPrio_Give,
        stack_e_give, sizeof(stack_e_give),
        task_func_e_give
    },
    {
        &task_e_value, "TaskValue", TaskPrio_Value,
        stack_e_value, sizeof(stack_e_value),
        task_func_e_value
    },
    {
        &task_e_specific, "TaskSpecific", TaskPrio_Specific,
        stack_e_specific, sizeof(stack_e_specific),
        task_func_e_specific
    },
    {
        &task_e_stream, "TaskStream", TaskPrio_Stream,
        stack_e_stream, sizeof(stack_e_stream),
        task_func_e_stream
    },
    { 
        &task_e_broadcast, "TaskBroadcast", TaskPrio_Broadcast,
        stack_e_broadcast, sizeof(stack_e_broadcast),
        task_func_e_broadcast
    },
};

/* public function ---------------------------------------------------------- */
void test_init(void)
{
    eos_db_register("Event_Value", sizeof(e_value_t), EOS_DB_ATTRIBUTE_VALUE);
    eos_db_register("Event_Value_Link", sizeof(e_value_t),
                    (EOS_DB_ATTRIBUTE_VALUE | EOS_DB_ATTRIBUTE_LINK_EVENT));
    
    eos_db_register("Event_Stream", 256, EOS_DB_ATTRIBUTE_STREAM);
    eos_db_register("Event_Stream_Link", 256,
                    (EOS_DB_ATTRIBUTE_STREAM | EOS_DB_ATTRIBUTE_LINK_EVENT));

    for (uint32_t i = 0;
         i < (sizeof(task_test_info) / sizeof(task_test_info_t));
         i ++)
    {
        eos_task_start(task_test_info[i].task,
                       task_test_info[i].name,
                       task_test_info[i].func,
                       task_test_info[i].prio,
                       task_test_info[i].stack,
                       task_test_info[i].stack_size);
    }

    eos_sm_led_init();
    eos_reactor_led_init();

    // eos_task_exit();
}

/* public function ---------------------------------------------------------- */
static void task_func_e_give(void *parameter)
{
    (void)parameter;

    while (1)
    {
        eos_test.time = eos_time();
        eos_test.send_count ++;
        
        eos_sheduler_lock();
        EOS_WARN("Send Event_One to TaskValue");
        eos_event_send("TaskValue", "Event_One");
        EOS_WARN("Send Event_Two to TaskSpecific");
        eos_event_send("TaskSpecific", "Event_Two");
        EOS_WARN("Event_Broadcast");
        eos_event_broadcast("Event_Broadcast");
        eos_sheduler_unlock();

        if (eos_test.flag_send_delay != 0) {
            eos_test.flag_send_delay = 0;
            EOS_DEBUG("Send Delay");
            eos_event_send_delay("TaskValue", "Event_Delay", 50);
        }
        
        if (eos_test.flag_send != 0) {
            eos_test.flag_send = 0;
            EOS_DEBUG("Publish event Delay");
            eos_event_publish("Event_Delay");
        }
        
        e_value_t e_value;
        e_value.count = eos_test.send_count;
        e_value.value = 12345678;

        eos_db_block_write("Event_Value", &e_value);
        eos_event_send("TaskValue", "Event_Value");
        eos_db_block_write("Event_Value", &e_value);
        eos_event_send("TaskValue", "Event_Value");
        eos_db_block_write("Event_Value_Link", &e_value);
        eos_event_send("TaskValue", "Event_Value_Link");
        
        eos_db_stream_write("Event_Stream", "abc", 3);
        eos_event_send("TaskStream", "Event_Stream");
        eos_db_stream_write("Event_Stream", "defg", 4);
        eos_event_send("TaskStream", "Event_Stream");
        eos_db_stream_write("Event_Stream_Link", "1234567890", 10);
        
        if ((eos_test.send_count % 10) == 0)
        {
            eos_event_send("TaskValue", "Event_Specific");
            eos_event_send("TaskSpecific", "Event_Two");
        }
        if (eos_test.send_count == 10) {
            eos_task_suspend("sm_led");
        }
        if (eos_test.send_count == 20) {
            eos_task_resume("sm_led");
        }
        
        eos_delay_ms(100);
    }
}

static void task_func_e_value(void *parameter)
{
    (void)parameter;
    eos_event_sub("Event_Value_Link");
    
    while (1) {
        eos_event_t e;
        if (eos_task_wait_event(&e, 10000) == false) {
            eos_test.error = 1;
            continue;
        }

        if (eos_event_topic(&e, "Event_One")) {
            EOS_WARN("Receive Event_One to TaskValue");
            eos_test.e_one ++;
        }

        if (eos_event_topic(&e, "Event_Delay")) {
            EOS_DEBUG("Receive event Delay");
            eos_test.e_delay ++;
        }
        
        if (eos_event_topic(&e, "Event_Specific")) {
            eos_test.e_specific ++;
        }
        
        if (eos_event_topic(&e, "Event_Value")) {
            eos_db_block_read("Event_Value", &eos_test.e_value_recv);
            eos_test.e_value ++;
        }
        
        if (eos_event_topic(&e, "Event_Value_Link")) {
            eos_db_block_read("Event_Value_Link", &eos_test.e_value_link_recv);
            eos_test.e_value_link ++;
        }
    }
}

static void task_func_e_specific(void *parameter)
{
    (void)parameter;

    eos_event_t e;
    while (1) {
        
        if (eos_task_wait_specific_event(&e, "Event_Specific", 10000)) {
            eos_test.error = 1;
            continue;
        }

        if (eos_event_topic(&e, "Event_Two")) {
            eos_test.e_two ++;
        }
        
        if (eos_event_topic(&e, "Event_Specific")) {
            eos_test.e_specific ++;
        }
    }
}

static void task_func_e_stream(void *parameter)
{
    (void)parameter;

    eos_event_t e;
    eos_event_sub("Event_Stream_Link");
    
    while (1)
    {
        if (eos_task_wait_event(&e, 10000) == false) {
            eos_test.error = 1;
            continue;
        }
        
        int32_t ret;
        if (eos_event_topic(&e, "Event_Stream")) {
            ret = eos_db_stream_read("Event_Stream", eos_test.buffer, 32);
            if (ret > 0) {
                eos_test.buffer[ret] = 0;
                EOS_INFO("Event_Stream: %s.", eos_test.buffer);
            }
            else {
                EOS_INFO("Event_Stream read err: %d.", (int32_t)ret);
            }
        }
        
        if (eos_event_topic(&e, "Event_Stream_Link")) {
            ret = eos_db_stream_read("Event_Stream_Link", eos_test.buffer, 32);
            if (ret > 0) {
                eos_test.buffer[ret] = 0;
                EOS_INFO("Event_Stream_Link: %s.", eos_test.buffer);
            }
            else {
                EOS_INFO("Event_Stream_Link read err: %d.", (int32_t)ret);
            }
        }
    }
}

static void task_func_e_broadcast(void *parameter)
{
    (void)parameter;
    eos_event_t e;

    while (1)
    {
        if (eos_task_wait_event(&e, 10000) == false) {
            eos_test.error = 1;
            continue;
        }
        
        if (eos_event_topic(&e, "Event_Broadcast")) {
            eos_test.e_broadcast ++;
        }
        
        if (eos_event_topic(&e, "Event_Value_Broadcast")) {
            eos_db_block_read("Event_Value_Broadcast",
                              &eos_test.e_value_broadcast);
            eos_test.e_broadcast_value ++;
        }
    }
}

