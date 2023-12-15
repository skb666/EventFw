//
// Created by jova on 2023/12/1.
//

#ifndef EVENTOS_DRV_UART_H
#define EVENTOS_DRV_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g0xx_hal.h"
#include "elab_serial.h"

typedef struct drv_uart_config
{
    const char *name;
    USART_TypeDef *Instance;
    IRQn_Type irq_type;
} drv_uart_config_t;

typedef struct drv_uart
{
    UART_HandleTypeDef handle;
    drv_uart_config_t *config;
#ifdef SERIAL_USING_DMA

#endif
    elab_serial_t serial;
    uint8_t rx_data;
    uint8_t tx_data;
} drv_uart_t;

#ifdef __cpluscplus
{
#endif

#endif //EVENTOS_DRV_UART_H
