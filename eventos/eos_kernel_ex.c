#include "eos_kernel.h"
#include "eventos.h"

eos_err_t eos_task_init(eos_task_t *task,
                        const char *name,
                        void (*entry)(void *parameter),
                        void *parameter,
                        void *stack_start,
                        eos_u32_t stack_size,
                        eos_u8_t priority,
                        eos_u32_t tick)
{
    return ek_task_init(&task->task_,
                        name, entry, parameter,
                        stack_start, stack_size,
                        priority, tick);
}