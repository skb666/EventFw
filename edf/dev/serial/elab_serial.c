/*
 * eLab Project
 * Copyright (c) 2023, EventOS Team, <event-os@outlook.com>
 */

/* includes ----------------------------------------------------------------- */
#include "elab_serial.h"
#include "elab_common.h"
#include "elab_assert.h"
#include "elib_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

ELAB_TAG("Edf_Serial");

#define WAIT_FOREVER    (0xFFFFFFFFU)

/* public function prototypes ----------------------------------------------- */
void elab_device_unregister(elab_device_t *me);

/* private function prototypes ---------------------------------------------- */
static elab_err_t _device_enable(elab_device_t *me, bool status);
static int32_t _device_read(elab_device_t *me,
                                uint32_t pos, void *buffer, uint32_t size);
static int32_t _device_write(elab_device_t *me,
                                uint32_t pos, const void *buffer, uint32_t size);

#if (ELAB_DEV_PALTFORM == ELAB_PALTFORM_POLL)
static void _device_poll(elab_device_t *me);
#endif

/* private variables -------------------------------------------------------- */
static const elab_dev_ops_t _device_ops =
{
    .enable = _device_enable,
    .read = _device_read,
    .write = _device_write,
#if (ELAB_DEV_PALTFORM == ELAB_PALTFORM_POLL)
    .poll = _device_poll,
#endif
};

/* public functions --------------------------------------------------------- */
/**
  * @brief  Register elab serial device to edf framework.
  * @param  serial      elab serial device handle.
  * @param  name        Serial name.
  * @param  ops         Serial device ops.
  * @param  attr        Serial device attribute.
  * @param  user_data   The pointer of private data.
  * @retval None.
  */
void elab_serial_register(elab_serial_t *serial, const char *name,
                          elab_uart_ops_t *ops,
                          elab_serial_attr_t *attr,
                          void *user_data)
{
    elab_assert(serial != NULL);
    elab_assert(name != NULL);
    elab_assert(ops != NULL);

    memset(serial, 0, sizeof(elab_serial_t));

    /* The serial class data */
    serial->ops = ops;
    if (attr == NULL)
    {
        serial->attr = (elab_serial_attr_t)ELAB_SERIAL_ATTR_DEFAULT;
    }
    else
    {
        memcpy(&serial->attr, attr, sizeof(elab_serial_attr_t));
    }

    /* The serial device initialization. */
    serial->ops = ops;
    if (serial->ops->config != NULL)
    {
        elab_serial_attr_t _attr = serial->attr;
        serial->ops->config(serial, (elab_serial_config_t *)&_attr);
    }
    /* If RS485 mode, set to rx mode. */
    if (serial->attr.mode == ELAB_SERIAL_MODE_HALF_DUPLEX)
    {
        serial->ops->set_tx(serial, false);
    }
    
    /* Apply buffer memory */
    serial->queue_rx = (elib_queue_t *) elab_malloc(sizeof(elib_queue_t) + serial->attr.bufsz);
    elab_assert(serial->queue_rx != NULL);
    elib_queue_init(serial->queue_rx, (serial->queue_rx + 1), serial->attr.bufsz);

    serial->queue_tx = (elib_queue_t *) elab_malloc(sizeof(elib_queue_t) + serial->attr.bufsz);
    elab_assert(serial->queue_tx != NULL);
    elib_queue_init(serial->queue_tx, (serial->queue_tx + 1), serial->attr.bufsz);

    /* The super class data */
    elab_device_t *device = &(serial->super);
    device->ops = &_device_ops;
    device->user_data = user_data;

    /* register a character device */
    elab_device_attr_t _dev_attr =
    {
        .name = name,
        .sole = true,
        .type = ELAB_DEVICE_UART,
    };
    if (serial->attr.mode == ELAB_SERIAL_MODE_HALF_DUPLEX)
    {
        _dev_attr.sole = false;
    }
    elab_device_register(device, &_dev_attr);

}

/**
  * @brief  Register serial device to elab device
  * @param  serial      The pointer of elab serial dev
  * @retval None.
  */
void elab_serial_unregister(elab_serial_t *serial)
{
    elab_assert(serial != NULL);
    elab_assert(!elab_device_is_enabled(ELAB_DEVICE_CAST(serial)));

    elab_device_lock(ELAB_DEVICE_CAST(serial));
    elab_free(serial->queue_rx);
    elab_free(serial->queue_tx);
    elab_device_unlock(ELAB_DEVICE_CAST(serial));

    elab_device_unregister(ELAB_DEVICE_CAST(serial));
}

/**
  * @brief  The serial device tx ISR function.
  * @param  serial      elab serial device handle
  * @retval None.
  */
void elab_serial_tx_end(elab_serial_t *serial)
{
    elab_assert(serial != NULL);

//    elab_assert(ret_os == ELAB_OK);
}

/**
  * @brief  The serial device rx ISR function.
  * @param  serial      elab serial device handle.
  * @param  buffer      The buffer memory.
  * @param  size        The serial memory size.
  * @retval None.
  */
void elab_serial_isr_rx(elab_serial_t *serial, void *buffer, uint32_t size)
{
    elab_assert(serial != NULL);
    elab_assert(buffer != NULL);
    elab_assert(size != 0);

    uint8_t *buff = (uint8_t *)buffer;
    if (!elab_device_is_test_mode(&serial->super))
    {
        if (elab_device_is_enabled(&serial->super))
        {
            elib_queue_pull_pop(serial->queue_rx, buff, size);
        }
    }
}

/**
  * @brief  elab serial device write function.
  * @param  me      The elab device handle.
  * @param  buff    The pointer of buffer
  * @param  size    Expected write length
  * @retval Actual write length or error ID.
  */
int32_t elab_serial_write(elab_device_t * const me, void *buff, uint32_t size)
{
    elab_assert(me != NULL);
    elab_assert(buff != NULL);
    elab_assert(size != 0);
    elab_assert(elab_device_is_enabled(me));

    int32_t ret = ELAB_OK;
    elab_serial_t *serial = (elab_serial_t *)me;
    elab_assert(serial->ops != NULL);
    elab_assert(serial->ops->send != NULL);

    if (serial->attr.mode == ELAB_SERIAL_MODE_HALF_DUPLEX)
    {
        elab_assert(serial->ops->set_tx != NULL);
    }

    /* If not in testing mode. */
    if (!elab_device_is_test_mode(&serial->super))
    {
        /* If RS485 mode, set to tx mode. */
        if (serial->attr.mode == ELAB_SERIAL_MODE_HALF_DUPLEX)
        {
            serial->ops->set_tx(serial, true);
        }

        /* Send data by the low level serial write function in non-block mode. */
        ret = serial->ops->send(serial, buff, size);
        if (ret < 0)
        {
            goto exit;
        }

exit:
        /* If RS485 mode, set to rx mode. */
        if (serial->attr.mode == ELAB_SERIAL_MODE_HALF_DUPLEX)
        {
            serial->ops->set_tx(serial, false);
        }
    }

    return ret;
}

/**
  * @brief  elab device read function
  * @param  me      The elab device handle.
  * @param  buffer  The pointer of buffer
  * @param  size    Expected read length
  * @retval Actual write length or error ID.
  */
int32_t elab_serial_read(elab_device_t * const me, void *buff,
                            uint32_t size, uint32_t timeout)
{
    elab_assert(me != NULL);
    elab_assert(buff != NULL);
    elab_assert(size != 0);
    elab_assert(elab_device_is_enabled(me));

    int32_t ret = ELAB_OK;
    int32_t recv_num = 0;
    int32_t recv_sum = 0;
    elab_serial_t *serial = (elab_serial_t *)me;
    elab_assert(serial->ops != NULL);
    elab_assert(serial->ops->recv != NULL);

    /* If not in testing mode. */
    if (!elab_device_is_test_mode(&serial->super))
    {
        uint32_t time_start = eos_time();
        uint32_t time = timeout;
        do
        {
            if (timeout != WAIT_FOREVER && timeout != 0)
            {
                if ((eos_time() - time_start) < timeout)
                {
                    time = time_start + timeout - eos_time();
                }
                else
                {
                    if (ret == ELAB_OK)
                    {
                        ret = ELAB_ERR_TIMEOUT;
                    }
                    break;
                }
            }

            recv_num = serial->ops->recv(serial, buff, size);
            if (recv_num > 0)
            {
                recv_sum += recv_num;
                if (recv_sum >= size)
                {
                    break;
                }
                else
                {
                    continue;
                }
            }
//            elab_assert(false);
        } while(recv_sum < size);
    }

    return recv_sum;
}

/**
  * @brief  Set the attribute of the elab serial device
  * @param  serial      elab serial device handle
  * @param  name        Serial name
  * @param  ops         Serial device ops
  * @param  user_data   The pointer of private data
  * @retval See elab_err_t
  */
void elab_serial_set_attr(elab_device_t *me, elab_serial_attr_t *attr)
{
    elab_assert(me != NULL);
    elab_assert(attr != NULL);
    elab_assert(elab_device_is_enabled(me));

    elab_serial_t *serial = ELAB_SERIAL_CAST(me);

    elab_device_lock(serial);
    elab_assert(attr->mode == serial->attr.mode);
    elab_assert(attr->bufsz == serial->attr.bufsz);
    /* Set the config data of serial device. */
    if (memcmp(&serial->attr, attr, sizeof(elab_serial_attr_t)) != 0)
    {
        elab_err_t ret = serial->ops->config(serial, (elab_serial_config_t *)attr);
        if (ret == ELAB_OK)
        {
            memcpy(&serial->attr, attr, sizeof(elab_serial_attr_t));
        }
    }
    elab_device_unlock(serial);
}

/**
  * @brief  Register elab serial device to serial device
  * @param  serial      elab serial device handle
  * @param  name        Serial name
  * @param  ops         Serial device ops
  * @param  user_data   The pointer of private data
  * @retval See elab_err_t
  */
elab_serial_attr_t elab_serial_get_attr(elab_device_t *me)
{
    elab_assert(me != NULL);

    elab_serial_attr_t attr;
    elab_serial_t *serial = ELAB_SERIAL_CAST(me);

    elab_device_lock(serial);
    memcpy(&attr, &serial->attr, sizeof(elab_serial_attr_t));
    elab_device_unlock(serial);

    return attr;
}

/**
  * @brief  Set the serial device baudrate.
  * @param  dev         The device handle.
  * @param  baudrate    The serial device baudrate.
  * @retval See elab_err_t.
  */
void elab_serial_set_baudrate(elab_device_t *me, uint32_t baudrate)
{
    elab_assert(me != NULL);
    elab_assert(baudrate == 4800 || baudrate == 9600 ||
                    baudrate == 19200 || baudrate == 38400 ||
                    baudrate == 57600 || baudrate == 115200 ||
                    baudrate == 230400 || baudrate == 460800 ||
                    baudrate == 921600 || baudrate >= 1000000);

    if (!elab_device_is_enabled(me))
    {
        ELAB_LOG_E("The serial device %s is NOT open yet.", me->attr.name);
        goto exit;
    }

    elab_serial_attr_t attr = elab_serial_get_attr(me);
    if (attr.baud_rate != baudrate)
    {
        attr.baud_rate = baudrate;
        elab_serial_set_attr(me, &attr);
    }

exit:
    return;
}

/**
  * @brief  elab serial device xfer function, just for half duplex mode.
  * @param  serial      elab serial device handle
  * @param  name        Serial name
  * @param  ops         Serial device ops
  * @param  user_data   The pointer of private data
  * @retval See elab_err_t
  */
int32_t elab_serial_xfer(elab_device_t *me,
                            void *buff_tx, uint32_t size_tx,
                            void *buff_rx, uint32_t size_rx,
                            uint32_t timeout)
{
    elab_assert(me != NULL);
    elab_assert(buff_tx != NULL);
    elab_assert(buff_tx != NULL);
    elab_assert(size_tx != 0);
    elab_assert(size_rx != 0);
    elab_assert(ELAB_SERIAL_CAST(me)->ops != NULL);
    elab_assert(ELAB_SERIAL_CAST(me)->ops->send != NULL);
    elab_assert(ELAB_SERIAL_CAST(me)->ops->recv != NULL);

    elab_serial_t *serial = ELAB_SERIAL_CAST(me);
    elab_assert(serial->attr.mode == ELAB_SERIAL_MODE_HALF_DUPLEX);

    uint32_t time_start = eos_time();
    uint32_t timeout_read = timeout;
    int32_t ret = elab_serial_write(me, buff_tx, size_tx);
    if (ret > 0)
    {
        if (timeout != WAIT_FOREVER && timeout != 0)
        {
            if (eos_time() >= (time_start + timeout))
            {
                ret = ELAB_ERR_TIMEOUT;
                goto exit;
            }

            timeout_read = eos_time() - time_start;
        }

        assert_id(ret == size_tx, ret);
        ret = elab_serial_read(me, buff_rx, size_rx, timeout_read);
    }

exit:
    return ret;
}

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  elab device enable function
  * @param  me  The elab device handle.
  * @retval See elab_err_t
  */
static elab_err_t _device_enable(elab_device_t *me, bool status)
{
    elab_assert(me != NULL);

    elab_serial_t *serial = (elab_serial_t *)me;
    elab_assert(serial->ops != NULL);
    elab_assert(serial->ops->enable != NULL);

    return serial->ops->enable(serial, status);
}

/**
  * @brief  elab device read function
  * @param  me      The elab device handle.
  * @param  pos     Position
  * @param  buffer  The pointer of buffer
  * @param  size    Expected read length
  * @retval Auctual read length
  */
static int32_t _device_read(elab_device_t *me,
                            uint32_t pos, void *buffer, uint32_t size)
{
    (void)pos;

    return elab_serial_read(me, buffer, size, WAIT_FOREVER);
}

/**
  * @brief  elab device write function.
  * @param  me      The elab device handle.
  * @param  pos     Position
  * @param  buffer  The pointer of buffer
  * @param  size    Expected write length
  * @retval Actual write length
  */
static int32_t _device_write(elab_device_t *me,
                                uint32_t pos, const void *buffer, uint32_t size)
{
    (void)pos;

    return elab_serial_write(me, (void *)buffer, size);
}

#ifdef __cplusplus
}
#endif

/* ----------------------------- end of file -------------------------------- */
