/* include ------------------------------------------------------------------ */
#include "eos.h"
#include <string.h>
#include <stdint.h>
#include "test.h"

int main(void)
{
    // Start EventOS.
    eos_init();                                     // EventOS初始化

    // Start EventOS Database.
    static uint8_t db_memory[5120];
    eos_db_init(db_memory, sizeof(db_memory));

    test_init();

    eos_kernel_start();                                      // EventOS启动

    return 0;
}
