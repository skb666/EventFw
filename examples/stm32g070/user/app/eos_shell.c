//
// Created by jova on 2023/12/8.
//

#include "shell.h"
#include "elab_serial.h"
#include "elab_export.h"
#include "elab_log.h"
#include "elab_assert.h"
#include "event_def.h"
#include "eventos.h"

ELAB_TAG("ShellExport");

/* private config ----------------------------------------------------------- */
#define SHELL_BUFFER_SIZE                   (512)

/* data structure ----------------------------------------------------------- */
typedef struct eos_reactor_led_tag {
    eos_reactor_t super;
    eos_u8_t status;
} eos_reactor_shell_t;
eos_reactor_shell_t actor_shell;

/* private variables -------------------------------------------------------- */
static elab_device_t *device_shell = EOS_NULL;
static Shell shell_uart;
static char shell_uart_buffer[SHELL_BUFFER_SIZE];

static void shell_e_handler(eos_reactor_shell_t * const me, eos_event_t const * const e);


static int16_t serial_read(char *buf, uint16_t len)
{
    elab_assert(device_shell != EOS_NULL);
    return (int16_t)device_shell->ops->read(device_shell, 0, buf, len);
}

static int16_t serial_write(char *buf, uint16_t len)
{
    elab_assert(device_shell != EOS_NULL);
    return (int16_t)device_shell->ops->write(device_shell, 0, buf, len);
}

static void shell_uart_init(void)
{
    device_shell = elab_device_find("uart3");
    elab_device_open(device_shell);

    shell_uart.read = (int16_t (*)(char *, uint16_t)) serial_read;
    shell_uart.write = (int16_t (*)(char *, uint16_t)) serial_write;
    shellInit(&shell_uart, shell_uart_buffer, SHELL_BUFFER_SIZE);
}
INIT_EXPORT(shell_uart_init, EXPORT_MIDDLEWARE);

void shell_poll(void)
{
    eos_reactor_init(&actor_shell.super, 3, EOS_NULL);
    eos_reactor_start(&actor_shell.super, EOS_HANDLER_CAST(shell_e_handler));

    eos_event_sub((eos_actor_t *)(&actor_shell), Event_Input_Char);
}
INIT_EXPORT(shell_poll, EXPORT_APP);

/* static state function ---------------------------------------------------- */
static void shell_e_handler(eos_reactor_shell_t * const me, eos_event_t const * const e)
{
    if (e->topic == Event_Input_Char) {
        shellTask(&shell_uart);
    }
}

