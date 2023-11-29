// include ---------------------------------------------------------------------
#include "drv_imu.h"
#include "dev_imu.h"
#include "mlog/mlog.h"
#include "dev_def.h"
#include "lpi2c_cmsis_user.h"
#include "Driver_I2C.h"
#include "fsl_gpt.h"
#include "meow.h"
#include "fsl_gpio.h"
#include "fsl_cache.h"
#include "rtk_def.h"
#include "dal_def.h"
#include "fusion.h"

M_TAG("Drv-Imu")

// 设备函数 ---------------------------------------------------------------------
static bool drv_bno055_read_busy(dev_imu_t * imu);
static void drv_bno055_trig_data(dev_imu_t * imu);

static const struct dev_imu_ops drv_imu_ops = {
    drv_bno055_read_busy,
    drv_bno055_trig_data,
};

// static function -------------------------------------------------------------
static void drv_imu_cmd(uint8_t reg_add, uint8_t trsdata, uint8_t num); 
static void I2C1_Callback(uint32_t event);

// 驱动 ------------------------------------------------------------------------
enum {
    ImuState_Init = 0,
    ImuState_Config,
    ImuState_WaitConfigEnd,
    ImuState_ConfigDelay,
    ImuState_Idle,
    ImuState_TrigAddr,
    ImuState_TrigRecieve,
    ImuState_WaitData,
};

#pragma pack(1)
typedef struct imu_config_data_tag {
    uint8_t addr;
    uint8_t value;
    uint8_t num;
} imu_config_data_t;
#pragma pack()

dev_imu_t dev_imu;
AT_NONCACHEABLE_SECTION(volatile uint8_t bno_state = ImuState_Init);
imu_config_data_t config_data[64];
uint8_t config_count = 0;
uint8_t config_end_count = 0;
uint64_t time_imu_ms = 0;
uint32_t time_trig_ms = 0;
bool imu_sm_en = false;

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
#define GPT_IRQ_ID                          GPT2_IRQn
#define EXAMPLE_GPT                         GPT2
#define EXAMPLE_GPT_IRQHandler              GPT2_IRQHandler
#define EXAMPLE_GPT_CLK_FREQ                CLOCK_GetFreq(kCLOCK_PerClk)

// api -------------------------------------------------------------------------
void drv_imu_bno055_init(void)
{
    i2c_init(&Driver_I2C1, I2C1_Callback);

    // 启动对BNO055的配置过程
    // Axis Config
    drv_imu_cmd(BNO055_ADDR_AXIS_MAP_CONFIG, 0, 2);
    drv_imu_cmd(BNO055_ADDR_AXIS_MAP_SIGN, 0, 2);
    // Power Mode
    drv_imu_cmd(BNO055_ADDR_PWR_MODE, BNO055_PWR_MODE_NORMAL, 2);
    // 去BNO055的第1页寄存器
    drv_imu_cmd(BNO055_ADDR_PAGEID, 1, 2);
    // 设置六轴数据的数据频率
    drv_imu_cmd(BNO055_ADDR_GYR_CFG0, BNO055_CFG_GYR_116Hz, 2);
    drv_imu_cmd(BNO055_ADDR_ACC_CFG, BNO055_CFG_ACC_125Hz, 2);
    // 回到BNO055的第0页寄存器
    drv_imu_cmd(BNO055_ADDR_PAGEID, 0, 2);
    // 设置操作模式
    drv_imu_cmd(BNO055_ADDR_OPR_MODE, BNO055_OPR_MODE_AMG, 2);

    // IMU延时一段时间再进入数据读取模式。
    bno_state = ImuState_Config;
    
    // GPT定时器
    uint32_t gptFreq;
    gpt_config_t gptConfig;
    GPT_GetDefaultConfig(&gptConfig);
    GPT_Init(EXAMPLE_GPT, &gptConfig);
    /* Divide GPT clock source frequency by 3 inside GPT module */
    GPT_SetClockDivider(EXAMPLE_GPT, 1);

    /* Get GPT clock frequency */
    gptFreq = EXAMPLE_GPT_CLK_FREQ;

    /* GPT frequency is divided by 3 inside module */
    gptFreq /= 1000;

    /* Set both GPT modules to 1 second duration */
    GPT_SetOutputCompareValue(EXAMPLE_GPT, kGPT_OutputCompare_Channel1, gptFreq);

    /* Enable GPT Output Compare1 interrupt */
    GPT_EnableInterrupts(EXAMPLE_GPT, kGPT_OutputCompare1InterruptEnable);

    /* Enable at the Interrupt */
    NVIC_SetPriority(GPT_IRQ_ID, 0);
    EnableIRQ(GPT_IRQ_ID);
    GPT_StartTimer(EXAMPLE_GPT);
    
    M_DEBUG("drv_imu_bno055_init, TMR3 Init, State WaitData to Idle.");

    // IMU ops
    dev_imu.ops = &drv_imu_ops;

    // register a imu device
    dev_err_t ret = imu_reg(&dev_imu, DEV_IMU, (void *)0);
    M_ASSERT(ret == Dev_OK);
    
    //imu_sm_en = true;
}

void drv_imu_trig(void)
{
    //drv_bno055_trig_data(&dev_imu);
}

// static function -------------------------------------------------------------
static bool drv_bno055_read_busy(dev_imu_t * imu)
{
    (void)imu;

    return bno_state == ImuState_Idle ? true : false;
}

static void drv_bno055_trig_data(dev_imu_t * imu)
{
    if (port_get_time_ms() < 5000)
        return;
    if (imu->super.en == false)
        return;
    if (bno_state != ImuState_Idle)
        return;

    uint8_t address = 8;
    Driver_I2C1.MasterTransmit(0x29, &address, 1, false);
    bno_state = ImuState_TrigAddr;
}

static void drv_imu_cmd(uint8_t reg_add, uint8_t trsdata, uint8_t num)
{
    config_data[config_count].addr = reg_add;
    config_data[config_count].value = trsdata;
    config_data[config_count].num = num;
    config_count ++;
}

m_imu_data_t data;
static void I2C1_Callback(uint32_t event)
{
    uint8_t count;
    if (event & ARM_I2C_EVENT_TRANSFER_DONE) {
        if (bno_state == ImuState_WaitConfigEnd) {
            config_end_count ++;
            if (config_end_count < config_count)
                bno_state = ImuState_Config;
            else
                bno_state = ImuState_Idle;
            M_DEBUG("I2C1_Callback, ARM_I2C_EVENT_TRANSFER_DONE. State: %d. Count: %d.",
                    bno_state, config_end_count);
        }
        else if (bno_state == ImuState_TrigAddr) {
            //M_DEBUG("I2C1_Callback, TrigAddr to Idle");
            Driver_I2C1.MasterReceive(0x29, &dev_imu.data, 18, false);
            bno_state = ImuState_Idle;
        }
        else if (bno_state == ImuState_Idle) {
            data.time = time_trig_ms;
            for (int i = 0; i < 3; i ++)
                data.acc[i] = (float)dev_imu.data.acc[i] / 100.0;
            for (int i = 0; i < 3; i ++)
                data.gyr[i] = (float)dev_imu.data.gyr[i] / 16.0;
            data.speed = (dal.status.motor.speed_r + dal.status.motor.speed_l) / 2;
            
            for (int i = 0; i < 3; i ++)
                if (data.acc[i] > 10)
                    M_INFO("Acc %d Wrong Data: %f.", i, data.acc[i]);
            if (data.acc[2] < 2.0)
                M_INFO("Acc 2 Wrong Data: %f.", data.acc[2]);
            for (int i = 0; i < 3; i ++)
                if (data.gyr[i] > 10)
                    M_INFO("Gyr %d Wrong Data: %f.", i, data.gyr[i]);
    
            fusion_set_imu(&data);
        }
        return;
    }
    if (event & ARM_I2C_EVENT_ADDRESS_NACK) {
        M_WARN("I2C1_Callback, ARM_I2C_EVENT_ADDRESS_NACK");
        return;
    }
    if (event & ARM_I2C_EVENT_TRANSFER_INCOMPLETE) {
        M_WARN("I2C1_Callback, ARM_I2C_EVENT_TRANSFER_INCOMPLETE");
        return;
    }
    if (event & ARM_I2C_EVENT_BUS_ERROR) {
        M_ERROR("I2C1_Callback, ARM_I2C_EVENT_BUS_ERROR");
        return;
    }
    if (event & ARM_I2C_EVENT_BUS_CLEAR) {
        M_ERROR("I2C1_Callback, ARM_I2C_EVENT_BUS_CLEAR");
        return;
    }
    if (event & ARM_I2C_EVENT_SLAVE_TRANSMIT) {
        M_ERROR("I2C1_Callback, ARM_I2C_EVENT_SLAVE_TRANSMIT");
        return;
    }
    if (event & ARM_I2C_EVENT_SLAVE_RECEIVE) {
        M_ERROR("I2C1_Callback, ARM_I2C_EVENT_SLAVE_RECEIVE");
        return;
    }
    if (event & ARM_I2C_EVENT_GENERAL_CALL) {
        M_WARN("I2C1_Callback, ARM_I2C_EVENT_GENERAL_CALL");
        return;
    }
    if (event & ARM_I2C_EVENT_ARBITRATION_LOST) {
        M_ERROR("I2C1_Callback, ARM_I2C_EVENT_ARBITRATION_LOST");
        return;
    }
}

// IMU状态机轮询 ---------------------------------------------------------------
void drv_imu_poll(void)
{
    DisableIRQ(LPI2C1_IRQn);
    uint32_t bno_state_temp = bno_state;
    DisableIRQ(LPI2C1_IRQn);
    
    switch (bno_state_temp) {
        case ImuState_Config: {
            uint8_t i = config_end_count;
            Driver_I2C1.MasterTransmit(0x29, &config_data[i], config_data[i].num, false);
            bno_state_temp = ImuState_WaitConfigEnd;
            break;
        }
        
        default:
            break;
    }
    
    DisableIRQ(LPI2C1_IRQn);
    bno_state = bno_state_temp;
    DisableIRQ(LPI2C1_IRQn);
}

uint32_t error_count_trigaddr = 0;
uint32_t trig_count = 0;
uint32_t time_unit_ms = 100;
bool pps_status = true;
bool pps_status_bkp = true;
uint32_t time_pps_ms = 0;
void EXAMPLE_GPT_IRQHandler(void)
{
    GPT_ClearStatusFlags(EXAMPLE_GPT, kGPT_OutputCompare1Flag);
    
    if (imu_sm_en == false)
        return;
    
    time_imu_ms ++;
    pps_status = GPIO_PinRead(GPIO3, 4) == 0 ? false : true;
    if (pps_status_bkp == false && pps_status == true) {
        //M_DEBUG("GPT_IRQHandler, time_imu_ms: %lu.", time_imu_ms);
        time_pps_ms = time_imu_ms;
        time_imu_ms = 0;
    }
    pps_status_bkp = pps_status;
    
    if ((time_imu_ms % time_unit_ms) != 0)
        return;

    if (bno_state != ImuState_Idle)
        return;
    
    bno_state = ImuState_TrigAddr;
    uint8_t address = 8;
    Driver_I2C1.Control(ARM_I2C_ABORT_TRANSFER, 0);
    Driver_I2C1.MasterTransmit(0x29, &address, 1, false);
    time_trig_ms = time_imu_ms;
}
