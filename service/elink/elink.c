
/* include ------------------------------------------------------------------ */
#include "elink.h"
#include "eos.h"
#include <stdlib.h>

/* private variable --------------------------------------------------------- */
elink_block_t elink;

/* MCU write, Host read */
static uint8_t buff_tx[ELINK_BUFFER_TX_SIZE];

/* MCU read, Host write */
static uint8_t buff_rx[ELINK_BUFFER_RX_SIZE];

/* private function --------------------------------------------------------- */
static uint16_t _buff_tx_free_size(void);

/* public function ---------------------------------------------------------- */
void elink_init(void)
{
    elink.magic = 0xdeadbeef;
    // ^*+ecp->block@$#
    elink.label[0] = '^';
    elink.label[1] = '+';
    elink.label[2] = 'e';
    elink.label[3] = 'l';
    elink.label[4] = 'i';
    elink.label[5] = 'n';
    elink.label[6] = 'k';
    elink.label[7] = '.';
    elink.label[8] = 'b';
    elink.label[9] = 'l';
    elink.label[10] = 'o';
    elink.label[11] = 'c';
    elink.label[12] = 'k';
    elink.label[13] = '@';
    elink.label[14] = '$';
    elink.label[15] = 0;

    elink.state = ElinkState_Init;
    elink.capacity_tx = ELINK_BUFFER_TX_SIZE;
    elink.capacity_rx = ELINK_BUFFER_RX_SIZE;

    elink.head_tx = 0;
    elink.tail_tx = 0;
    elink.head_rx = 0;
    elink.tail_rx = 0;

    elink.buff_rx = buff_rx;
    elink.buff_tx = buff_tx;

    elink.tx_read_lock = 0;
    elink.rx_write_lock = 0;
}

uint16_t elink_write(void *data, uint16_t size)
{
    uint16_t ret = size;
    uint16_t size_free;
    uint32_t level = eos_hw_interrupt_disable();

    // Get the remaining size of the tx buffer
    size_free = _buff_tx_free_size();

    // Copy the data into buffer.
    elink.tx_read_lock = 1;
    for (uint16_t i = 0; i < size; i ++)
    {
        while (i >= size_free)
        {
            elink.tx_read_lock = 0;
            eos_hw_interrupt_enable(level);
            eos_task_delay_ms(1);
            level = eos_hw_interrupt_disable();
            elink.tx_read_lock = 1;
            size_free = _buff_tx_free_size();
        }
        elink.buff_tx[elink.head_tx] = ((uint8_t *)data)[i];
        elink.head_tx = (elink.head_tx + 1) % elink.capacity_tx;
    }
    elink.tx_read_lock = 0;

    eos_hw_interrupt_enable(level);
    return ret;
}

uint16_t elink_read(void *data, uint16_t size)
{
    uint16_t ret = size;
    uint32_t level = eos_hw_interrupt_disable();

    // Get the data size in the rx buffer
    uint16_t size_data = 0;
    if (elink.head_rx >= elink.tail_rx)
    {
        size_data = (elink.head_rx - elink.tail_rx);
    }
    else
    {
        size_data = elink.capacity_rx - (elink.tail_rx - elink.head_rx);
    }

    // If no data in buffer rx.
    if (size_data == 0)
    {
        ret = 0;
        goto exit;
    }

    // Copy the data from the buffer
    elink.rx_write_lock = 1;
    ret = size_data > size ? size : size_data;
    for (uint16_t i = 0; i < ret; i ++)
    {
        elink.buff_rx[elink.tail_rx] = ((uint8_t *)data)[i];
        elink.tail_rx = (elink.tail_rx + 1) % elink.capacity_rx;
    }
    elink.rx_write_lock = 0;

exit:
    eos_hw_interrupt_enable(level);
    return ret;
}

/* private function --------------------------------------------------------- */
static uint16_t _buff_tx_free_size(void)
{
    uint16_t size_free = 0;

    if (elink.head_tx >= elink.tail_tx)
    {
        size_free = elink.capacity_tx - (elink.head_tx - elink.tail_tx) - 1;
    }
    else
    {
        size_free = (elink.tail_tx - elink.head_tx) - 1;
    }

    return size_free;
}
