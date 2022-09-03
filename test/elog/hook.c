#include "eos.h"
#include <stdio.h>
#include <assert.h>

eos_u32_t assert_id = 0;
void eos_port_assert(const char *tag, const char *name, eos_u32_t id)
{
    eos_hw_interrupt_disable();
    
    assert_id = id;
    printf("tag: %s, name: %s, id: %u.\n", tag, name, id);
    fflush(stdout);
    assert(false);
    
    while (1);
}
