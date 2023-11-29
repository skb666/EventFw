
// include ---------------------------------------------------------------------
#include "stm32f10x.h"
#include "stdint.h"
#include "dev_can.h"
#include "mdebug/m_assert.h"
#include "dev_def.h"

M_MODULE_NAME("Bsp-Can")

// 待完善 ----------------------------------------------------------------------
// TODO 回环模式，尚未完成，近期不再理会此功能。2020-09-08。

// 数据结构 ---------------------------------------------------------------------
typedef struct can_baudrate_set_tag {
    uint8_t sjw;
    uint8_t bit_segment1;
    uint8_t bit_segment2;
    uint8_t prescaler;
} can_baudrate_set_t;

static const uint32_t can_baudrate_table[5] = {
    DevCanBaudRate_1M,
    DevCanBaudRate_500K,
    DevCanBaudRate_250K,
    DevCanBaudRate_125K,
    DevCanBaudRate_100K
};

static const can_baudrate_set_t CanBaudRateSet[5] = {
    // >    800K, 75%
    // >    500K, 80%
    // <=   500K, 87.5%
    // 36M / 9 / ( 1 + 2 + 1) = 1M
    { CAN_SJW_1tq,  CAN_BS1_2tq, CAN_BS2_1tq,  9  },    // 1000K, 75% 
    // 36M / 9 / ( 1 + 6 + 1) = 500K
    { CAN_SJW_1tq,  CAN_BS1_6tq, CAN_BS2_1tq,  9  },    // 500K,  87.5%
    // 36M / 18 / ( 1 + 6 + 1) = 250K
    { CAN_SJW_1tq,  CAN_BS1_6tq, CAN_BS2_1tq,  18 },    // 250K,  87.5%
    // 36M / 36 / ( 1 + 6 + 1) = 125K
    { CAN_SJW_1tq,  CAN_BS1_6tq, CAN_BS2_1tq,  36 },    // 125K,  87.5%
    // 36M / 45 / ( 1 + 6 + 1) = 100K
    { CAN_SJW_1tq,  CAN_BS1_6tq, CAN_BS2_1tq,  45 }     // 100K,  87.5%
};

// 设备指针 ---------------------------------------------------------------------
dev_can_t dev_can;

// ops -------------------------------------------------------------------------
static dev_err_t drv_can_enable(dev_can_t * can, bool enable);
static dev_err_t drv_can_config(dev_can_t * can, dev_can_config_t * config);
static void drv_can_send(dev_can_t * can, const dev_can_msg_t * msg);
static bool drv_can_send_busy(dev_can_t * can);
static void drv_can_config_filter(dev_can_t * can);

static const struct dev_can_ops can_ops = {
    drv_can_enable,
    drv_can_config,
    drv_can_send,
    drv_can_send_busy,
    drv_can_config_filter
};
 
// API -------------------------------------------------------------------------
void bsp_can_init(void)
{
    dev_can.ops = &can_ops;

    // Configure the NVIC Preemption Priority Bits
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
    // enabling interrupt
    NVIC_InitTypeDef NVIC_InitStruct;
    NVIC_InitStruct.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

    // Configure CAN1 Rx (PA11) as input pull-up
    GPIO_InitTypeDef GPIO_InitStruct; 
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Configure CAN1 Tx (PA12) as alternate function output push-pull.
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP; 
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStruct); 

    dev_can_config_t config_default = {
        DevCanBaudRate_125K, DevCanMode_Normal
    };
    drv_can_config(&dev_can, &config_default);

    dev_err_t ret = dev_can_reg(&dev_can, DEV_CAN, (void *)0);
    M_ASSERT(ret == Dev_OK);
}

static dev_err_t drv_can_config(dev_can_t * can, dev_can_config_t * config)
{
    (void)can;

    // CAN register init.
    CAN_DeInit(CAN1);
    
    int i = 0;
    for (; i < 5; i ++) {
        if (config->baud_rate == can_baudrate_table[i]) {
            break;
        }
    }
    M_ASSERT(i < 5);
    
    // CAN cell init
    CAN_InitTypeDef CAN_InitStruct;
    CAN_StructInit(&CAN_InitStruct);
    CAN_InitStruct.CAN_TTCM = DISABLE;
    CAN_InitStruct.CAN_ABOM = DISABLE;
    CAN_InitStruct.CAN_AWUM = DISABLE;
    CAN_InitStruct.CAN_NART = DISABLE;
    CAN_InitStruct.CAN_RFLM = DISABLE;
    CAN_InitStruct.CAN_TXFP = DISABLE;
    CAN_InitStruct.CAN_Mode = CAN_Mode_Normal;
    CAN_InitStruct.CAN_SJW = CanBaudRateSet[i].sjw;
    CAN_InitStruct.CAN_BS1 = CanBaudRateSet[i].bit_segment1;
    CAN_InitStruct.CAN_BS2 = CanBaudRateSet[i].bit_segment2;
    CAN_InitStruct.CAN_Prescaler = CanBaudRateSet[i].prescaler;
    CAN_Init(CAN1, &CAN_InitStruct);

    if (can->count_filter == 0) {
        // CAN filter init
        CAN_FilterInitTypeDef CAN_FilterInitStruct; 
        // 指定过滤器为0
        CAN_FilterInitStruct.CAN_FilterNumber = 0;
        // 指定过滤器为标识符屏蔽位模式
        CAN_FilterInitStruct.CAN_FilterMode = CAN_FilterMode_IdMask;
        // 过滤器位宽为32位
        CAN_FilterInitStruct.CAN_FilterScale = CAN_FilterScale_32bit;
        // 过滤器标识符的高16位值与低16位值
        CAN_FilterInitStruct.CAN_FilterIdHigh = 0;
        CAN_FilterInitStruct.CAN_FilterIdLow = 0;
        // 过滤器屏蔽标识符的高16位值与低16位值
        CAN_FilterInitStruct.CAN_FilterMaskIdHigh = 0;
        CAN_FilterInitStruct.CAN_FilterMaskIdLow = 0;
        // 设定了指向过滤器的FIFO为0
        CAN_FilterInitStruct.CAN_FilterFIFOAssignment = CAN_FIFO0;
        // 使能过滤器
        CAN_FilterInitStruct.CAN_FilterActivation = ENABLE;
        // 配置过滤器
        CAN_FilterInit(&CAN_FilterInitStruct);
    }
    
    // CAN FIFO0 message pending interrupt enable
    CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);
    CAN_ITConfig(CAN1, CAN_IT_TME, ENABLE); 
    
    return Dev_OK;
}

static dev_err_t drv_can_enable(dev_can_t * can, bool enable)
{
    (void)can;

    if (enable == true) {
        CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);
        CAN_ITConfig(CAN1, CAN_IT_TME, ENABLE); 
    }
    else {
        CAN_ITConfig(CAN1, CAN_IT_FMP0, DISABLE);
        CAN_ITConfig(CAN1, CAN_IT_TME, DISABLE); 
    }
    
    return Dev_OK;
}

static void drv_can_send(dev_can_t * can, const dev_can_msg_t * msg)
{
    CanTxMsg can_tx_msg;

    can_tx_msg.StdId = msg->ide == 0 ? (msg->id & 0x7FF) : 0;
    can_tx_msg.ExtId = msg->ide == 0 ? 0 : msg->id;
    can_tx_msg.RTR = msg->rtr == 0 ? CAN_RTR_DATA : CAN_RTR_REMOTE;
    can_tx_msg.IDE = msg->ide == 0 ? CAN_Id_Standard : CAN_Id_Extended;
    can_tx_msg.DLC = msg->len;
    for (int i = 0; i < can_tx_msg.DLC; i ++)
        can_tx_msg.Data[i] = msg->data[i];
    
    CAN_Transmit(CAN1, &can_tx_msg);
}

static void drv_can_recv(dev_can_t * can, dev_can_msg_t * msg)
{
    (void)can;
    
    CanRxMsg can_rx_msg;
    can_rx_msg.StdId = 0x00;
    can_rx_msg.ExtId = 0x00;
    can_rx_msg.IDE = CAN_ID_STD;
    can_rx_msg.DLC = 0x00;
    can_rx_msg.FMI = 0x00;
    for (int i = 0; i < 8; i ++)
        can_rx_msg.Data[i] = 0x00;
    CAN_Receive(CAN1, CAN_FIFO0, &can_rx_msg);

    msg->ide = can_rx_msg.IDE == CAN_Id_Standard ? 0 : 1;
    msg->rtr = can_rx_msg.RTR == CAN_RTR_DATA ? 0 : 1;
    msg->len = can_rx_msg.DLC;
    // 帧ID
    msg->id = can_rx_msg.IDE == CAN_Id_Standard ? can_rx_msg.StdId : can_rx_msg.ExtId;

    for (int i = 0; i < can_rx_msg.DLC; i ++)
        msg->data[i] = can_rx_msg.Data[i];
}

static bool drv_can_send_busy(dev_can_t * can)
{
    (void)can;

    for (int mailbox = 0; mailbox < 3; mailbox ++)
        if (CAN_TransmitStatus(CAN1, mailbox) == CAN_TxStatus_Ok)
            return false;
    
    return true;
}

static void drv_can_config_filter(dev_can_t * can)
{
    M_ASSERT(can->count_filter <= 14);

    for (int i = 0; i < can->count_filter; i ++) {
        // CAN filter init
        CAN_FilterInitTypeDef CanFilter; 
        // 指定过滤器为0
        CanFilter.CAN_FilterNumber = i;
        // 指定过滤器为标识符屏蔽位模式
        CanFilter.CAN_FilterMode = can->filter[i].mode == DevCanFilterMode_Mask ?
                                   CAN_FilterMode_IdMask :
                                   CAN_FilterMode_IdList;
        // 过滤器位宽为32位
        CanFilter.CAN_FilterScale = CAN_FilterScale_32bit;

        uint32_t id = 0;
        uint32_t mask = 0;
        // id
        id |= ((can->filter[i].id & 0x7ff));
        id <<= 21;
        id |= ((((can->filter[i].id & 0x1fffffff) >> 11) << 3));
        // mask
        mask |= ((can->filter[i].mask & 0x7ff));
        mask <<= 21;
        mask |= ((((can->filter[i].mask & 0x1fffffff) >> 11) << 3));
        if (can->filter[i].rtr == true)
            id |= (1 << 1);
        if (can->filter[i].ide == true)
            id |= (1 << 2);
        mask |= (1 << 1);
        mask |= (1 << 2);
        if (can->filter[i].mode == DevCanFilterMode_Mask) {
            // 过滤器标识符的高16位值与低16位值
            CanFilter.CAN_FilterIdHigh = (id >> 16);
            CanFilter.CAN_FilterIdLow = (id & 0xff);
            CanFilter.CAN_FilterMaskIdHigh = (mask >> 16);
            CanFilter.CAN_FilterMaskIdLow = (mask & 0xff);
        }
        else {
            // 过滤器标识符的高16位值与低16位值
            CanFilter.CAN_FilterIdHigh = (id >> 16);
            CanFilter.CAN_FilterIdLow = (id & 0xff);
            CanFilter.CAN_FilterMaskIdHigh = (id >> 16);
            CanFilter.CAN_FilterMaskIdLow = (id & 0xff);
        }
        // 设定了指向过滤器的FIFO为0
        CanFilter.CAN_FilterFIFOAssignment = CAN_FIFO0;
        // 使能过滤器
        CanFilter.CAN_FilterActivation = ENABLE;
        // 配置过滤器
        CAN_FilterInit(&CanFilter);
    }
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
    CAN_ITConfig(CAN1, CAN_IT_FMP0, DISABLE);

    dev_can_msg_t msg;
    drv_can_recv(&dev_can, &msg);
    dev_can_isr_recv(&dev_can, &msg);

    CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);  
}
