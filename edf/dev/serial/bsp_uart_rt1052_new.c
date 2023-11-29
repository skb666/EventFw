
// include ---------------------------------------------------------------------
#include "bsp_uart.h"
#include "mlog/mlog.h"
#include "serial/dev_serial.h"
#include "dev_def.h"
#include "dal_def.h"
#include "mlib/m_mem.h"
#include "stdint.h"
#include "stdio.h"
#include "fsl_lpuart.h"
#include "fsl_lpuart_edma.h"
#include "fsl_dmamux.h"
#include "fsl_cache.h"

M_TAG("Bsp-Uart-1052")

// 数据结构 ---------------------------------------------------------------------
enum {
    Serial_Rtk = 0,
    Serial_Debug,
    Serial_Wifi,
    Serial_Else,
    
    Serial_Max,
};

typedef struct drv_uart_data {
    // 只读数据
    uint32_t serial_id;
    dev_serial_t *serial;
    const char *dev_name;
    uint8_t *buff_tx;
    uint8_t *buff_rx;
    uint32_t size_rx;
    LPUART_Type * uart;
    uint32_t dma_source_tx;
    uint32_t dma_source_rx;
    uint32_t dma_channel_tx;
    uint32_t dma_channel_rx;
    lpuart_edma_transfer_callback_t callback;
    IRQn_Type irq;
    // 可写数据
    uint64_t time_sys_ms_bkp;
    edma_handle_t * dma_handle_tx;
    edma_handle_t * dma_handle_rx;
    lpuart_edma_handle_t * dma_uart_handle;
    lpuart_transfer_t * xfer_tx;
    lpuart_transfer_t * xfer_rx;
    uint32_t count_recv;
} drv_uart_data_t;

// call back -------------------------------------------------------------------
static void lpuart8_edma_transfer_callback( LPUART_Type *base,
                                            lpuart_edma_handle_t *handle,
                                            status_t status,
                                            void *userData) {}
static void lpuart6_edma_transfer_callback( LPUART_Type *base,
                                            lpuart_edma_handle_t *handle,
                                            status_t status,
                                            void *userData) {}
static void lpuart3_edma_transfer_callback( LPUART_Type *base,
                                            lpuart_edma_handle_t *handle,
                                            status_t status,
                                            void *userData) {}
static void lpuart5_edma_transfer_callback( LPUART_Type *base,
                                            lpuart_edma_handle_t *handle,
                                            status_t status,
                                            void *userData) {}

// 数据 ------------------------------------------------------------------------
// 调试串口
dev_serial_t serial_debug;
static edma_handle_t edma_handle_tx_debug;
static edma_handle_t edma_handle_rx_debug;
static lpuart_edma_handle_t edma_uart_handle_debug;
static lpuart_transfer_t xfer_send_debug;
static lpuart_transfer_t xfer_recv_debug;
AT_NONCACHEABLE_SECTION(static uint8_t serial_buff_tx_debug[BUFF_SERIAL_RX_DEBUG]);
AT_NONCACHEABLE_SECTION(static uint8_t serial_buff_rx_debug[BUFF_SERIAL_RX_DEBUG]);

// RTK串口
dev_serial_t serial_rtk;
edma_handle_t edma_handle_tx_rtk;
edma_handle_t edma_handle_rx_rtk;
lpuart_edma_handle_t edma_uart_handle_rtk;
lpuart_transfer_t xfer_send_rtk;
lpuart_transfer_t xfer_recv_rtk;
AT_NONCACHEABLE_SECTION(static uint8_t serial_buff_tx_rtk[BUFF_SERIAL_RX_RTK]);
AT_NONCACHEABLE_SECTION(uint8_t serial_buff_rx_rtk[BUFF_SERIAL_RX_RTK]);

// Wifi串口
dev_serial_t serial_wifi;
static edma_handle_t edma_handle_tx_wifi;
static edma_handle_t edma_handle_rx_wifi;
static lpuart_edma_handle_t edma_uart_handle_wifi;
static lpuart_transfer_t xfer_send_wifi;
static lpuart_transfer_t xfer_recv_wifi;
AT_NONCACHEABLE_SECTION(uint8_t serial_buff_tx_wifi[BUFF_SERIAL_RX_WIFI]);
AT_NONCACHEABLE_SECTION(uint8_t serial_buff_rx_wifi[BUFF_SERIAL_RX_WIFI]);

// Else
dev_serial_t serial_else;
static edma_handle_t edma_handle_tx_else;
static edma_handle_t edma_handle_rx_else;
static lpuart_edma_handle_t edma_uart_handle_else;
static lpuart_transfer_t xfer_send_else;
static lpuart_transfer_t xfer_recv_else;
AT_NONCACHEABLE_SECTION(static uint8_t serial_buff_tx_else[BUFF_SERIAL_RX_ELSE]);
AT_NONCACHEABLE_SECTION(static uint8_t serial_buff_rx_else[BUFF_SERIAL_RX_ELSE]);

drv_uart_data_t drv_data[Serial_Max] = {
    // Rtk
    {
        Serial_Rtk,
        &serial_rtk,
        DEV_SERIAL_RTK,
        serial_buff_tx_rtk, serial_buff_rx_rtk, BUFF_SERIAL_RX_RTK,
        LPUART8, kDmaRequestMuxLPUART8Tx, kDmaRequestMuxLPUART8Rx, 4, 5,
        lpuart8_edma_transfer_callback, LPUART8_IRQn,
        0,
        &edma_handle_tx_rtk, &edma_handle_rx_rtk, &edma_uart_handle_rtk,
        &xfer_send_rtk, &xfer_recv_rtk,
        0,
    },
    // Debug
    {
        Serial_Debug,
        &serial_debug,
        DEV_SERIAL_DEBUG,
        serial_buff_tx_debug, serial_buff_rx_debug, BUFF_SERIAL_RX_DEBUG,
        LPUART6, kDmaRequestMuxLPUART6Tx, kDmaRequestMuxLPUART6Rx, 8, 9,
        lpuart6_edma_transfer_callback, LPUART6_IRQn,
        0,
        &edma_handle_tx_debug, &edma_handle_rx_debug, &edma_uart_handle_debug,
        &xfer_send_debug, &xfer_recv_debug,
        0,
    },
    // Wifi
    {
        Serial_Wifi,
        &serial_wifi,
        DEV_SERIAL_WIFI,
        serial_buff_tx_wifi, serial_buff_rx_wifi, BUFF_SERIAL_RX_WIFI,
        LPUART3, kDmaRequestMuxLPUART3Tx, kDmaRequestMuxLPUART3Rx, 0, 1,
        lpuart3_edma_transfer_callback, LPUART3_IRQn,
        0,
        &edma_handle_tx_wifi, &edma_handle_rx_wifi, &edma_uart_handle_wifi,
        &xfer_send_wifi, &xfer_recv_wifi,
        0,
    },
    // Else
    {
        Serial_Else,
        &serial_else,
        DEV_SERIAL_ELSE,
        serial_buff_tx_else, serial_buff_rx_else, BUFF_SERIAL_RX_ELSE,
        LPUART5, kDmaRequestMuxLPUART5Tx, kDmaRequestMuxLPUART5Rx, 6, 7,
        lpuart5_edma_transfer_callback, LPUART5_IRQn,
        0,
        &edma_handle_tx_else, &edma_handle_rx_else, &edma_uart_handle_else,
        &xfer_send_else, &xfer_recv_else,
        0,
    },
};

// ops -------------------------------------------------------------------------
static dev_err_t drv_serial_config(dev_serial_t *serial, dev_serial_config_t *config);
static dev_err_t drv_serial_send(dev_serial_t *serial, void *p_buff, int size);
static void drv_serial_mode485_send(dev_serial_t *serial, bool send);
static void drv_serial_poll(dev_serial_t *serial, uint64_t time_sys_ms);
static void drv_serial_enter_critical(dev_serial_t *serial);
static void drv_serial_exit_critical(dev_serial_t *serial);

static const struct dev_serial_ops serial_ops = {
    drv_serial_config,
    drv_serial_send,
    drv_serial_mode485_send,
    drv_serial_poll,
    drv_serial_enter_critical,
    drv_serial_exit_critical,
};

static uint32_t LPUARTx_GetFreq(void)
{
    uint32_t freq;

    /* To make it simple, we assume default PLL and divider settings, and the only variable
       from application is use PLL3 source or OSC source */
    if (CLOCK_GetMux(kCLOCK_UartMux) == 0) { /* PLL3 div6 80M */
        freq = (CLOCK_GetPllFreq(kCLOCK_PllUsb1) / 6U) / (CLOCK_GetDiv(kCLOCK_UartDiv) + 1U);
    }
    else {
        freq = CLOCK_GetOscFreq() / (CLOCK_GetDiv(kCLOCK_UartDiv) + 1U);
    }

    return freq;
}

// api -------------------------------------------------------------------------
static void bsp_uart_dma_init(int id)
{
    lpuart_config_t lpuart_config;
    
    // 获取默认配置并进行修改
    LPUART_GetDefaultConfig(&lpuart_config);
    lpuart_config.rxIdleConfig = kLPUART_IdleCharacter16;
    lpuart_config.rxIdleType = kLPUART_IdleTypeStopBit;
    lpuart_config.enableTx = true;
    lpuart_config.enableRx = true;
    lpuart_config.baudRate_Bps = 115200;
    lpuart_config.dataBitsCount = kLPUART_EightDataBits;
    lpuart_config.parityMode = kLPUART_ParityDisabled;
    lpuart_config.stopBitCount = kLPUART_OneStopBit;

    // 串口初始化
    LPUART_Init(drv_data[id].uart, &lpuart_config, LPUARTx_GetFreq());

    // DMA通道配置
    DMAMUX_SetSource(DMAMUX, drv_data[id].dma_channel_tx, drv_data[id].dma_source_tx);
    DMAMUX_SetSource(DMAMUX, drv_data[id].dma_channel_rx, drv_data[id].dma_source_rx);
    DMAMUX_EnableChannel(DMAMUX, drv_data[id].dma_channel_tx);
    DMAMUX_EnableChannel(DMAMUX, drv_data[id].dma_channel_rx);

    EDMA_CreateHandle(drv_data[id].dma_handle_tx, DMA0, drv_data[id].dma_channel_tx);
    EDMA_CreateHandle(drv_data[id].dma_handle_rx, DMA0, drv_data[id].dma_channel_rx);

    // 创建LPUART DMA Handle
    LPUART_TransferCreateHandleEDMA(drv_data[id].uart, drv_data[id].dma_uart_handle,
                                    drv_data[id].callback, NULL,
                                    drv_data[id].dma_handle_tx,
                                    drv_data[id].dma_handle_rx);

    // 中断使能
    LPUART_EnableInterrupts(drv_data[id].uart, kLPUART_IdleLineInterruptEnable);
    EnableIRQ(drv_data[id].irq);

    // DMA接收
    drv_data[id].xfer_rx->data = drv_data[id].buff_rx;
    drv_data[id].xfer_rx->dataSize = drv_data[id].size_rx;
    LPUART_ReceiveEDMA(drv_data[id].uart, drv_data[id].dma_uart_handle, drv_data[id].xfer_rx);
}

void bsp_uart_init(void)
{
    edma_config_t edma_config;

    // DMA初始化
    DMAMUX_Init(DMAMUX);
    // 获取默认DMA配置
    EDMA_GetDefaultConfig(&edma_config);
    EDMA_Init(DMA0, &edma_config);
    
    // 所有串口的底层配置
    for (int i = 0; i < Serial_Max; i++) {
        bsp_uart_dma_init(i);
        // Ops
        drv_data[i].serial->ops = &serial_ops;
        // 串口设备注册
        dev_err_t ret = serial_reg(drv_data[i].serial, drv_data[i].dev_name, (void *)&drv_data[i]);
        M_ASSERT_ID(i, ret == Dev_OK);
    }
}

void bsp_uart_assert_send(void *buff, int size)
{
    L1CACHE_CleanDCacheByRange((uint32_t)buff, size);
    bsp_uart_dma_init(Serial_Debug);
    drv_serial_send(&serial_debug, buff, size);
}

// static function -------------------------------------------------------------
static dev_err_t drv_serial_config(dev_serial_t *serial, dev_serial_config_t *config)
{
    (void)serial;
    (void)config;
    
    return Dev_OK;
}

static dev_err_t drv_serial_send(dev_serial_t *serial, void *buff, int size)
{
    drv_uart_data_t * data = (drv_uart_data_t *)serial->super.user_data;
    M_ASSERT(size < data->size_rx);
    
    memcpy(data->buff_tx, buff, size);
    data->xfer_tx->data = data->buff_tx;
    data->xfer_tx->dataSize = size;
    LPUART_SendEDMA(data->uart, data->dma_uart_handle, data->xfer_tx);

    return Dev_OK;
}

static void drv_serial_poll(dev_serial_t *serial, uint64_t time_sys_ms)
{
    (void)serial;
    (void)time_sys_ms;
}

static void drv_serial_mode485_send(dev_serial_t *serial, bool send)
{
    (void)serial;
    (void)send;
}

static void drv_serial_enter_critical(dev_serial_t *serial)
{
    drv_uart_data_t * data = (drv_uart_data_t *)serial->super.user_data;
    DisableIRQ(drv_data[data->serial_id].irq);
}

static void drv_serial_exit_critical(dev_serial_t *serial)
{
    drv_uart_data_t * data = (drv_uart_data_t *)serial->super.user_data;
    EnableIRQ(drv_data[data->serial_id].irq);
}

// irq -------------------------------------------------------------------------
static void lpuat_idle_handler(uint32_t serial_id)
{
    uint8_t i = serial_id;
    if ((kLPUART_IdleLineFlag) & LPUART_GetStatusFlags(drv_data[i].uart)) {
        LPUART_ClearStatusFlags(drv_data[i].uart, kLPUART_IdleLineFlag);
        
        LPUART_TransferGetReceiveCountEDMA( drv_data[i].uart,
                                            drv_data[i].dma_uart_handle,
                                            &drv_data[i].count_recv);
        if (drv_data[i].count_recv == 0) {
            bsp_uart_dma_init(i);
            return;
        }
        
        LPUART_TransferAbortReceiveEDMA(drv_data[i].uart, drv_data[i].dma_uart_handle);

        if (drv_data[i].serial->super.en == true &&
            drv_data[i].serial->super.en_poll == true) {
            serial_isr_recv(drv_data[i].serial,
                            drv_data[i].buff_rx,
                            drv_data[i].count_recv);
        }
        drv_data[i].buff_rx[0] = 0;
        
        LPUART_ReceiveEDMA( drv_data[i].uart,
                            drv_data[i].dma_uart_handle,
                            drv_data[i].xfer_rx);
    }
}

void LPUART8_IRQHandler(void)
{
    lpuat_idle_handler(Serial_Rtk);
    
    __DSB();
}

void LPUART6_IRQHandler(void)
{
    lpuat_idle_handler(Serial_Debug);
    
    __DSB();
}

void LPUART5_IRQHandler(void)
{
    lpuat_idle_handler(Serial_Else);
    
    __DSB();
}

void LPUART3_IRQHandler(void)
{
    lpuat_idle_handler(Serial_Wifi);
    
    __DSB();
}
