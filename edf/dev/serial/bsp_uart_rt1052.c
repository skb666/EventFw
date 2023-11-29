
// include ---------------------------------------------------------------------
#include "bsp_uart.h"
#include "mlog/mlog.h"
#include "serial/dev_serial.h"
#include "dev_def.h"
#include "dal_def.h"
#include "mlib/m_mem.h"
#include "stdint.h"
#include "stdio.h"
#include "lpuart_cmsis_user.h"

M_TAG("Bsp-Uart-1052")

// 数据结构 ---------------------------------------------------------------------
enum {
    Serial_Rtk = 0,
    Serial_Time,
    Serial_IO,
    Serial_Wifi,

    Serial_Max,
};

typedef struct drv_uart_data {
    // 只读数据
    uint32_t serial_id;
    dev_serial_t *serial;
    const char *dev_name;
    uint8_t *buff_rx;
    uint32_t size_rx;
    // 可写数据
    uint64_t time_sys_ms_bkp;
} drv_uart_data_t;

// 数据 ------------------------------------------------------------------------
// 与RTK模块的通信串口（数据）
dev_serial_t serial_rtk;
uint8_t serial_buff_rx_rtk[BUFF_SERIAL_RX_RTK];

// 与RTK模块的通信串口（时间）
dev_serial_t serial_time;
uint8_t serial_buff_rx_time[BUFF_SERIAL_RX_TIME];

// 与IO扩展模块的串口
dev_serial_t serial_io;
uint8_t serial_buff_rx_io[BUFF_SERIAL_RX_IO];

// 与WIFI通信串口
dev_serial_t serial_wifi;
uint8_t serial_buff_rx_wifi[BUFF_SERIAL_RX_WIFI];

static drv_uart_data_t drv_data[Serial_Max] = {
    // RTK
    {
        Serial_Rtk,
        &serial_rtk,
        DEV_SERIAL_RTK,
        serial_buff_rx_rtk,
        BUFF_SERIAL_RX_RTK,
        0,
    },
    // Time
    {
        Serial_Time,
        &serial_time,
        DEV_SERIAL_TIME,
        serial_buff_rx_time,
        BUFF_SERIAL_RX_TIME,
        0,
    },
    // IO
    {
        Serial_IO,
        &serial_io,
        DEV_SERIAL_IO,
        serial_buff_rx_io,
        BUFF_SERIAL_RX_IO,
        0,
    },
    // Wifi
    {
        Serial_Wifi,
        &serial_wifi,
        DEV_SERIAL_WIFI,
        serial_buff_rx_wifi,
        BUFF_SERIAL_RX_WIFI,
        0,
    },
};

// ops -------------------------------------------------------------------------
static dev_err_t drv_serial_config(dev_serial_t *serial, dev_serial_config_t *config);
static dev_err_t drv_serial_send(dev_serial_t *serial, void *p_buff, int size);
static void drv_serial_mode485_send(dev_serial_t *serial, bool send);
static void drv_serial_poll(dev_serial_t *serial, uint64_t time_sys_ms);

static const struct dev_serial_ops serial_ops = {
    drv_serial_config,
    drv_serial_send,
    drv_serial_mode485_send,
    drv_serial_poll,
};

// api -------------------------------------------------------------------------
void bsp_uart_init(void)
{
    // 所有串口的底层配置
    for (int i = 0; i < Serial_Max; i++) {
        // Ops
        drv_data[i].serial->ops = &serial_ops;
        // 串口设备注册
        dev_err_t ret = serial_reg(drv_data[i].serial, drv_data[i].dev_name, (void *)&drv_data[i]);
        M_ASSERT_ID(i, ret == Dev_OK);
    }

    uart_init(&Driver_USART2, 115200, USART2_Callback);
    Driver_USART2.Receive(serial_buff_rx_rtk, BUFF_SERIAL_RX_RTK);
}

void bsp_uart_close(void)
{
    for (int i = 0; i < Serial_Max; i++)
    {
    }
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
    drv_uart_data_t *data = (drv_uart_data_t *)serial->super.user_data;

    switch (data->serial_id) {
    case Serial_Rtk:
        Driver_USART2.Send(buff, size);
        break;
    case Serial_Wifi:
        Driver_USART3.Send(buff, size);
        break;
    }

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
