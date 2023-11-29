
// include ---------------------------------------------------------------------
#include "bsp_uart.h"
#include "mlog/mlog.h"
#include "serial/dev_serial.h"
#include "dev_def.h"
#include "dal_def.h"
#include "mlib/m_mem.h"
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include "stdint.h"
#include "stdio.h"

M_TAG("Bsp-Uart-ttyUSB")

// static function -------------------------------------------------------------
#define BUFF_SERIAL_RS232_1_RX                      5120

// 数据结构 ---------------------------------------------------------------------
enum {
    Serial_Rs232_1 = 0,
    // Serial_Rs232_2,
    // Serial_Rs232_3,
    // Serial_Rs485_1,
    // Serial_Rs485_2,

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
        Serial_Rs232_1, &serial_232_1, DEV_RS232_RTK, "/dev/ttyUSB0", DevType_Normal,
        serial_232_1_buff_rx, BUFF_SERIAL_RS232_1_RX,
        0, false,
    },
};

// ops -------------------------------------------------------------------------
static dev_err_t drv_serial_config(dev_serial_t * serial, dev_serial_config_t * config);
static dev_err_t drv_serial_send(dev_serial_t * serial, void * p_buff, int size);
static void drv_serial_mode485_send(dev_serial_t * serial, bool send);
static void drv_serial_poll(dev_serial_t * serial, uint64_t time_sys_ms);

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
    drv_uart_data_t * data = (drv_uart_data_t *)serial->super.user_data;

    // 打开串口
    // TODO 以前是O_RDWR | O_NOCTTY | O_NOBLOCK的时候，会有串口关闭失败问题。
    //      现在这个问题反而不存在了，这是为什么？
    data->serial_fd = open(data->dev_name_linux, O_RDWR | O_NOCTTY | O_NDELAY);
    m_warn("fd %d %s\n",data->serial_fd, data->dev_name_linux);
    if (-1 == data->serial_fd)
        return Dev_Err_OpenFail;
    // resume the serial port to nonblock-state
    if (fcntl(data->serial_fd, F_SETFL, FNDELAY) < 0)
        return Dev_Error;
    // test if the serial port is a terminal-device
    if (0 == isatty(STDIN_FILENO))
        return Dev_Error;

    data->is_open = true;

    // 配置串口
    int speed_arr[] = {B1152000, B460800, B115200, B19200, B9600, B4800, B2400, B1200, B300};
    int name_arr[] = {1152000, 460800, 115200, 19200, 9600, 4800, 2400, 1200, 300};

    struct termios options;
    if (tcgetattr(data->serial_fd, &options) != 0)
        return Dev_Error;

    // set the input and output baudrate of the serial port
    for (uint8_t i = 0;  i < sizeof(speed_arr) / sizeof(int); i ++) {
        if (config->baud_rate == name_arr[i]) {
            cfsetispeed(&options, speed_arr[i]);
            cfsetospeed(&options, speed_arr[i]);
        }
    }

    // control mode
    options.c_cflag |= CLOCAL;
    options.c_cflag |= CREAD;
    // no data stream control
    options.c_cflag &= ~CRTSCTS;
    // data bits
    options.c_cflag &= ~CSIZE;
    options.c_iflag &= ~(ICRNL | IXON);

    switch (config->data_bits) {
        case 5: options.c_cflag |= CS5; break;
        case 6: options.c_cflag |= CS6; break;
        case 7: options.c_cflag |= CS7; break;
        case 8: options.c_cflag |= CS8; break;
        default:
            return Dev_Error;
    }
    // parity bits
    switch (config->parity) {
        case SerialParity_None:
            options.c_cflag &= ~PARENB;
            options.c_iflag &= ~INPCK;
            break;
        case SerialParity_Odd:
            options.c_cflag |= (PARODD | PARENB);
            options.c_iflag |= INPCK;
            break;
        case SerialParity_Even:
            options.c_cflag |= PARENB;
            options.c_cflag &= ~PARODD;
            options.c_iflag |= INPCK;
            break;
        default:
            return Dev_Error;
    }
    // stop bits
    switch (config->stop_bits) {
        case SerialStopBits_1:
            options.c_cflag &= ~CSTOPB;
            break;
        case SerialStopBits_2:
            options.c_cflag |= CSTOPB;
            break;
        default:
            return Dev_Error;
    }

    options.c_oflag &= ~OPOST;
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_cc[VTIME] = 1;
    options.c_cc[VMIN] = 1;

    tcflush(data->serial_fd, TCIFLUSH);

    // config
    if (tcsetattr(data->serial_fd, TCSANOW, &options) != 0)
        return Dev_Error;

    return Dev_OK;
}

static dev_err_t drv_serial_send(dev_serial_t * serial, void * buff, int size)
{
    drv_uart_data_t * data = (drv_uart_data_t *)serial->super.user_data;

    if (data->is_open == false)
        return Dev_Error;

    int count = write(data->serial_fd, buff, size);
    if (count != size) {
        tcflush(data->serial_fd, TCOFLUSH);
        return Dev_Error;
    }

    return Dev_OK;
}

static void drv_serial_poll(dev_serial_t * serial, uint64_t time_sys_ms)
{
    drv_uart_data_t * data = (drv_uart_data_t *)serial->super.user_data;
    if (data->is_open == false)
        return;
        
    int count = read(data->serial_fd, data->buff_rx, data->size_rx);
    if (count <= 0)
        return;

    serial_isr_recv(data->serial, data->buff_rx, count);
}

static void drv_serial_mode485_send(dev_serial_t * serial, bool send)
{
    (void)serial;
    (void)send;
}
