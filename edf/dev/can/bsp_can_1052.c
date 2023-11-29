
// include ---------------------------------------------------------------------
#include "Driver_CAN.h"
#include "dev_can.h"
#include "dev_def.h"
#include "fsl_iomuxc.h"
#include "mlog/mlog.h"
#include "pin_mux.h"
#include "stdint.h"
#include "canModuleIdUserDefine.h"
#include "fsl_gpt.h"
#include "port_isotp.h"
#include "isotp.h"
#include "canModuleIdUserDefine.h"

M_TAG("Bsp-Can")

#define CAN_NODE_LOOPBACK 0
#define CAN_NODE_A 1
#define CAN_NODE_B 2

#define CAN_NODE_MODE CAN_NODE_B

#if CAN_NODE_MODE == CAN_NODE_A

#define TX_ID 0x123
#define RX_ID 0x321

#elif CAN_NODE_MODE == CAN_NODE_B

#define TX_ID 0x321
#define RX_ID 0x123

#else

#define TX_ID 0x111
#define RX_ID 0x111

#endif
#define TX_FC_MB_INDEX 2
#define TX_MB_INDEX 1
#define RX_MB_INDEX 0

#define CAN_SEND_EVENT_SENDSTART 0x0001
#define CAN_SEND_EVENT_BUSIDLE 0x0002
#define CAN_SEND_EVENT_FC_PERMIT 0x0004

// 待完善 ----------------------------------------------------------------------
// TODO 回环模式，尚未完成，近期不再理会此功能。2020-09-08。

// 设备指针 --------------------------------------------------------------------
dev_can_t dev_can;

extern ARM_DRIVER_CAN Driver_CAN2;
static ARM_CAN_MSG_INFO tx_msg;
static ARM_CAN_MSG_INFO rx_msg;
static uint8_t rx_data[10] = {0};
static bool can_send_busy = false;

S_ISOTP_INIT_TYPE sMyIsotpCanDataStruct = {
    .CANDataTxID = ISOTP_ID_RTK_DATA_TX,
    .CANDataRxID = ISOTP_ID_RTK_DATA_RX,
    
    .CANMsgInfo.StdId = CAN_ID_RTK_INFO,
    .CANMsgInfo.DLC = 8,
    .CANMsgInfo.Data = {11,8,1,2,3,4,5,6},
    
    .isotp_GetTick = ISOTPGetSysTick,
    .CAN_sendmsg = CAN_sendmsg,
    
    .sIsotpCanTxAndRxData.bFlagOfRxCompleted = false,
    .sIsotpCanTxAndRxData.bFlagOfTxCompleted = true,
    .sIsotpCanTxAndRxData.dataLen = 4095,
    .sIsotpCanTxAndRxData.RecBuf = {0},
};

typedef struct {
    uint32_t time;
    uint8_t flag;
    double lon;
    double lat;
    float height;
} S_GPS_DATA_T;

S_GPS_DATA_T sGPSDataStruct = {
    .time = 0,
    .flag = 0,
    .lon = 0,
    .lat = 0,
    .height = 0, 
};

// ops -------------------------------------------------------------------------
static dev_err_t drv_can_enable(dev_can_t *can, bool enable);
static dev_err_t drv_can_config(dev_can_t *can, dev_can_config_t *config);
static bool drv_can_send(dev_can_t *can, const dev_can_msg_t *msg);
static bool drv_can_send_busy(dev_can_t *can);

static const struct dev_can_ops can_ops = {
    drv_can_enable,
    drv_can_config,
    drv_can_send,
    drv_can_send_busy
};

// API -------------------------------------------------------------------------
void bsp_can_init(void)
{
    dev_can.ops = &can_ops;

    dev_can_config_t config_default = {DevCanBaudRate_500K, DevCanMode_Normal};
    CLOCK_EnableClock(kCLOCK_Iomuxc); /* iomuxc clock (iomuxc_clk_enable): 0x03U */

    IOMUXC_SetPinMux(IOMUXC_GPIO_AD_B0_14_FLEXCAN2_TX, /* GPIO_AD_B0_14 is configured as FLEXCAN2_TX */
                     0U);                              /* Software Input On Field: Input Path is determined by functionality */
    IOMUXC_SetPinMux(IOMUXC_GPIO_AD_B0_15_FLEXCAN2_RX, /* GPIO_AD_B0_15 is configured as FLEXCAN2_RX */
                     0U);

    dev_err_t ret = dev_can_reg(&dev_can, DEV_CAN, (void *)0);
    M_ASSERT(ret == Dev_OK);
    
    // GPT定时器
    gpt_config_t gptConfig;
    GPT_GetDefaultConfig(&gptConfig);
    GPT_Init(GPT1, &gptConfig);
    GPT_SetClockDivider(GPT1, 1);
    uint32_t gpt_freq = CLOCK_GetFreq(kCLOCK_PerClk) / 1000;
    GPT_SetOutputCompareValue(GPT1, kGPT_OutputCompare_Channel1, gpt_freq);
    GPT_EnableInterrupts(GPT1, kGPT_OutputCompare1InterruptEnable);
    NVIC_SetPriority(GPT1_IRQn, 1);
    EnableIRQ(GPT1_IRQn);
    GPT_StartTimer(GPT1);
    
    // ISOTP_Init(&sMyIsotpCanDataStruct);
}

void Can_Callback_SignalUnit(uint32_t event)
{
    if (event & ARM_CAN_EVENT_UNIT_INACTIVE)  //设备进入非活跃状态  ：不支持
    {
    }
    if (event & ARM_CAN_EVENT_UNIT_ACTIVE)  //设备进入非激活状态   ：不支持
    {
    }
    if (event & ARM_CAN_EVENT_UNIT_WARNING)  //设备进入错误警告状态 ：不支持
    {
    }
    if (event & ARM_CAN_EVENT_UNIT_PASSIVE)  //设备进入错误被动状态 ：支持
    {
        __asm("nop");
    }

    if (event & ARM_CAN_EVENT_UNIT_BUS_OFF)  //设备进入总线关闭状态 ：支持
    {
        __asm("nop");
    }
}

void Can_Callback_SignalObject(uint32_t obj_idx, uint32_t event)
{
    if (event & ARM_CAN_EVENT_RECEIVE) {
        if (obj_idx == RX_MB_INDEX) {
            int len = Driver_CAN2.MessageRead(RX_MB_INDEX, &rx_msg, rx_data, 8); //启动接收
            if (rx_msg.id == 0x467 && rx_msg.id == 0x465) {
                dev_can_msg_t msg;
                msg.id = rx_msg.id;
                msg.ide = 0;
                msg.rtr = rx_msg.rtr;
                msg.len = rx_msg.dlc;
                for (int i = 0; i < msg.len; i ++)
                    msg.data[i] = rx_data[i];
                dev_can_isr_recv(&dev_can, &msg);
            }
            else {
                CanMsg_type canMsg;
                canMsg.StdId = rx_msg.id;
                canMsg.DLC = rx_msg.dlc;
                for (int i = 0; i < canMsg.DLC; i ++)
                    canMsg.Data[i] = rx_data[i];
                // ISOTP协议的解析
                // if (!ISOTP_parseMsg(&canMsg)) {
                //     if (true == sMyIsotpCanDataStruct.sIsotpCanTxAndRxData.bFlagOfRxCompleted) {
                //         memcpy(&sGPSDataStruct.time, &sMyIsotpCanDataStruct.sIsotpCanTxAndRxData.RecBuf[0], 4);
                //         memcpy(&sGPSDataStruct.flag, &sMyIsotpCanDataStruct.sIsotpCanTxAndRxData.RecBuf[4], 1);
                //         memcpy(&sGPSDataStruct.lon, &sMyIsotpCanDataStruct.sIsotpCanTxAndRxData.RecBuf[5], 8);
                //         memcpy(&sGPSDataStruct.lat, &sMyIsotpCanDataStruct.sIsotpCanTxAndRxData.RecBuf[13], 8);
                //         memcpy(&sGPSDataStruct.height, &sMyIsotpCanDataStruct.sIsotpCanTxAndRxData.RecBuf[21], 4);

                //         sMyIsotpCanDataStruct.sIsotpCanTxAndRxData.bFlagOfRxCompleted = false;
                //     }
                // }
            }
        }
    }
    if (event & ARM_CAN_EVENT_SEND_COMPLETE) {
        if (obj_idx == TX_MB_INDEX) {
        }
        else if (obj_idx == TX_FC_MB_INDEX) { //流控帧专用邮箱，不做处理
        }
    }
    if (event & ARM_CAN_EVENT_RECEIVE_OVERRUN) {
        __asm("nop");
    }
}

int ret = 0;
static dev_err_t drv_can_config(dev_can_t *can, dev_can_config_t *config)
{
    /* Set CAN_CLK_PODF. */
    CLOCK_SetDiv(kCLOCK_CanDiv, 1);
    /* Set Can clock source. */
    CLOCK_SetMux(kCLOCK_CanMux, 2);
    // Freq = 40M (480/6/2)
    ret = Driver_CAN2.Initialize(Can_Callback_SignalUnit, Can_Callback_SignalObject);
    M_ASSERT(ret == ARM_DRIVER_OK);
    ret = Driver_CAN2.PowerControl(ARM_POWER_FULL);

#if CAN_NODE_MODE != CAN_NODE_LOOPBACK
    ret = Driver_CAN2.SetMode(ARM_CAN_MODE_NORMAL);  //配置普通模式
    M_ASSERT(ret == ARM_DRIVER_OK);
#else
    Driver_CAN2.SetMode(ARM_CAN_MODE_LOOPBACK_INTERNAL);  //配置回环模式
#endif
    ret = Driver_CAN2.SetBitrate(ARM_CAN_BITRATE_NOMINAL, 500000U,
                            ARM_CAN_BIT_PROP_SEG(5U) | ARM_CAN_BIT_PHASE_SEG1(2U) |
                            ARM_CAN_BIT_PHASE_SEG2(2U) | ARM_CAN_BIT_SJW(1U));
    M_ASSERT(ret == ARM_DRIVER_OK);
    ret = Driver_CAN2.ObjectConfigure(TX_MB_INDEX, ARM_CAN_OBJ_TX);                            //配置发送邮箱
    M_ASSERT(ret == ARM_DRIVER_OK);
    ret = Driver_CAN2.ObjectConfigure(RX_MB_INDEX, ARM_CAN_OBJ_RX);                            //配置接收邮箱
    M_ASSERT(ret == ARM_DRIVER_OK);
    ret = Driver_CAN2.ObjectSetFilter(RX_MB_INDEX, ARM_CAN_FILTER_ID_EXACT_ADD, 0x467, 0);     //配置接收邮箱的过滤器
    M_ASSERT(ret == ARM_DRIVER_OK);
    ret = Driver_CAN2.ObjectSetFilter(RX_MB_INDEX, ARM_CAN_FILTER_ID_EXACT_ADD, 0x465, 0);     //配置接收邮箱的过滤器
    M_ASSERT(ret == ARM_DRIVER_OK);
    ret = Driver_CAN2.ObjectSetFilter(RX_MB_INDEX, ARM_CAN_FILTER_ID_EXACT_ADD, ISOTP_ID_RTK_DATA_RX, 0);
    M_ASSERT(ret == ARM_DRIVER_OK);
    
    return Dev_OK;
}

static dev_err_t drv_can_enable(dev_can_t *can, bool enable)
{   
    return Dev_OK;
}

static bool drv_can_send(dev_can_t *can, const dev_can_msg_t *msg)
{
    (void)can;

    tx_msg.id  = msg->id;
    tx_msg.dlc = msg->len;
    tx_msg.rtr = 0;
    M_ASSERT(msg->data != NULL);
    DisableIRQ(GPT1_IRQn);
    int32_t ret = Driver_CAN2.MessageSend(TX_MB_INDEX, &tx_msg, msg->data, msg->len);
    EnableIRQ(GPT1_IRQn);
    if (ret == ARM_DRIVER_ERROR) {
        return false;
    }
    else {
        return true;
    }
}

static void drv_can_recv(dev_can_t *can, dev_can_msg_t *msg)
{
    
}

static bool drv_can_send_busy(dev_can_t *can)
{
    return false;
}

uint32_t time_gpt1_count = 0;
void GPT1_IRQHandler(void)
{
    GPT_ClearStatusFlags(GPT1, kGPT_OutputCompare1Flag);
    
    time_gpt1_count ++;
}

bool isotp_send_1st = true;
device_t * can;
bool CAN_sendmsg(CanMsg_type * Msg)
{
    bool ret = true;
    
    if (isotp_send_1st == true) {
        isotp_send_1st = false;
        can = device_find(DEV_CAN);
        M_ASSERT(can != 0);
    }
    
    dev_can_msg_t can_msg;
    can_msg.rtr = 0;
    can_msg.ide = 0;
    can_msg.len = Msg->DLC;
    for (int i = 0; i < can_msg.len; i ++) {
        can_msg.data[i] = Msg->Data[i];
    }
    dev_can_send(can, &can_msg);
        
    return ret;
}
