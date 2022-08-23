#ifndef ELINK_H
#define ELINK_H

/* include ------------------------------------------------------------------ */
#include <stdint.h>
#include <stdbool.h>

/* define-------------------------------------------------------------------- */
#define ELINK_BUFFER_TX_SIZE                (1024)
#define ELINK_BUFFER_RX_SIZE                (16)

enum
{
    ElinkState_Init = 0,
    ElinkState_Running,
    ElinkState_Stop,
    ElinkState_Stopped,
};

/* data structure ----------------------------------------------------------- */
typedef struct elink_block
{
    /* MCU write, Host read */
    uint32_t magic;
    uint8_t label[16];  // ^+elink.block@$
    uint32_t state;
    uint16_t capacity_tx;
    uint16_t capacity_rx;
    uint8_t *buff_rx;
    uint8_t *buff_tx;
    uint16_t head_tx;
    uint16_t tail_rx;
    uint8_t tx_read_lock;
    uint8_t rx_write_lock;

    /* MCU read, Host write */
    uint8_t rx_read_lock;
    uint8_t tx_write_lock;
    uint16_t tail_tx;
    uint16_t head_rx;
} elink_block_t;

/* function ----------------------------------------------------------------- */
void elink_init(void);
uint16_t elink_write(void *data, uint16_t size);
uint16_t elink_read(void *data, uint16_t size);

/* port --------------------------------------------------------------------- */
void elink_port_isr_enable(bool enable);

#endif
