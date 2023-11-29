
// include ---------------------------------------------------------------------
#include "bsp_uart.h"
#include "serial/dev_serial.h"
#include "dev_def.h"
#include "dal_def.h"
#include "mlib/m_mem.h"
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include "stdint.h"
#include "stdio.h"
#include "mlog/mlog.h"

M_TAG("Bsp-Uart-Moc")

// static function -------------------------------------------------------------
#define BUFF_SERIAL_RS232_1_RX                      5120

// 数据结构 ---------------------------------------------------------------------
enum {
    Serial_Rs232_1 = 0,

    Serial_Max,
};

typedef struct drv_uart_data {
    // 只读数据
    uint32_t serial_id;
    dev_serial_t * serial;
    const char * dev_name;
    const char * dev_name_linux;
    dev_type_t dev_type;
    uint8_t * buff_rx;
    uint32_t size_rx;
    // 可写数据
    int serial_fd;
    bool is_open;
    uint64_t time_sys_ms_bkp;
} drv_uart_data_t;

// 数据 ------------------------------------------------------------------------
// 调试串口
dev_serial_t serial_232_1;
uint8_t serial_232_1_buff_rx[BUFF_SERIAL_RS232_1_RX];

static drv_uart_data_t drv_data[Serial_Max] = {
    // Serial_Debug 调试串口
    {
        Serial_Rs232_1, &serial_232_1, DEV_SERIAL_RTK, "/dev/ttyUSB1", DevType_Normal,
        serial_232_1_buff_rx, BUFF_SERIAL_RS232_1_RX,
        0, false,
    },
};

// ops -------------------------------------------------------------------------
static dev_err_t drv_serial_config(dev_serial_t * serial, dev_serial_config_t * config);
static dev_err_t drv_serial_send(dev_serial_t * serial, void * p_buff, int size);
static void drv_serial_mode485_send(dev_serial_t * serial, bool send);
static void drv_serial_poll(dev_serial_t * serial, uint64_t time_sys_ms);
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

// api -------------------------------------------------------------------------
void bsp_uart_init(void)
{
    // 所有串口的底层配置
    for (int i = 0; i < Serial_Max; i ++) {
        // Ops
        drv_data[i].serial->ops = &serial_ops;
        // 串口设备注册
        dev_err_t ret = serial_reg(drv_data[i].serial, drv_data[i].dev_name, (void *)&drv_data[i]);
        M_ASSERT_ID(i, ret == Dev_OK);
    }
}

void bsp_uart_close(void)
{
    for (int i = 0; i < Serial_Max; i ++) {
        close(drv_data[i].serial_fd);
    }
}

// static function -------------------------------------------------------------
static dev_err_t drv_serial_config(dev_serial_t * serial, dev_serial_config_t * config)
{
    (void)serial;
    (void)config;

    return Dev_OK;
}

static dev_err_t drv_serial_send(dev_serial_t * serial, void * buff, int size)
{

    return Dev_OK;
}

static void drv_serial_poll(dev_serial_t * serial, uint64_t time_sys_ms)
{
}

static void drv_serial_mode485_send(dev_serial_t * serial, bool send)
{
    (void)serial;
    (void)send;
}

static void drv_serial_enter_critical(dev_serial_t *serial)
{
    (void)serial;
}

static void drv_serial_exit_critical(dev_serial_t *serial)
{
    (void)serial;
}
