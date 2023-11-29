#include "isotp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dev_def.h"

static device_t *dev_can;
extern void ISOTP_delayMs(uint32_t msDelay);

#define ISOTP_log(...)

typedef enum { ISOTP_SINGLE = 0, ISOTP_FIRST = 1, ISOTP_CONS = 2, ISOTP_FLOW = 3 } ISOTP_control_type;

static void ISOTP_reset_msg(ISOTP_msgRx_type *IsotpMsg);
static void ISOTP_release_msg(ISOTP_msgRx_type *IsotpMsg);
static void ISOTP_malloc(ISOTP_msgRx_type *IsotpMsg);
static void ISOTP_flowControl(ISOTP_msgRx_type *IsotpMsg);
static void ISOTP_flowControl_reset(uint32_t Id);
static bool ISOTP_flowControl_wait(uint32_t Id);
static uint8_t ISOTP_flowControl_getTime(uint32_t Id);
static uint8_t ISOTP_flowControl_getBlock(uint32_t Id);
static void ISOTP_flowControl_set(uint32_t Id, uint8_t nBlock, uint8_t time);

static void ISOTP_parseFrame(dev_can_msg_t *CanMsg, ISOTP_msgRx_type *IsotpMsg);


void ISOTP_init()
{
	dev_can = device_find(DEV_CAN);
}

bool CAN_sendmsg(dev_can_msg_t *Msg)
{
    dev_can_send(dev_can, Msg);
}

static void ISOTP_reset_msg(ISOTP_msgRx_type *IsotpMsg)
{
    // original
    //	IsotpMsg->data_len = 0;
    //	IsotpMsg->byte_count = 0;
    //	IsotpMsg->sequence = 0;
    //	IsotpMsg->block_counter = 0;

    //	if( IsotpMsg->data )
    //		free( IsotpMsg->data );
    //	IsotpMsg->data = 0;
    //	IsotpMsg->malloc_len = 0;

    // modified by jay
    //	IsotpMsg->data_len = 0;
    IsotpMsg->byte_count    = 0;
    IsotpMsg->sequence      = 0;
    IsotpMsg->block_counter = 0;
}

static void ISOTP_release_msg(ISOTP_msgRx_type *IsotpMsg)
{
    if (IsotpMsg->frameCompled)
        IsotpMsg->frameCompled(IsotpMsg);

    ISOTP_reset_msg(IsotpMsg);
}

static void ISOTP_malloc(ISOTP_msgRx_type *IsotpMsg)
{
    if (IsotpMsg->malloc_len < IsotpMsg->data_len)
    {
        IsotpMsg->data = realloc(IsotpMsg->data, IsotpMsg->data_len);
        if (IsotpMsg->data)
            IsotpMsg->malloc_len = IsotpMsg->data_len;
    }
}

static void ISOTP_flowControl(ISOTP_msgRx_type *IsotpMsg)
{
    dev_can_msg_t msg;
    msg.id      = IsotpMsg->flowControl.send.id;
    msg.len     = 3;
    msg.data[0] = ISOTP_FLOW << 4;
    msg.data[1] = IsotpMsg->flowControl.send.nBlock;
    msg.data[2] = IsotpMsg->flowControl.send.delay;
    CAN_sendmsg(&msg);
}

static void ISOTP_flowControl_reset(uint32_t Id)
{
    for (uint8_t i = 0; isotpTable[i]; i++)
    {
        if (Id == isotpTable[i]->id)
        {
            isotpTable[i]->flowControl.received = false;
        }
    }
}

static bool ISOTP_flowControl_wait(uint32_t Id)
{
    for (uint8_t i = 0; isotpTable[i]; i++)
    {
        if (Id == isotpTable[i]->id)
        {
            return isotpTable[i]->flowControl.received;
        }
    }

    return false;
}

static uint8_t ISOTP_flowControl_getTime(uint32_t Id)
{
    for (uint8_t i = 0; isotpTable[i]; i++)
    {
        if (Id == isotpTable[i]->id)
        {
            return isotpTable[i]->flowControl.read.delay;
        }
    }

    return 0;
}

static uint8_t ISOTP_flowControl_getBlock(uint32_t Id)
{
    for (uint8_t i = 0; isotpTable[i]; i++)
    {
        if (Id == isotpTable[i]->id)
        {
            return isotpTable[i]->flowControl.read.nBlock;
        }
    }

    return 0;
}

static void ISOTP_flowControl_set(uint32_t Id, uint8_t nBlock, uint8_t delay)
{
    for (uint8_t i = 0; isotpTable[i]; i++)
    {
        if (Id == isotpTable[i]->id)
        {
            isotpTable[i]->flowControl.read.nBlock = nBlock;
            isotpTable[i]->flowControl.read.delay  = delay;
            isotpTable[i]->flowControl.received    = true;
        }
    }
}

static void ISOTP_parseFrame(dev_can_msg_t *CanMsg, ISOTP_msgRx_type *IsotpMsg)
{
    ISOTP_control_type pci_type = (ISOTP_control_type)((CanMsg->data[0] & 0xF0) >> 4);

    switch (pci_type)
    {
        case ISOTP_SINGLE: {
            ISOTP_reset_msg(IsotpMsg);
            uint16_t sf_dl = CanMsg->data[0] & 0xF;

            if (sf_dl > 7)
            {
                ISOTP_log("invalid single frame data lenght\n");
                return;
            }

            IsotpMsg->id       = CanMsg->id;
            IsotpMsg->data_len = sf_dl;
            ISOTP_malloc(IsotpMsg);
            if (IsotpMsg->data)
                memcpy(IsotpMsg->data, &CanMsg->data[1], sf_dl);

            ISOTP_release_msg(IsotpMsg);
        }
        break;

        case ISOTP_FIRST: {
            ISOTP_reset_msg(IsotpMsg);
            IsotpMsg->id       = CanMsg->id;
            uint16_t ff_dl     = ((CanMsg->data[0] & 0xF) << 8) + CanMsg->data[1];
            IsotpMsg->data_len = ff_dl;

            uint16_t len = IsotpMsg->data_len < 6 ? IsotpMsg->data_len : 6;
            ISOTP_malloc(IsotpMsg);
            if (IsotpMsg->data)
                memcpy(&IsotpMsg->data[IsotpMsg->byte_count], &CanMsg->data[2], len);

            IsotpMsg->byte_count += len;

            IsotpMsg->sequence += 1;

            // send flow control frame
            // if( IsotpMsg->flowControl.enable == true )
            ISOTP_flowControl(IsotpMsg);

            IsotpMsg->block_counter = IsotpMsg->flowControl.send.nBlock;
        }
        break;

        case ISOTP_CONS: {
            if (IsotpMsg->data_len == 0)
            {
                ISOTP_log("missing first frame\n");
                return;
            }

            uint8_t frame_sequence = CanMsg->data[0] & 0xF;

            if (frame_sequence != IsotpMsg->sequence)
            {
                IsotpMsg->data_len = IsotpMsg->byte_count = 0;
                ISOTP_log("invalid frame sequence\n");
                return;
            }

            uint16_t last_bytes = IsotpMsg->data_len - IsotpMsg->byte_count;

            uint16_t len = (last_bytes < 7) ? last_bytes : 7;
            if (IsotpMsg->data)
                memcpy(&IsotpMsg->data[IsotpMsg->byte_count], &CanMsg->data[1], len);

            IsotpMsg->byte_count += len;

            IsotpMsg->sequence += 1;
            if (IsotpMsg->sequence > 0xF)
                IsotpMsg->sequence = 0;

            if (IsotpMsg->byte_count == IsotpMsg->data_len)
            {
                ISOTP_release_msg(IsotpMsg);
            }
            else if (IsotpMsg->byte_count > IsotpMsg->data_len)
            {
                IsotpMsg->data_len = IsotpMsg->byte_count = 0;
                ISOTP_log("invalid message lenght\n");
                return;
            }

            if (IsotpMsg->block_counter > 0)
            {
                IsotpMsg->block_counter -= 1;
                if (IsotpMsg->block_counter == 0)
                {
                    // send flow control frame
                    ISOTP_flowControl(IsotpMsg);

                    IsotpMsg->block_counter = IsotpMsg->flowControl.send.nBlock;
                }
            }
        }
        break;

        case ISOTP_FLOW:
            ISOTP_flowControl_set(CanMsg->id, CanMsg->data[1], CanMsg->data[2]);
            break;

        default:
            ISOTP_log("unknow PCItype\n");
            break;
    }
}

bool ISOTP_parseMsg(dev_can_msg_t *const CanMsg)
{
    for (uint8_t i = 0; isotpTable[i]; i++)
    {
        if (CanMsg->id == isotpTable[i]->id)
        {
            ISOTP_parseFrame(CanMsg, isotpTable[i]);
            return true;
        }
    }

    return false;
}

ISOTP_ret_id ISOTP_sendFrame(ISOTP_msgTx_type *const IsotpMsg)
{
    ISOTP_ret_id ret;

    if (IsotpMsg->ISOTP_state != ISOTP_START_SEND)
    {
        if (osKernelGetTickCount() > IsotpMsg->timeout_frame)
        {
            if (IsotpMsg->ISOTP_state == ISOTP_WAIT_FLOW)
                ret = ISOTP_FLOW_TIMEOUT;
            else
                ret = ISOTP_TIMEOUT;

            IsotpMsg->ISOTP_state = ISOTP_START_SEND;
            return ret;
        }
    }

    switch (IsotpMsg->ISOTP_state)
    {
        case ISOTP_START_SEND:
            // IsotpMsg->timeout_frame = osKernelGetTickCount() + 100 + IsotpMsg->data_len;
            IsotpMsg->timeout_frame = osKernelGetTickCount() + 100;
            if (IsotpMsg->data_len > 4096)
            {
                ISOTP_log("data must be <= 4095 byte\n");
                ret = ISOTP_ERROR;
                break;
            }

            IsotpMsg->msg.id = IsotpMsg->id;
            ISOTP_flowControl_reset(IsotpMsg->flowControl.id);

            if (IsotpMsg->data_len < 8)
            {
                IsotpMsg->msg.len     = IsotpMsg->data_len + 1;
                IsotpMsg->msg.data[0] = (ISOTP_SINGLE << 4) | IsotpMsg->data_len;
                memcpy(&IsotpMsg->msg.data[1], IsotpMsg->data, IsotpMsg->data_len);

                if (CAN_sendmsg(&IsotpMsg->msg))
                {
                    IsotpMsg->ISOTP_state = ISOTP_START_SEND;
                    ret                   = ISOTP_OK;
                    break;
                }
                else
                {
                    IsotpMsg->ISOTP_state = ISOTP_RETRY_SEND;
                    ret                   = ISOTP_RETRY;
                    break;
                }
            }
            else
            {
                IsotpMsg->msg.len     = 8;
                IsotpMsg->msg.data[0] = (ISOTP_FIRST << 4) | IsotpMsg->data_len >> 8;
                IsotpMsg->msg.data[1] = IsotpMsg->data_len;
                memcpy(&IsotpMsg->msg.data[2], IsotpMsg->data, 6);
                ISOTP_flowControl_reset(IsotpMsg->flowControl.id);

                IsotpMsg->byte_count = 6;
                IsotpMsg->sequence   = 1;
                if (IsotpMsg->flowControl.enable)
                    IsotpMsg->block_counter = 1;
                else
                    IsotpMsg->block_counter = 0;  // disable FlowControll

                if (CAN_sendmsg(&IsotpMsg->msg))
                {
                    IsotpMsg->ISOTP_state = ISOTP_CONSEGUITEVE_SEND;
                    ret                   = ISOTP_CONTINUE;
                    break;
                }
                else
                {
                    IsotpMsg->ISOTP_state = ISOTP_RETRY_SEND;
                    ret                   = ISOTP_RETRY;
                    break;
                }
            }
            // break;	 comment to remove warning

        case ISOTP_RETRY_SEND:
            if (CAN_sendmsg(&IsotpMsg->msg))
            {
                if (IsotpMsg->data_len < 8)
                {
                    IsotpMsg->ISOTP_state = ISOTP_START_SEND;
                    ret                   = ISOTP_OK;
                    break;
                }
                else
                {
                    if (IsotpMsg->byte_count < IsotpMsg->data_len)
                    {
                        IsotpMsg->ISOTP_state = ISOTP_CONSEGUITEVE_SEND;
                        ret                   = ISOTP_CONTINUE;
                        break;
                    }
                    else
                    {
                        IsotpMsg->ISOTP_state = ISOTP_START_SEND;
                        ret                   = ISOTP_OK;
                        break;
                    }
                }
            }
            else
            {
                IsotpMsg->ISOTP_state = ISOTP_RETRY_SEND;
                ret                   = ISOTP_RETRY;
                break;
            }

        case ISOTP_CONSEGUITEVE_SEND: {
            if (IsotpMsg->byte_count < IsotpMsg->data_len)
            {
                if (IsotpMsg->flowControl.enable)
                {
                    IsotpMsg->block_counter -= 1;
                    if (IsotpMsg->block_counter == 0)
                    {
                        IsotpMsg->ISOTP_state = ISOTP_WAIT_FLOW;
                        ret                   = ISOTP_CONTINUE;
                        break;
                    }
                }
            }
            else
            {
                IsotpMsg->ISOTP_state = ISOTP_START_SEND;
                ret                   = ISOTP_OK;
                break;
            }

            uint8_t timeDelay;
            if (!IsotpMsg->flowControl.enable)
            {
                IsotpMsg->ISOTP_state = ISOTP_SEND;
                ret                   = ISOTP_CONTINUE;
                break;
            }

            timeDelay = ISOTP_flowControl_getTime(IsotpMsg->flowControl.id);
            if (timeDelay && timeDelay < 0x80)
            {
                IsotpMsg->delay_msg = osKernelGetTickCount() + timeDelay;
            }
            else
            {
                IsotpMsg->delay_msg = 0;  // should be 100-900uS
            }
            IsotpMsg->ISOTP_state = ISOTP_WAIT_SEND;
            // break; fall down
        }

        case ISOTP_WAIT_SEND:
            if (osKernelGetTickCount() < IsotpMsg->delay_msg)
            {
                ret = ISOTP_CONTINUE;
                break;
            }
            IsotpMsg->ISOTP_state = ISOTP_SEND;
            // break; fall down

        case ISOTP_SEND: {
            uint16_t last_bytes = IsotpMsg->data_len - IsotpMsg->byte_count;
            uint16_t len        = (last_bytes < 7) ? last_bytes : 7;

            IsotpMsg->msg.len     = len + 1;
            IsotpMsg->msg.data[0] = (ISOTP_CONS << 4) | IsotpMsg->sequence;
            memcpy(&IsotpMsg->msg.data[1], &IsotpMsg->data[IsotpMsg->byte_count], len);

            IsotpMsg->sequence = (++IsotpMsg->sequence) & 0x0F;
            IsotpMsg->byte_count += len;

            if (CAN_sendmsg(&IsotpMsg->msg))
            {
                IsotpMsg->ISOTP_state = ISOTP_CONSEGUITEVE_SEND;
                ret                   = ISOTP_CONTINUE;
                break;
            }
            else
            {
                IsotpMsg->ISOTP_state = ISOTP_RETRY_SEND;
                ret                   = ISOTP_RETRY;
                break;
            }
        }
            // break; warning remove;

        case ISOTP_WAIT_FLOW:
            if (ISOTP_flowControl_wait(IsotpMsg->flowControl.id))
            {
                IsotpMsg->block_counter = ISOTP_flowControl_getBlock(IsotpMsg->flowControl.id);
                ISOTP_flowControl_reset(IsotpMsg->flowControl.id);

                uint8_t timeDelay = ISOTP_flowControl_getTime(IsotpMsg->flowControl.id);

                if (timeDelay && (timeDelay < 0x80))
                    IsotpMsg->timeout_frame = osKernelGetTickCount() + (IsotpMsg->block_counter * (timeDelay + 1)) + 50;
                else
                    IsotpMsg->timeout_frame = osKernelGetTickCount() + (IsotpMsg->block_counter) + 2000;

                if (timeDelay && timeDelay < 0x80)
                {
                    IsotpMsg->delay_msg   = osKernelGetTickCount() + timeDelay;
                    IsotpMsg->ISOTP_state = ISOTP_WAIT_SEND;
                }
                else
                {
                    IsotpMsg->delay_msg   = 0;  // should be 100-900uS
                    IsotpMsg->ISOTP_state = ISOTP_SEND;
                }

                ret = ISOTP_CONTINUE;
                break;
            }
            else
            {
                IsotpMsg->ISOTP_state = ISOTP_WAIT_FLOW;
                ret                   = ISOTP_CONTINUE;
                break;
            }
            // break; warning remove;
    }

    return ret;
}
