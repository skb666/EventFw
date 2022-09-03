/* include ------------------------------------------------------------------ */
#include "eos.h"
#include <string.h>
#include <stdint.h>
#include "elog_test.h"

int main(void)
{
    /* Start EventOS. */
    eos_init();

    /* Start EventOS Database. */
    static uint8_t db_memory[512000];
    eos_db_init(db_memory, sizeof(db_memory));

    /* Start elog testing */
    elog_test_start();

    /* Start EventOS kernel. */
    eos_kernel_start();

    return 0;
}
