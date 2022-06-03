
#include "esh.h"
#include <stdint.h>
#include <string.h>
#include "ecp.h"
#include "ecp_def.h"
#include "SEGGER_RTT.h"

void esh_port_init(void)
{
    SEGGER_RTT_Init();
}

void esh_port_send(void * data, uint32_t size)
{
    SEGGER_RTT_printf(0, "%s", (char *)data);
}

uint8_t esh_port_recv(void * data, uint8_t size)
{
    return 0;
}
