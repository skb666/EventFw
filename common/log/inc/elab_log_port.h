#ifndef ELAB_LOG_PORT_H_
#define ELAB_LOG_PORT_H_

#include "elab_log_cfg.h"
#include <stdint.h>

#if ELAB_LOG_TIMESTAMP_EN > 0x00
extern uint32_t elab_log_timestamp_get(void);
#endif
extern int32_t elab_log_port_output(const void *log, uint16_t size);
extern void elab_log_enter_critical_zone(void);
extern void elab_log_exit_critical_zone(void);

#endif
