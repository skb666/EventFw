#include "elab_log_port.h"
#include <stdio.h>


#if ELAB_LOG_TIMESTAMP_EN > 0x00
__attribute__((weak)) uint32_t elab_log_timestamp_get(void)
{
    uint32_t timestamp = 0;

    return timestamp;
}
#endif

__attribute__((weak)) int32_t elab_log_port_output(const void *log, uint16_t size)
{
    return printf(log);
}

__attribute__((weak)) void elab_log_enter_critical_zone(void)
{
    ;
}

__attribute__((weak)) void elab_log_exit_critical_zone(void)
{
    ;
}
