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

    eos_enter_critical();
    test_init();
    eos_exit_critical();

    eos_kernel_start();                             // EventOS启动

    return 0;
}

void SysTick_Handler(void)
{
    eos_interrupt_enter();
    eos_tick_increase();
    eos_interrupt_leave();
}
