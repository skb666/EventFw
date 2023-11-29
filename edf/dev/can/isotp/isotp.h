#ifndef __ISOTP_H__
#define __ISOTP_H__

#include <stdbool.h>
#include <stdint.h>

#include "can/dev_can.h"

#define FC_BLOCK_COUNTER 8
#define FC_DELAY_BLOCK_MS 0

typedef struct ISOTP_msgTx_type ISOTP_msgTx_type;
typedef struct ISOTP_msgRx_type ISOTP_msgRx_type;
typedef void (*isotpFrameCb_type)(ISOTP_msgRx_type *IsotpMsg);

typedef enum {
    ISOTP_START_SEND,
    ISOTP_RETRY_SEND,
    ISOTP_CONSEGUITEVE_SEND,
    ISOTP_WAIT_SEND,
    ISOTP_SEND,
    ISOTP_WAIT_FLOW
} ISOTP_state_id;

struct ISOTP_msgTx_type {
    uint32_t id;
    uint16_t data_len;
    uint16_t byte_count;
    uint8_t sequence;
    uint8_t block_counter;
    uint8_t *data;

    ISOTP_state_id ISOTP_state;
    uint32_t timeout_frame;
    uint32_t delay_msg;

    dev_can_msg_t msg;

    struct {
        bool enable;  // waiting flow control, NOT ISOtp standard
        uint32_t id;  // CAN ID for Flow control
    } flowControl;
};

struct ISOTP_msgRx_type {
    uint32_t id;
    uint16_t data_len;
    uint16_t byte_count;
    uint8_t sequence;
    uint8_t block_counter;
    uint8_t *data;
    uint16_t malloc_len;

    // ISOTP_state_id ISOTP_state;

    struct {
        bool received;
        struct {
            uint8_t nBlock;  // nBlock before next flow control
            uint8_t delay;   // delay between CAN msg
        } read;
        struct {
            uint32_t id;     // CAN ID for Flow control
            uint8_t nBlock;  // nBlock before next flow control
            uint8_t delay;   // delay between CAN msg
        } send;
    } flowControl;

    isotpFrameCb_type frameCompled;
};

typedef enum { ISOTP_CONTINUE, ISOTP_RETRY, ISOTP_OK, ISOTP_ERROR, ISOTP_TIMEOUT, ISOTP_FLOW_TIMEOUT } ISOTP_ret_id;

extern ISOTP_msgRx_type *const isotpTable[];

void ISOTP_init();

bool ISOTP_parseMsg(dev_can_msg_t *const CanMsg);

ISOTP_ret_id ISOTP_sendFrame(ISOTP_msgTx_type *const IsotpMsg);
#endif  // ISOTP_H
