
/* include ------------------------------------------------------------------ */
#include "elink.h"
#include "eos.h"
#include <stdlib.h>

/* private define ----------------------------------------------------------- */
#define ELINK_WAIT_FOREVER                  (0xffffffff)
#define ELINK_REFRESH_PRIORITY              (1)

/* private variable --------------------------------------------------------- */
elink_block_t elink;

/* MCU write, Host read */
static uint8_t buff_tx[ELINK_BUFFER_RX_SIZE];

static eos_sem_t sem_tx;
static eos_sem_t sem_refresh;
static eos_task_t task_refresh;
static uint32_t stack_refresh[128];

/* MCU read, Host write */
static uint8_t buff_rx[ELINK_BUFFER_TX_SIZE];

/* private function --------------------------------------------------------- */
static void task_entry_buffer_refresh(void *parameters);

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

    eos_sem_init(&sem_tx, (ELINK_BUFFER_TX_SIZE - 1));
    eos_sem_init(&sem_refresh, 0);

    eos_task_init(&task_refresh, "elink_refresh",
                    task_entry_buffer_refresh,
                    NULL, stack_refresh, sizeof(stack_refresh),
                    ELINK_REFRESH_PRIORITY);
}

uint16_t elink_write(void *data, uint16_t size)
{
    uint16_t ret = size;
    uint32_t level = eos_hw_interrupt_disable();

    // Get the remaining size of the tx buffer
    uint32_t size_free = 0;
    if (elink.head_tx >= elink.tail_tx)
    {
        size_free = elink.capacity_tx - (elink.head_tx - elink.tail_tx) - 1;
    }
    else
    {
        size_free = (elink.tail_tx - elink.head_tx) - 1;
    }

    // Copy the data into buffer.
    elink.tx_read_lock = 1;
    for (uint16_t i = 0; i < size; i ++)
    {
        eos_sem_take(&sem_tx, EOS_WAIT_FOREVER);
        if (i >= size_free)
        {
            eos_sem_release(&sem_refresh);
            eos_hw_interrupt_enable(level);
            level = eos_hw_interrupt_disable();
            break;
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
static void task_entry_buffer_refresh(void *parameters)
{
    while (1)
    {
        eos_sem_take(&sem_refresh, EOS_WAIT_FOREVER);

        while (1)
        {
            // Get the remaining size of the tx buffer
            uint32_t size_free = 0;
            if (elink.head_tx >= elink.tail_tx)
            {
                size_free = elink.capacity_tx - (elink.head_tx - elink.tail_tx) - 1;
            }
            else
            {
                size_free = (elink.tail_tx - elink.head_tx) - 1;
            }

            // If size_free is not 0.
            if (size_free == 0)
            {
                eos_task_delay_ms(1);
            }
            else
            {
                eos_sem_reset(&sem_tx, size_free);
                break;
            }
        }
    }
}
