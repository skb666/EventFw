#include "bsp.h"
#include "stm32f4xx.h"
#include "test.h"

void bsp_init(void)
{
    SystemCoreClockUpdate();
    
    if (SysTick_Config(SystemCoreClock / 1000) != 0)
    {
        while (1);
    }
}

void timer_init(uint32_t time_ms)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    TIM_TimeBaseInitTypeDef timer_init;
    timer_init.TIM_Period = (10 * time_ms - 1);     // Tout=(ARR+1)(PSC+1)/Tclk   (4999+1)(8399+1)/(1/84M)
    timer_init.TIM_Prescaler = (8132 - 1);          // 9000
    timer_init.TIM_CounterMode = TIM_CounterMode_Up;
    timer_init.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM3, &timer_init);
    
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    NVIC_InitTypeDef nvic_init;
    nvic_init.NVIC_IRQChannel = TIM3_IRQn;
    nvic_init.NVIC_IRQChannelCmd=ENABLE;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 0x01;   //抢占优先级为1
    nvic_init.NVIC_IRQChannelSubPriority = 0x03;
    NVIC_Init(&nvic_init);

    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

    TIM_Cmd(TIM3, ENABLE);
}

uint32_t count_timer3 = 0;
void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
        
        count_timer3 ++;
        timer_isr_1ms();
    }
}
