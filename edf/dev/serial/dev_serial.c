
// include ---------------------------------------------------------------------
#include "serial/dev_serial.h"
#include "mlog/mlog.h"
#include "meow.h"
#include "dev_def.h"

M_TAG("Dev-Serial")

// static function -------------------------------------------------------------
static void serial_poll(device_t * dev, uint64_t time_system_ms);

const char * serial_name;
int ret_value = 0;

// API -------------------------------------------------------------------------
dev_err_t serial_reg(dev_serial_t * serial, const char * name, void * user_data)
{
    device_t * dev = &(serial->super);
    device_clear(dev);

    // interface
    dev->poll = serial_poll;
    // user data
    dev->user_data = user_data;
    // data
    serial->config = (dev_serial_config_t)DEV_SERIAL_CONFIG_DEFAULT;
    serial->byte_bits = 2 + serial->config.data_bits + serial->config.stop_bits;
    serial->evt_recv = 0;
    // apply configuration
    M_ASSERT(serial->ops->config != (void *)0);
    dev_err_t err = serial->ops->config(serial, &serial->config);
    M_ASSERT(err == Dev_OK);
    // stream
    int ret = 0;
    ret = m_stream_init(&serial->stream_rx, SERIAL_QUEUE_RX_SIZE);
    M_ASSERT(ret == Stream_OK);
    ret = m_stream_init(&serial->stream_tx, SERIAL_QUEUE_TX_SIZE);
    M_ASSERT(ret == Stream_OK);

    // register a character device
    return device_reg(dev, name, DevType_UseOneTime);
}

void serial_isr_recv(dev_serial_t * serial, void * p_buff, int size)
{
    if (size == 0)
        return;

    int ret = m_stream_push(&serial->stream_rx, p_buff, size);
    if (ret != Stream_OK) {
        serial_name = serial->super.name;
        ret_value = ret;
        ret = ret;
        
        M_ERROR(    "Line: %d, Name: %s, Ret: %d. Size: %d, EmptySize: %d, Head: %d, Tail: %d.",
                     __LINE__, serial->super.name, ret, size,
                     m_stream_empty_size(&serial->stream_rx),
                     serial->stream_rx.head, serial->stream_rx.tail);
        return;
    }

//    M_ASSERT_INFO(ret == Stream_OK,
//                  "Line: %d, Name: %s, Ret: %d. Size: %d, EmptySize: %d, Head: %d, Tail: %d.",
//                  __LINE__, serial->super.name, ret, size,
//                  m_stream_empty_size(&serial->stream_rx),
//                  serial->stream_rx.head, serial->stream_rx.tail);

    serial->time_expect_read = serial->time_wait_rx + devf_port_get_time_ms();
}

void serial_config(device_t * dev, dev_serial_config_t * config)
{
    dev_serial_t * serial = (dev_serial_t *)dev;

    serial->ops->config((dev_serial_t *)dev, config);

    // 发送每个字节的位数
    serial->byte_bits = 1 + config->data_bits + config->stop_bits;
    if (config->parity != SerialParity_None)
        serial->byte_bits ++;
}

void serial_send(device_t * dev, void * p_buff, int size)
{
    if (size == 0)
        return;
    
    dev_serial_t * serial = (dev_serial_t *)dev;
    int ret = m_stream_push(&serial->stream_tx, p_buff, size);
    if (ret != Stream_OK) {
        serial_name = serial->super.name;
        ret_value = ret;
        ret = ret;
        
        M_ERROR(    "Line: %d, Name: %s, Ret: %d. Size: %d, EmptySize: %d, Head: %d, Tail: %d.",
                     __LINE__, serial->super.name, ret, size,
                     m_stream_empty_size(&serial->stream_rx),
                     serial->stream_rx.head, serial->stream_rx.tail);
    }
//    M_ASSERT_INFO(ret == Stream_OK, "Line: %d, Name: %s, Ret: %d.",
//                  __LINE__, serial->super.name, ret);
    
    // 如果等待time_wait_rx没有产生新的包，则认为串口空闲中断。
    if (serial->config.baud_rate > 19200)
        serial->time_wait_rx = 5;
    else
        serial->time_wait_rx = 10;
}

int serial_recv(device_t * dev, void * p_buff, int size)
{
    dev_serial_t * serial = (dev_serial_t *)dev;
    
    serial->ops->enter_critical(serial);
    int count = m_stream_pull_pop(&serial->stream_rx, p_buff, size);
    serial->ops->exit_critical(serial);
    
    if (count < 0) {
        serial_name = serial->super.name;
        ret_value = count;
        count = count;
    }
    M_ASSERT_INFO(count >= 0, "Line: %d, Name: %s, Count: %d.",
                  __LINE__, serial->super.name, count);
    
    return count;
}

void serial_set_evt_recv(device_t * dev, int evt_id)
{
    ((dev_serial_t *)dev)->evt_recv = evt_id;
}

int serial_get_evt_recv(device_t * dev)
{
    return ((dev_serial_t *)dev)->evt_recv;
}

// static function -------------------------------------------------------------
static uint8_t buffer[SERIAL_BUFF_SIZE];
static void serial_poll(device_t * dev, uint64_t time_system_ms)
{
    dev_serial_t * serial = (dev_serial_t *)dev;
    serial->ops->poll(serial, time_system_ms);

    // 如果超过读取期望时间
    if (time_system_ms > serial->time_expect_read) {
        serial->ops->enter_critical(serial);
        bool empty = m_stream_empty(&serial->stream_rx);
        serial->ops->exit_critical(serial);
        
        if (empty == false) {
            EVT_PUB(serial->evt_recv);
        }
        serial->time_expect_read = time_system_ms + 10;
    }
    
    // 如果超过了发送时间
    if (time_system_ms > serial->time_send) {
        serial->ops->enter_critical(serial);
        int count = m_stream_pull(&serial->stream_tx, buffer, 1000);
        serial->ops->exit_critical(serial);
        
        if (count == 0)
            return;
        if (count < 0) {
            serial_name = serial->super.name;
            ret_value = count;
            count = count;
        }
        M_ASSERT_INFO(count >= 0, "Line: %d, Name: %s, Count: %d.",
                      __LINE__, serial->super.name, count);
        
        dev_err_t err = serial->ops->send(serial, buffer, count);
        M_ASSERT_INFO(err == Dev_OK, "Line: %d, Name: %s, Error: %d.",
                      __LINE__, serial->super.name, err);
        
        serial->ops->enter_critical(serial);
        m_stream_pop(&serial->stream_tx, count);
        serial->ops->exit_critical(serial);
        
        serial->time_send = (serial->byte_bits * count * 1000 / serial->config.baud_rate) +
                            time_system_ms + 3;
    }
}
