// include ---------------------------------------------------------------------
#include "drv_imu.h"
#include "dev_imu.h"
#include "mlog/mlog.h"
#include "dev_def.h"
#include "lpi2c_cmsis_user.h"
#include "fsl_lpi2c.h"
#include "fsl_gpt.h"
#include "meow.h"
#include "fsl_gpio.h"
#include "fsl_cache.h"
#include "rtk_def.h"
#include "dal_def.h"
#include "fusion.h"

M_TAG("Drv-Imu")

// 数据结构 --------------------------------------------------------------------
typedef struct drv_imu_data_tag {
    lpi2c_master_handle_t handle;
    lpi2c_master_transfer_t xfer;
    
    m_imu_data_t data;                              // 采集到的数据
    
    bool en_manual;                                 // 手动使能关闭
    bool en_trig;                                   // 触发数据使能
    bool busy;                                      // IMU忙标志位
    bool pps_pulse_first;                           // 第一个PPS脉冲
    
    uint32_t time_gpt2;                             // 定时器计时
    uint32_t time_trig_ms;                          // IMU触发时间
    uint32_t time_rtk_ms;                           // RTK时间
} drv_imu_data_t;

// 设备函数 --------------------------------------------------------------------
static bool drv_bno055_read_busy(dev_imu_t * imu);
static void drv_bno055_trig_data(dev_imu_t * imu);

static const struct dev_imu_ops drv_imu_ops = {
    drv_bno055_read_busy,
    drv_bno055_trig_data,
};

// static function -------------------------------------------------------------
static void drv_imu_cmd(uint8_t reg_add, uint8_t trsdata); 

// 驱动 ------------------------------------------------------------------------
dev_imu_t dev_imu;
AT_NONCACHEABLE_SECTION(drv_imu_data_t drv_imu);

// define ----------------------------------------------------------------------
// Address
#define BNO055_ADDR_AXIS_MAP_CONFIG         (0X41)
#define BNO055_ADDR_AXIS_MAP_SIGN           (0X42)
#define BNO055_ADDR_PWR_MODE                (0x3E)
#define BNO055_ADDR_OPR_MODE                (0x3D)
#define BNO055_ADDR_ACC_CFG                 (0x08)
#define BNO055_ADDR_MAG_CFG                 (0x09)
#define BNO055_ADDR_GYR_CFG0                (0x0A)
#define BNO055_ADDR_PAGEID                  (0x07)

// Power MOde
#define BNO055_PWR_MODE_NORMAL              (0x00)

// Operate Mode (Page54)
#define BNO055_OPR_MODE_IMUPLUS             (0x08)
#define BNO055_OPR_MODE_ORIGIN              (0x07)
#define BNO055_OPR_MODE_AMG                 (0x07)
#define BNO055_OPR_MODE_CFG                 (0x00)

// Config
#define BNO055_CFG_GYR_116Hz                (0x10)      // Page28
#define BNO055_CFG_GYR_523Hz                (0x00)
#define BNO055_CFG_ACC_125Hz                (0x11)      // Page27
#define BNO055_CFG_ACC_500Hz                (0x19)

// ADDR
#define BNO055_ACCEL_DATA_X_LSB_ADDR        (0x08)

// def -------------------------------------------------------------------------
#define EXAMPLE_GPT_CLK_FREQ                

#include "fsl_clock.h"
#include "fsl_common.h"
#include "fsl_iomuxc.h"
#include "pin_mux.h"
#include "board.h"

/* Select USB1 PLL (480 MHz) as master lpi2c clock source */
#define LPI2C_CLOCK_SOURCE_SELECT (0U)
/* Clock divider for master lpi2c clock source */
#define LPI2C_CLOCK_SOURCE_DIVIDER (5U)
/* Get frequency of lpi2c clock */
#define LPI2C_CLOCK_FREQUENCY ((CLOCK_GetFreq(kCLOCK_Usb1PllClk) / 8) / (LPI2C_CLOCK_SOURCE_DIVIDER + 1U))

uint32_t LPI2C1_GetFreq(void)
{
    return LPI2C_CLOCK_FREQUENCY;
}

// api -------------------------------------------------------------------------

static void lpi2c_master_callback(LPI2C_Type *base, lpi2c_master_handle_t *handle, status_t status, void *userData)
{
    if (status == kStatus_Success) {
        drv_imu.data.time = (drv_imu.time_trig_ms + drv_imu.time_rtk_ms);
        for (int i = 0; i < 3; i ++)
            drv_imu.data.acc[i] = (float)dev_imu.data.acc[i] / 100.0;
        for (int i = 0; i < 3; i ++)
            drv_imu.data.gyr[i] = (float)dev_imu.data.gyr[i] / 16.0;
        drv_imu.data.speed = (dal.status.motor.speed_r - dal.status.motor.speed_l) / 2000.0;

        fusion_set_imu(&drv_imu.data);
            
        drv_imu.busy = false;
    }
    else {
        M_ERROR("Master Send Error: %d.", status);
    }
}

void drv_imu_bno055_init(void)
{
    // 数据
    drv_imu.pps_pulse_first = false;
    drv_imu.en_trig = false;
    drv_imu.en_manual = true;
    drv_imu.busy = true;
    
    CLOCK_EnableClock(kCLOCK_Iomuxc);

    // GPIO
    IOMUXC_SetPinMux(IOMUXC_GPIO_AD_B1_00_LPI2C1_SCL, 1U);
    IOMUXC_SetPinMux(IOMUXC_GPIO_AD_B1_01_LPI2C1_SDA, 1U);
    IOMUXC_SetPinConfig(IOMUXC_GPIO_AD_B1_00_LPI2C1_SCL, 0xD8B0u);
    IOMUXC_SetPinConfig(IOMUXC_GPIO_AD_B1_01_LPI2C1_SDA, 0xD8B0u);
    
    // I2C外设初始化
    lpi2c_master_config_t master_config;
    LPI2C_MasterGetDefaultConfig(&master_config);
    LPI2C_MasterInit(LPI2C1, &master_config, LPI2C1_GetFreq());
    LPI2C_MasterTransferCreateHandle(LPI2C1, &drv_imu.handle, lpi2c_master_callback, NULL);
    NVIC_SetPriority(LPI2C1_IRQn, 0);
    EnableIRQ(LPI2C1_IRQn);

    // 启动对BNO055的配置过程
    // Axis Config
    drv_imu_cmd(BNO055_ADDR_AXIS_MAP_CONFIG, 0);
    drv_imu_cmd(BNO055_ADDR_AXIS_MAP_SIGN, 0);
    // Power Mode
    drv_imu_cmd(BNO055_ADDR_PWR_MODE, BNO055_PWR_MODE_NORMAL);
    // 去BNO055的第1页寄存器
    drv_imu_cmd(BNO055_ADDR_PAGEID, 1);
    // 设置六轴数据的数据频率
    drv_imu_cmd(BNO055_ADDR_GYR_CFG0, BNO055_CFG_GYR_116Hz);
    drv_imu_cmd(BNO055_ADDR_ACC_CFG, BNO055_CFG_ACC_125Hz);
    // 回到BNO055的第0页寄存器
    drv_imu_cmd(BNO055_ADDR_PAGEID, 0);
    // 设置操作模式
    drv_imu_cmd(BNO055_ADDR_OPR_MODE, BNO055_OPR_MODE_IMUPLUS);

    // IMU ops
    dev_imu.ops = &drv_imu_ops;

    // register a imu device
    dev_err_t ret = imu_reg(&dev_imu, DEV_IMU, (void *)0);
    M_ASSERT(ret == Dev_OK);

    // xfer read
    drv_imu.xfer.slaveAddress = 0x29;
    drv_imu.xfer.direction = kLPI2C_Read;
    drv_imu.xfer.subaddress = (uint32_t)0x08;
    drv_imu.xfer.subaddressSize = 1;
    drv_imu.xfer.data = &dev_imu.data;
    drv_imu.xfer.dataSize = 26 - 1;
    drv_imu.xfer.flags = kLPI2C_TransferDefaultFlag;
    
    // GPT定时器
//    gpt_config_t gptConfig;
//    GPT_GetDefaultConfig(&gptConfig);
//    GPT_Init(GPT2, &gptConfig);
//    GPT_SetClockDivider(GPT2, 1);
//    uint32_t gpt_freq = CLOCK_GetFreq(kCLOCK_PerClk) / 1000;
//    GPT_SetOutputCompareValue(GPT2, kGPT_OutputCompare_Channel1, gpt_freq);
//    GPT_EnableInterrupts(GPT2, kGPT_OutputCompare1InterruptEnable);
//    NVIC_SetPriority(GPT2_IRQn, 0);
//    EnableIRQ(GPT2_IRQn);
//    GPT_StartTimer(GPT2);
//    
//    drv_imu.busy = false;
}

void drv_imu_trig_enable(void)
{
    drv_imu.en_trig = true;
}

// static function -------------------------------------------------------------
static bool drv_bno055_read_busy(dev_imu_t * imu)
{
    (void)imu;

    return false;
}

static void drv_bno055_trig_data(dev_imu_t * imu)
{
    if (port_get_time_ms() < 5000)
        return;
    if (imu->super.en == false)
        return;
}

static void drv_imu_cmd(uint8_t reg_add, uint8_t trsdata)
{
    drv_imu.xfer.slaveAddress = 0x29;
    drv_imu.xfer.direction = kLPI2C_Write;
    drv_imu.xfer.subaddress = (uint32_t)reg_add;
    drv_imu.xfer.subaddressSize = 1;
    drv_imu.xfer.data = &trsdata;
    drv_imu.xfer.dataSize = 1;
    drv_imu.xfer.flags = kLPI2C_TransferDefaultFlag;
    
    int ret = LPI2C_MasterTransferBlocking(LPI2C1, &drv_imu.xfer);
    if (ret != kStatus_Success) {
        M_ERROR("MasterTransferBlocking Error: %d.", ret);
    }
}

// IMU状态机轮询 ---------------------------------------------------------------
uint32_t u32ISOTPTimerTickCount = 0;
bool pps_status = true;
bool pps_status_bkp = true;
void GPT2_IRQHandler(void)
{
    u32ISOTPTimerTickCount ++;
    
    GPT_ClearStatusFlags(GPT2, kGPT_OutputCompare1Flag);
    
    if (drv_imu.time_gpt2 < 999)
        drv_imu.time_gpt2 ++;
    pps_status = GPIO_PinRead(GPIO3, 4) == 0 ? false : true;
    if (pps_status_bkp == false && pps_status == true) {
        drv_imu.time_gpt2 = 0;
        if (drv_imu.en_trig == true)
            drv_imu.pps_pulse_first = true;
        
        drv_imu.time_rtk_ms = (fusion_get_time() / 1000 + 1) * 1000;
    }
    pps_status_bkp = pps_status;
    
    if (drv_imu.en_manual == false)
        return;
    
    if (drv_imu.en_trig == false || drv_imu.pps_pulse_first == false)
        return;
    
    if ((drv_imu.time_gpt2 % 10) != 0)
        return;
    if (drv_imu.busy == true && (drv_imu.time_gpt2 % 1000 == 0)) {
        M_ERROR("GPT2_IRQHandler Master Busy.");
        return;
    }

    drv_imu.busy = true;
    int ret = LPI2C_MasterTransferNonBlocking(LPI2C1, &drv_imu.handle, &drv_imu.xfer);
    if (ret != kStatus_Success) {
        M_ERROR("Master TransferNonBlocking Error: %d.", ret);
    }
    drv_imu.time_trig_ms = drv_imu.time_gpt2;
}
