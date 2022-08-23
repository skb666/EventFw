/* include ------------------------------------------------------------------ */
#include "bsp.h"
#include "eos.h"
#include <string.h>
#include "test.h"

int main(void)
{
    bsp_init();

    // Start EventOS.
    eos_init();                                     // EventOS初始化
    
    // Start EventOS Database.
    static uint8_t db_memory[5120];
    eos_db_init(db_memory, sizeof(db_memory));

    test_init();

    eos_run();                                      // EventOS启动

    return 0;
}

uint32_t time_ms = 0;
void SysTick_Handler(void)
{
    time_ms ++;
    
    eos_interrupt_enter();
    eos_tick();
    eos_interrupt_leave();
}

void HardFault_Handler(void)
{
    while (1)
    {
    }
}
