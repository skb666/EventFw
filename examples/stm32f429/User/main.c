/* include ------------------------------------------------------------------ */
#include "stm32f4xx.h"
#include "eventos.h"
#include "esh.h"
#include "elog.h"
#include <string.h>
#include "test.h"

EOS_TAG("Main")

void esh_flush(void)
{
}

int main(void)
{
    SystemCoreClockUpdate();
    
    if (SysTick_Config(SystemCoreClock / 1000) != 0)
        while (1);

    static elog_device_t dev_esh;
    dev_esh.enable = 1;
    dev_esh.level = eLogLevel_Debug;
    dev_esh.name = "eLogDev_Esh";
    dev_esh.out = esh_log;
    dev_esh.flush = esh_flush;
    dev_esh.ready = esh_ready;
    
    elog_init();
    elog_set_level(eLogLevel_Debug);
    esh_init();
    esh_start();
    
    elog_device_register(&dev_esh);
    elog_start();
    
    // Start EventOS.
    eos_init();                                     // EventOS初始化
    
    // Start EventOS Database.
    static uint8_t db_memory[5120];
    eos_db_init(db_memory, sizeof(db_memory));

    test_init();

    eos_run();                                      // EventOS启动

    return 0;
}

void SysTick_Handler(void)
{
    eos_interrupt_enter();
    eos_tick();
    eos_interrupt_exit();
}

void HardFault_Handler(void)
{
    EOS_ERROR("HardFault_Handler.");
    
    while (1) {
    }
}
