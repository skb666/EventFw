/* include ------------------------------------------------------------------ */
#include <stdint.h>
#include <stdio.h>
#include <conio.h>
#include <assert.h>
#include <string.h>
#include "cmsis_os.h"
#include "elink.h"
#include "exsh.h"

static void exsh_func_exit(void *paras);
extern void exsh_func_normal_key_terminal(uint8_t key_id, void *paras);
extern uint8_t exsh_key_parser_terminal(uint8_t *key, uint8_t count);

static void func_read_mcu(void *paras);
static void func_write_mcu(void *paras);

extern bool elink_running(void);

int main(int argc, char ** argv)
{
    (void)argc;
    (void)argv;

    ex_shell_init(exsh_key_parser_terminal,
                  exsh_func_normal_key_terminal,
                  exsh_func_exit,
                  NULL);

    elink_init();

    osThreadNew(func_read_mcu, NULL, NULL);
    osThreadNew(func_write_mcu, NULL, NULL);

    ex_shell_run();

    while (elink_running());
    
    return 0;
}

static void exsh_func_exit(void *paras)
{
    printf("Func exit.\n");
}

static void func_read_mcu(void *paras)
{
    char buffer[1024];
    while (ex_shell_is_running())
    {
        memset(buffer, 0, 1024);
        uint16_t count = elink_read(buffer, 1024);

        if (count > 0)
        {
            printf("%s", buffer);
        }

        osDelay(10);
    }
}

static void func_write_mcu(void *paras)
{
    while (ex_shell_is_running())
    {
        elink_write("A", 1);

        osDelay(100);
    }
}
