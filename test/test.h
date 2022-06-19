#ifndef TEST_H
#define TEST_H

#define TEST_EN_01                      (1)
#define TEST_EN_02_0                    (0)
#define TEST_EN_02_1                    (0)
#define TEST_EN_02_2                    (0)
#define TEST_EN_03                      (0)
#define TEST_EN_04                      (0)
#define TEST_EN_05                      (0)
#define TEST_EN_06                      (0)

enum
{
    TaskPrio_Give = 1,
    TaskPrio_Middle,
    TaskPrio_Value,
    TaskPrio_SmLed,
    TaskPrio_ReacotrLed,
    TaskPrio_High,

    TaskPrio_Max
};

void test_init(void);
void eos_reactor_led_init(void);
void eos_sm_led_init(void);

void eos_sm_count(void);
void eos_reactor_count(void);

void timer_isr_1ms(void);

#endif
