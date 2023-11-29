
// include ---------------------------------------------------------------------
#include "stdint.h"
#include "dev_can.h"
#include "mdebug/m_assert.h"
#include "dev_def.h"
#include "moc.h"
#include "stdc/stdc_queue.h"

M_MODULE_NAME("Bsp-Can-Moc")

// 设备指针 ---------------------------------------------------------------------
enum {
    DevCanId_Can1 = 0, DevCanId_Can2, DevCanId_Max
};

dev_can_t dev_can[DevCanId_Max];

// data struct -----------------------------------------------------------------
typedef struct drv_can_data_tag {
    bool enable;
    bool send_busy;
    uint32_t baudrate;
    const char * name;
    m_queue_t sent_queue;
} drv_can_data_t;

static drv_can_data_t moc_data[DevCanId_Max] = {
    // CAN1
    { false, false, 125000, DEV_CAN_1 },
    // CAN2
    { false, false, 125000, DEV_CAN_2 }
};

// ops -------------------------------------------------------------------------
static dev_err_t drv_can_enable(dev_can_t * can, bool enable);
static dev_err_t drv_can_config(dev_can_t * can, dev_can_config_t * config);
static int drv_can_send(dev_can_t * can, const dev_can_msg_t * msg);
static bool drv_can_send_busy(dev_can_t * can);

static const struct dev_can_ops can_ops = {
    drv_can_enable, drv_can_config, drv_can_send, drv_can_send_busy
};

// API -------------------------------------------------------------------------
void bsp_can_init(void)
{
    for (int i = 0; i < DevCanId_Max; i ++) {
        dev_can[i].ops = &can_ops;
        // sent queue
        int result = m_queue_init(&moc_data[i].sent_queue, 16, sizeof(dev_can_msg_t), true, false);
        M_ASSERT(result == StdQueue_OK);
        // 注册
        dev_err_t ret = dev_can_reg(&dev_can[i], moc_data[i].name, (void *)&moc_data[i]);
        M_ASSERT(ret == Dev_OK);
        // config
        dev_can_config_t config_default = {
            DevCanBaudRate_125K, DevCanMode_Normal
        };
        drv_can_config(&dev_can[i], &config_default);
    }
}

static dev_err_t drv_can_config(dev_can_t * can, dev_can_config_t * config)
{
    drv_can_data_t * moc = (drv_can_data_t *)can->super.user_data;
    moc->baudrate = config->baud_rate;

    return Dev_OK;
}

static dev_err_t drv_can_enable(dev_can_t * can, bool enable)
{
    drv_can_data_t * moc = (drv_can_data_t *)can->super.user_data;
    moc->enable = enable;
    
    return Dev_OK;
}

static int drv_can_send(dev_can_t * can, const dev_can_msg_t * msg)
{
    drv_can_data_t * moc = (drv_can_data_t *)can->super.user_data;
    if (moc->enable == false)
        return 0;

    int ret = m_queue_push(&moc->sent_queue, (void *)msg);
    M_ASSERT(ret == StdQueue_OK);

    return 0;
}

static bool drv_can_send_busy(dev_can_t * can)
{
    drv_can_data_t * moc = (drv_can_data_t *)can->super.user_data;

    return moc->send_busy;
}

// 中断函数 ---------------------------------------------------------------------
void moc_can1_irq_recv(void * msg)
{
    dev_can_isr_recv(&dev_can[DevCanId_Can1], (dev_can_msg_t *)msg);
}

int moc_can1_sent_msg(void * msg)
{
    return m_queue_pull_pop(&moc_data[DevCanId_Can1].sent_queue, msg);
}

void moc_can2_irq_recv(void * msg)
{
    dev_can_isr_recv(&dev_can[DevCanId_Can2], (dev_can_msg_t *)msg);
}

int moc_can2_sent_msg(void * msg)
{
    return m_queue_pull_pop(&moc_data[DevCanId_Can1].sent_queue, msg);
}
