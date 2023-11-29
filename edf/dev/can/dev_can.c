
// incldue ---------------------------------------------------------------------
#include "dev_can.h"
#include "mlog/mlog.h"
#include "port_queue.h"

M_TAG("Dev-Can")

// private function ------------------------------------------------------------
static void dev_can_poll(device_t * dev, uint64_t time_system_ms);
static dev_err_t dev_can_enable(device_t * dev, bool enable);

// API -------------------------------------------------------------------------
dev_err_t dev_can_reg(dev_can_t * can, const char * name, void * user_data)
{
    device_t * dev = &(can->super);

    dev->poll = dev_can_poll;
    dev->enable = dev_can_enable;
    dev->user_data = user_data;

    // apply configuration
    M_ASSERT(can->ops->config != (void *)0);
    can->config = (dev_can_config_t)DEV_CAN_DEFAULT_CONFIG;
    dev_err_t err = can->ops->config(can, &can->config);
    M_ASSERT_INFO(err == Dev_OK, "line: %d, err: %d.", __LINE__, err);

    // queue
    int ret = 0;
    int size = sizeof(dev_can_msg_t);
    // 接收队列和发送队列
    ret = m_queue_init(&can->queue_rx, CAN_QUEUE_RX_SIZE, size);
    M_ASSERT_INFO(ret == Queue_OK, "line: %d, ret: %d.", __LINE__, ret);
    ret = m_queue_init(&can->queue_tx, CAN_QUEUE_TX_SIZE, size);
    M_ASSERT_INFO(ret == Queue_OK, "line: %d, ret: %d.", __LINE__, ret);

    // 滤波器的初始化
    can->count_filter = 0;
    can->send_busy = false;

    return device_reg(dev, name, DevType_UseOneTime);
}

void dev_can_isr_recv(dev_can_t * can, dev_can_msg_t * msg)
{
    if (m_queue_full(&can->queue_rx) == true) {
        m_queue_clear(&can->queue_rx);
    }
    int ret = m_queue_push(&can->queue_rx, (void *)msg, 1);
    M_ASSERT_INFO(ret == Queue_OK, "line: %d, ret: %d.", __LINE__, ret);
}

void dev_can_config(device_t * dev, dev_can_config_t * config)
{
    dev_can_t * can = (dev_can_t *)dev;
    can->config.baud_rate = config->baud_rate;
    can->config.mode = config->mode;
    can->ops->config(can, config);
}

void dev_can_send(device_t * dev, const dev_can_msg_t * msg)
{
    dev_can_t * can = (dev_can_t *)dev;
    int ret = m_queue_push(&can->queue_tx, (void *)msg, 1);
    M_ASSERT_INFO(ret == Queue_OK, "line: %d, ret: %d.", __LINE__, ret);
}

int dev_can_recv(device_t * dev, dev_can_msg_t * msg)
{
    dev_can_t * can = (dev_can_t *)dev;
    if (m_queue_empty(&can->queue_rx) == true)
        return Dev_Err_Empty;

    devf_port_irq_disable();
    int ret = m_queue_pull_pop(&can->queue_rx, msg, 1);
    devf_port_irq_enable();
    M_ASSERT_INFO(ret == 1, "line: %d, ret: %d.", __LINE__, ret);

    return Dev_OK;
}

void dev_can_set_evt(device_t * dev, int evt_recv, int evt_send_end)
{
    ((dev_can_t *)dev)->evt_recv = evt_recv;
    ((dev_can_t *)dev)->evt_send_end = evt_send_end;
}

void dev_can_add_filter(device_t * dev, dev_can_filter_t * filter)
{
    dev_can_t * can = (dev_can_t *)dev;

    M_ASSERT(can->count_filter >= CAN_FILTER_NUM);

    int i = can->count_filter;
    can->filter[i].id = filter->id;
    can->filter[i].mask = filter->mask;
    can->filter[i].ide = filter->ide;
    can->filter[i].mode = filter->mode;
    can->filter[i].rtr = filter->rtr;
}

void dev_can_config_filter(device_t * dev)
{
    dev_can_t * can = (dev_can_t *)dev;
    can->ops->config_filter(can);
}

void dev_can_clear_filter(device_t * dev)
{
    dev_can_t * can = (dev_can_t *)dev;
    can->count_filter = 0;
}

// private function ------------------------------------------------------------
static void dev_can_poll(device_t * dev, uint64_t time_system_ms)
{
    (void)time_system_ms;
    
    dev_can_t * can = (dev_can_t *)dev;
    can->send_busy = can->ops->send_busy(can);
    if (can->send_busy == true) {
        return;
    }
    
    dev_can_msg_t msg;
    int ret;
    while (m_queue_empty(&can->queue_tx) == false) {
        ret = m_queue_pull(&can->queue_tx, (void *)&msg, 1);
        M_ASSERT_INFO(ret == 1, "Can Queue_Tx Pull. Line: %d, ret: %d.", __LINE__, ret);
        if (can->ops->send(can, &msg) == false) {
            break;
        }
        ret = m_queue_pop(&can->queue_tx, 1);
        M_ASSERT_INFO(ret == 1, "Can Queue_Tx Pop. Line: %d, ret: %d.", __LINE__, ret);
    }
}

static dev_err_t dev_can_enable(device_t * dev, bool enable)
{
    dev_can_t * can = (dev_can_t *)dev;

    return can->ops->enable(can, enable);
}
