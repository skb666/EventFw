/*
 * eLab Project
 * Copyright (c) 2023, EventOS Team, <event-os@outlook.com>
 */

#ifndef ELAB_COMMON_H
#define ELAB_COMMON_H

/* includes ----------------------------------------------------------------- */
#include <stdint.h>
#include "elab_config.h"

#if (ELAB_RTOS_CMSIS_OS_EN != 0)
#include "../os/cmsis_os.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* public typedef ----------------------------------------------------------- */
uint32_t elab_version(void);

/* time related. */
uint32_t elab_time_ms(void);
void elab_delay_us(void);

/* Memory management related. */
void *elab_malloc(uint32_t size);
void elab_free(void *memory);

/* MCU related */
void elab_mcu_reset(void);

/* uart debug related. */
void elab_debug_uart_init(uint32_t baudrate);
int16_t elab_debug_uart_send(void *buffer, uint16_t size);
int16_t elab_debug_uart_receive(void *buffer, uint16_t size);
void elab_assert_func(void);

#ifdef __cplusplus
}
#endif

#endif

/* ----------------------------- end of file -------------------------------- */
