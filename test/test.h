#ifndef TEST_H
#define TEST_H

#include "eventos_config.h"

#define TEST_TIME_MAX                   10000
#define TEST_MODE                       0

#if (TEST_MODE == 0)

#define TEST_EN_01_0                    0
#define TEST_EN_01_1                    0
#define TEST_EN_02_0                    0
#define TEST_EN_02_1                    0
#define TEST_EN_02_2                    0
#define TEST_EN_03                      1
#define TEST_EN_04                      0
#define TEST_EN_05_0                    0
#define TEST_EN_05_1                    0
#define TEST_EN_06                      0
#define TEST_EN_07                      0
#define TEST_EN_09                      0

#endif

enum
{
    TaskPrio_High = 0,
    TaskPrio_ReacotrLed,
    TaskPrio_SmLed,
    TaskPrio_Value,
    TaskPrio_Middle,
    TaskPrio_Give1,
    TaskPrio_Give2 = TaskPrio_Give1,

    TaskPrio_Max
};

void test_init(void);
void test_general(void);

void eos_reactor_led_init(void);
void eos_sm_led_init(void);

void eos_sm_count(void);
void eos_reactor_count(void);
void eos_idle_count(void);

void timer_isr_1ms(void);

#endif
