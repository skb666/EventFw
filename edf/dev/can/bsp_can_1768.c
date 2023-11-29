
// include ---------------------------------------------------------------------
#include "system_LPC17xx.h"
#include "LPC17xx.h"
#include "lpc17xx_can.h"
#include "stdint.h"
#include "dev_can.h"
#include "mdebug/m_assert.h"
#include "dev_def.h"

M_MODULE_NAME("Bsp-Can")

// 待完善 ----------------------------------------------------------------------
// TODO 回环模式，尚未完成，近期不再理会此功能。2020-09-08。

// 设备指针 --------------------------------------------------------------------
dev_can_t dev_can;

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
    dev_can.ops = &can_ops;

    LPC_SC->PCONP       |=  (1 << 13);          // Enable power to CAN1 block
    LPC_PINCON->PINSEL1 |=  (1 << 10);          // Pin P0.21 used as RD1 (CAN1)
    LPC_PINCON->PINSEL1 |=  (1 << 11);
    LPC_PINCON->PINSEL1 |=  (1 << 12);          // Pin P0.22 used as TD1 (CAN1)
    LPC_PINCON->PINSEL1 |=  (1 << 13);

    dev_can_config_t config_default = {
        DevCanBaudRate_500K, DevCanMode_Normal
    };
    drv_can_config(&dev_can, &config_default);

    dev_err_t ret = dev_can_reg(&dev_can, DEV_CAN, (void *)0);
    M_ASSERT(ret == Dev_OK);
}

static dev_err_t drv_can_config(dev_can_t * can, dev_can_config_t * config)
{
    (void)can;

    CAN_Init(LPC_CAN1, config->baud_rate);

    CAN_MODE_Type can_mode = CAN_OPERATING_MODE;
    CAN_ModeConfig(LPC_CAN1, can_mode, ENABLE);
    //CAN_LoadExplicitEntry(LPC_CAN1, 0, EXT_ID_FORMAT);
    CAN_LoadGroupEntry(LPC_CAN1, 0x000, 0xfff, STD_ID_FORMAT);
    CAN_IRQCmd(LPC_CAN1, CANINT_RIE, ENABLE);
    
    NVIC_SetPriority(CAN_IRQn, 0 | 0);
    NVIC_EnableIRQ(CAN_IRQn);
    
    return Dev_OK;
}

static dev_err_t drv_can_enable(dev_can_t * can, bool enable)
{
    (void)can;

    if (enable == true)
        LPC_CAN1->MOD = 0;
    else
        LPC_CAN1->MOD = 1;
    
    return Dev_OK;
}

CAN_MSG_Type can_msg;
static int drv_can_send(dev_can_t * can, const dev_can_msg_t * msg)
{
    (void)can;
    
    can_msg.id = msg->id;
    can_msg.len = msg->len;
    can_msg.format = msg->ide == 0 ? STD_ID_FORMAT : EXT_ID_FORMAT;
    can_msg.type = msg->rtr == 0 ? DATA_FRAME : REMOTE_FRAME;
    for (int i = 0; i < msg->len; i ++)  {
        if (i <= 3)
            can_msg.dataA[i] = msg->data[i];
        else
           can_msg.dataB[i - 4] = msg->data[i];
    }

    CAN_SendMsg(LPC_CAN1, &can_msg);

    return 0;
}

static void drv_can_recv(dev_can_t * can, dev_can_msg_t * msg)
{
    (void)can;
    
    CAN_MSG_Type can_msg;
    CAN_ReceiveMsg(LPC_CAN1, &can_msg);

    msg->ide = can_msg.format == STD_ID_FORMAT ? 0 : 1;     // 扩展帧
    msg->rtr = can_msg.type == DATA_FRAME ? 0 : 1;          // 远程帧
    msg->len = can_msg.len;                                 // 数据个数
    msg->id = can_msg.id;                                   // 帧ID

    for (int i = 0; i < 4; i ++)
        msg->data[i] = can_msg.dataA[i];
    for (int i = 0; i < 4; i ++)
        msg->data[i + 4] = can_msg.dataB[i];
}

static bool drv_can_send_busy(dev_can_t * can)
{
    (void)can;
    
    if (0 == (LPC_CAN1->GSR & (1 << 2)))
        return true;
    else
        return false;
}

void CAN_IRQHandler(void)
{
    if (1 == (CAN_IntGetStatus(LPC_CAN1) & CAN_ICR_RI)) {
        dev_can_msg_t msg;
        drv_can_recv(&dev_can, &msg);
        dev_can_isr_recv(&dev_can, &msg);
    }
}
