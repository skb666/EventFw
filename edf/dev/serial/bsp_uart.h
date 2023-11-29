#ifndef __BSP_UART_H__
#define __BSP_UART_H__

// include ---------------------------------------------------------------------
#include "devf.h"

// buffer config ---------------------------------------------------------------
#define BUFF_SERIAL_RX_DEBUG                    2048
#define BUFF_SERIAL_RX_RTK                      512
#define BUFF_SERIAL_RX_WIFI                     20480
#define BUFF_SERIAL_RX_ELSE                     2048

// API -------------------------------------------------------------------------
// 所有串口的初始化和设备注册
void bsp_uart_init(void);
// 调试串口发送，不依赖于设备框架和内核，用于断言的打印，以便在内核和框架停止工作的时候，
// 仍然能够将断言信息打印到串口上。
void bsp_uart_assert_send(void * buff, int size);

#endif
