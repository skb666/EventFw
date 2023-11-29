
#ifndef __DEV_CAN_H__
#define __DEV_CAN_H__

// include ---------------------------------------------------------------------
#include "devf.h"
#include "mlib/m_queue.h"

// config ----------------------------------------------------------------------
#define CAN_QUEUE_TX_SIZE                   160
#define CAN_QUEUE_RX_SIZE                   160

#define CAN_FILTER_NUM                      14

// data struct -----------------------------------------------------------------
// TODO 优化这个地方，使之用枚举定义，而不是直接用值。
enum {
    DevCanBaudRate_1M = 1000UL * 1000,     // 1 MBit/sec
    DevCanBaudRate_500K = 1000UL * 500,    // 500 kBit/sec
    DevCanBaudRate_250K = 1000UL * 250,    // 250 kBit/sec
    DevCanBaudRate_125K = 1000UL * 125,    // 125 kBit/sec
    DevCanBaudRate_100K = 1000UL * 100,    // 100 kBit/sec
};

enum {
    DevCanMode_Normal = 0,
    DevCanMode_Loopback,
};

enum {
    DevCanFilterMode_Mask = 0,
    DevCanFilterMode_List = !DevCanFilterMode_Mask,
};

typedef struct dev_can_config_tag {
    unsigned int baud_rate;
    int mode;
} dev_can_config_t;

#define DEV_CAN_DEFAULT_CONFIG {                                               \
    DevCanBaudRate_500K,                                                       \
    DevCanMode_Normal,                                                         \
};

typedef struct dev_can_msg_tag {
    uint32_t id             : 29;               // 帧ID
    uint32_t ide            : 1;                // 扩展帧
    uint32_t rtr            : 1;                // 远程帧
    uint32_t rsv            : 1;
    uint32_t len;
    uint8_t data[8];
} dev_can_msg_t;

typedef struct dev_can_filter_tag {
    // 扩展模式下，29位以上，是不起作用的；标准模式下，11位以上，是不起作用的。
    // List模式下，mask是无效的。
    uint32_t id;
    uint32_t mask;
    bool rtr;
    bool ide;
    int mode;
} dev_can_filter_t;

// class -----------------------------------------------------------------------
typedef struct dev_can_tag {
    device_t super;

    const struct dev_can_ops * ops;

    dev_can_config_t config;

    int evt_recv;
    int evt_send_end;
    bool send_busy;
    
    m_queue_t queue_tx;
    m_queue_t queue_rx;

    dev_can_filter_t filter[CAN_FILTER_NUM];
    int count_filter;
} dev_can_t;

struct dev_can_ops {
    dev_err_t (* enable)(dev_can_t * can, bool enable);
    dev_err_t (* config)(dev_can_t * can, dev_can_config_t * config);
    bool (* send)(dev_can_t * can, const dev_can_msg_t * msg);
    bool (* send_busy)(dev_can_t * can);
    void (* config_filter)(dev_can_t * can);
};

// API -------------------------------------------------------------------------
// BSP层
dev_err_t dev_can_reg(dev_can_t * can, const char * name, void * user_data);
void dev_can_isr_recv(dev_can_t * can, dev_can_msg_t * msg);

// 应用层
void dev_can_config(device_t * dev, dev_can_config_t * config);
void dev_can_send(device_t * dev, const dev_can_msg_t * msg);
int dev_can_recv(device_t * dev, dev_can_msg_t * msg);
void dev_can_set_evt(device_t * dev, int evt_recv, int evt_send_end);
void dev_can_add_filter(device_t * dev, dev_can_filter_t * filter);
void dev_can_config_filter(device_t * dev);
void dev_can_clear_filter(device_t * dev);

#endif
