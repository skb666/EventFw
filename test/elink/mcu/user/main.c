/* include ------------------------------------------------------------------ */
#include "stm32f4xx.h"
#include <string.h>
#include <stdio.h>
#include "elink.h"
#include "eos.h"
#include <stdint.h>

/* private variables -------------------------------------------------------- */
uint32_t str_size = 0;
uint32_t test_count = 0;

uint32_t time_ms = 0;
uint32_t speed = 0;

/* private function --------------------------------------------------------- */
static void entry_elink_write_test(void *paras);
static void entry_elink_read_test(void *paras);

typedef struct task_test
{
    eos_task_t *task;
    const char *name;
    uint8_t prio;
    void *stack;
    uint32_t stack_size;
    void (* func)(void *parameter);
} task_test_info_t;

static uint64_t stack_elink_write[64];
static eos_task_t task_elink_write;
static uint64_t stack_elink_read[64];
static eos_task_t task_elink_read;

static const task_test_info_t task_test_info[] =
{
    {
        &task_elink_write, "TaskGive1", 1,
        stack_elink_write, sizeof(stack_elink_write),
        entry_elink_write_test
    },
    {
        &task_elink_read, "TaskGive2", 2,
        stack_elink_read, sizeof(stack_elink_read),
        entry_elink_read_test
    },
};

/* main function ------------------------------------------------------------ */
int main(void)
{
    SystemCoreClockUpdate();
    
    if (SysTick_Config(SystemCoreClock / 1000) != 0)
        while (1);

    // Start EventOS.
    eos_init();
    
    // Start EventOS Database.
    static uint8_t db_memory[5120];
    eos_db_init(db_memory, sizeof(db_memory));
    
    elink_init();

    for (uint32_t i = 0;
         i < (sizeof(task_test_info) / sizeof(task_test_info_t));
         i ++)
    {
        eos_task_init(task_test_info[i].task,
                       task_test_info[i].name,
                       task_test_info[i].func,
                       EOS_NULL,
                       task_test_info[i].stack,
                       task_test_info[i].stack_size,
                       task_test_info[i].prio);
        eos_task_startup(task_test_info[i].task);
    }

    eos_kernel_start();
}

/* ISR function ------------------------------------------------------------- */
void SysTick_Handler(void)
{
    eos_interrupt_enter();
    eos_tick_increase();
    eos_interrupt_leave();
    
    time_ms ++;
}

/* private function --------------------------------------------------------- */
static char str[256];
uint32_t write_byte_count = 0;
static void entry_elink_write_test(void *paras)
{
    while (1)
    {
        str[0] = 0;
        sprintf(str, "Log Test: %8u\n", test_count ++);
        str_size = strlen(str);

        elink_write(str, str_size);
        write_byte_count += str_size;
        speed = write_byte_count / time_ms;
    }

}

static void entry_elink_read_test(void *paras)
{
    uint8_t byte = 0;

    while (1)
    {
        elink_read(&byte, 1);
        eos_task_delay_ms(1);
    }
}
