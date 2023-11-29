
#ifndef __DEV_SERIAL_H__
#define __DEV_SERIAL_H__

#ifdef __cplusplus
extern "C" {
#endif

// include ---------------------------------------------------------------------
#include "devf.h"
#include "mlib/m_stream.h"

// include ---------------------------------------------------------------------
#define SERIAL_BUFF_SIZE                    2048
#define SERIAL_QUEUE_TX_SIZE                20480
#define SERIAL_QUEUE_RX_SIZE                20480

// macro definition ------------------------------------------------------------
enum {
    SerialStopBits_1 = 0, SerialStopBits_2,
};

enum {
    SerialParity_None = 0, SerialParity_Odd, SerialParity_Even
};

enum {
    SerialRs485_Send = 0, SerialRs485_Recv = !SerialRs485_Send
};

// data struct definition ------------------------------------------------------
// Default config for serial_configure structure.
#define DEV_SERIAL_CONFIG_DEFAULT {                                            \
    115200,                                         /* 115200 bits/s */        \
    8,                                              /* 8 databits */           \
    SerialStopBits_1,                               /* 1 stopbit */            \
    SerialParity_None,                              /* No parity  */           \
    0,                                              /* RS232 Mode */           \
    0                                                                          \
}

typedef struct dev_serial_config_tag {
    uint32_t baud_rate;
    uint32_t data_bits                  :4;
    uint32_t stop_bits                  :2;
    uint32_t parity                     :2;
    uint32_t fifo_size                  :16;
    uint32_t rs485                      :1;
    uint32_t reserved                   :7;
} dev_serial_config_t;

typedef struct dev_serial_tag {
    device_t super;

    const struct dev_serial_ops * ops;
    dev_serial_config_t config;
    uint8_t byte_bits;

    int rs485_mode;

    int evt_recv;

    m_stream_t stream_tx;
    m_stream_t stream_rx;

    uint64_t time_expect_read;                      // 串口期望读取时间
    uint64_t time_wait_rx;                          // 串口接收等待时间（30字节）
    uint64_t time_send;                             // 期望的下次发送时间
} dev_serial_t;

struct dev_serial_ops {
    dev_err_t (* config)(dev_serial_t * serial, dev_serial_config_t * config);
    dev_err_t (* send)(dev_serial_t * serial, void * p_buff, int size);
    void (* mode485_send)(dev_serial_t * serial, bool send);
    void (* poll)(dev_serial_t * serial, uint64_t time_sys_ms);
    void (* enter_critical)(dev_serial_t * serial);
    void (* exit_critical)(dev_serial_t * serial);
};

// API -------------------------------------------------------------------------
// BSP层调用
dev_err_t serial_reg(dev_serial_t * serial, const char * name, void * user_data);
void serial_isr_recv(dev_serial_t * serial, void * p_buff, int size);

// 应用层调用
void serial_config(device_t * dev, dev_serial_config_t * config);
void serial_send(device_t * dev, void * p_buff, int size);
int serial_recv(device_t * dev, void * p_buff, int size);
void serial_set_evt_recv(device_t * dev, int evt_id);
int serial_get_evt_recv(device_t * dev);

#ifdef __cplusplus
}
#endif

#endif

// Note ------------------------------------------------------------------------
/*  Serail设备的设计原则
    01  发送采用异步原则。
    02  如果接收到的数据没有在应用层被及时使用掉，那么就抛弃旧数据，保持新数据。
*/ 
