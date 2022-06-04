#ifndef TEST_H
#define TEST_H

enum
{
    TaskPrio_Give = 1,
    TaskPrio_Value,
    TaskPrio_Specific,
    TaskPrio_Stream,
    TaskPrio_Broadcast,
    TaskPrio_Exit,
    TaskPrio_SmLed,
    TaskPrio_ReacotrLed,
    TaskPrio_Test,

    TaskPrio_Max
};

void test_init(void);
void eos_reactor_led_init(void);
void eos_sm_led_init(void);

void eos_sm_count(void);
void eos_reactor_count(void);

void timer_isr_1ms(void);

#endif
