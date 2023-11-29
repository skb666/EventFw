// include ---------------------------------------------------------------------
#include "system_LPC17xx.h"
#include "LPC17xx.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_clkpwr.h"
#include "drv_imu.h"
#include "dev_imu.h"
#include "mdebug/m_assert.h"
#include "dev_def.h"
#include "meow.h"

// TODO 对BNO055的驱动，进行稳定性测试。

M_MODULE_NAME("Drv-Imu-Bno055-1768")

// 设备函数 ---------------------------------------------------------------------
static bool drv_bno055_read_busy(dev_imu_t * imu);
static void drv_bno055_trig_data(dev_imu_t * imu);

static const struct dev_imu_ops drv_imu_ops = {
    drv_bno055_read_busy,
    drv_bno055_trig_data,
};

// static function -------------------------------------------------------------
static void drv_imu_cmd(uint8_t cmd, uint8_t para);
static void imu_delay_ms(__IO uint32_t count);

// 驱动 ------------------------------------------------------------------------
enum {
    ImuState_Init = 0,
    ImuState_Delay,
    ImuState_Work,
    ImuState_Error,
    ImuState_ErrorDelay,
};

dev_imu_t dev_imu;
dev_imu_data_t imu_data;
bool imu_read_busy = false;
bool imu_i2c_error = false;
uint8_t imu_state = ImuState_Init;
uint64_t time_imu_delay_ms;

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

#define BIT(x)                              ((uint32_t)1 << (x))

// API -------------------------------------------------------------------------
uint64_t time_start = 0;
void drv_imu_1768_init(void)
{
    LPC_SC->PCONP |= (1 << 26);
    
    // Output
    LPC_PINCON->PINSEL3 &= ~BIT(13);            // P1_22
    LPC_PINCON->PINSEL3 &= ~BIT(12);
    
    LPC_PINCON->PINSEL2 &= ~BIT(3);             // P1_1
    LPC_PINCON->PINSEL2 &= ~BIT(2);
    
    // OUTPUT
    LPC_GPIO1->FIODIR |=BIT(1)+BIT(22);
    
    // pull-up 
    LPC_PINCON->PINMODE3 &= ~BIT(13);           // P1_22
    LPC_PINCON->PINMODE3 &= ~BIT(12);
    
    // pull-up
    LPC_PINCON->PINMODE2 &= ~BIT(3);            // P1_1
    LPC_PINCON->PINMODE2 &= ~BIT(2);

    // Input 
    LPC_PINCON->PINSEL1 &= ~BIT(9);             // P0_20
    LPC_PINCON->PINSEL1 &= ~BIT(9);

    // Input
    LPC_GPIO0->FIODIR &= ~BIT(20);
    
    // pull-up
    LPC_PINCON->PINMODE4 &= ~BIT(9);
    LPC_PINCON->PINMODE4 &= ~BIT(8);
    
    LPC_GPIO1->FIOSET |= BIT(22);               // Boot Off
    
    LPC_PINCON->PINSEL0 |= BIT(21);             // P0_10  SDA1
    LPC_PINCON->PINSEL0 &= ~BIT(20);
    
    LPC_PINCON->PINSEL0 |= BIT(23);             // P0_11  SCL1
    LPC_PINCON->PINSEL0 &= ~BIT(22);
    
    LPC_PINCON->PINMODE0 |= BIT(21);            // 10
    LPC_PINCON->PINMODE0 &= ~BIT(20);
    
    LPC_PINCON->PINMODE0 |= BIT(23);            // 11
    LPC_PINCON->PINMODE0 &= ~BIT(22);
    
    LPC_PINCON->PINMODE_OD0 |= BIT(10)+BIT(11);
    
    #define I2CONSET_I2EN       (BIT(6))  /* I2C Control Set Register */
    #define I2CONSET_AA         (BIT(2))
    #define I2CONSET_SI         (BIT(3))
    #define I2CONSET_STO        (BIT(4))
    #define I2CONSET_STA        (BIT(5))

    #define I2CONCLR_AAC        (BIT(2))  /* I2C Control clear Register */
    #define I2CONCLR_SIC        (BIT(3))
    #define I2CONCLR_STAC       (BIT(5))
    #define I2CONCLR_I2ENC      (BIT(6))
    
    #define I2C2_CLK             CLKPWR_GetPCLK (CLKPWR_PCLKSEL_I2C2)  // 25MHz
    #define I2C_SPD              (100000)
    
    #define I2SCLH_SCLH         (I2C2_CLK / I2C_SPD / 2)
    #define I2SCLL_SCLL         (I2C2_CLK / I2C_SPD - I2SCLH_SCLH)
    
    LPC_I2C2->I2SCLL = I2SCLL_SCLL;
    LPC_I2C2->I2SCLH = I2SCLH_SCLH;
    
    
    LPC_I2C2->I2CONCLR = I2CONCLR_AAC | I2CONCLR_SIC | I2CONCLR_STAC | I2CONCLR_I2ENC;
    LPC_I2C2->I2CONSET = I2CONSET_I2EN;

    // 中断
    NVIC_DisableIRQ(I2C2_IRQn);
    NVIC_SetPriority(I2C2_IRQn, (0x00 << 3) | 0x01);

    // 使能I2C操作
    I2C_Cmd(LPC_I2C2, ENABLE);

//    // Reset操作
    LPC_GPIO1->FIOCLR |= BIT(1);
    imu_delay_ms(500);
    LPC_GPIO1->FIOSET |= BIT(1);
    imu_delay_ms(500);
    
    // 启动对BNO055的配置过程
    imu_read_busy = true;
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
    
//    // Reset操作
//    LPC_GPIO1->FIOCLR |= BIT(1);
//    imu_delay_ms(500);
//    LPC_GPIO1->FIOSET |= BIT(1);
//    imu_delay_ms(500);
    
    imu_read_busy = false;
    // IMU延时一段时间再进入数据读取模式。
    imu_state = ImuState_Delay;
    time_imu_delay_ms = port_get_time_ms() + 100;
    
    // IMU
    dev_imu.ops = &drv_imu_ops;
    
    // register a imu device
    dev_err_t ret = imu_reg(&dev_imu, DEV_IMU, (void *)0);
    M_ASSERT(ret == Dev_OK);
}

// static function -------------------------------------------------------------
static void imu_delay_ms(__IO uint32_t count)   // 100Mhz  
{
    __IO uint32_t n_count = 0;
    
    while(count--) {
        n_count = 10000;
        for (; n_count != 0; n_count--);
    }
}

static bool drv_bno055_read_busy(dev_imu_t * imu)
{
    (void)imu;
    
    NVIC_DisableIRQ(I2C2_IRQn);
    bool imu_is_read_busy = imu_read_busy;
    NVIC_EnableIRQ(I2C2_IRQn);
    
    return imu_is_read_busy;
}

int recv_count = 0;
static void drv_bno055_trig_data(dev_imu_t * imu)
{
    if (port_get_time_ms() < 5000)
        return;
        
    if (imu->super.en == false)
        return;
    
    if (imu_i2c_error == true)
        return;
    
    if (imu_state != ImuState_Work)
        return;

    LPC_I2C2->I2CONCLR = I2CONCLR_SIC;       // clear SI
    LPC_I2C2->I2CONSET |= I2CONSET_STA;      // SET STA GOTO master transmit  
    recv_count = 0;
    
    NVIC_DisableIRQ(I2C2_IRQn);
    imu_read_busy = true;
    NVIC_EnableIRQ(I2C2_IRQn);
}

static void drv_imu_cmd(uint8_t reg_add, uint8_t trsdata)
{
    uint16_t timei;
    
    // Start
    LPC_I2C2->I2CONCLR = I2CONCLR_SIC;          // clear SI
    LPC_I2C2->I2CONSET |= I2CONSET_STA;         // SET STA GOTO master transmit
    timei = 16000;
    while((!(LPC_I2C2->I2CONSET & (I2CONSET_SI))) && (timei --));   //wait SI set
    LPC_I2C2->I2CONCLR = I2CONCLR_STAC;         // clear STA
    if ((LPC_I2C2->I2STAT & 0xF8) != 0x08)
        return;
    
    // 发送器件地址
    LPC_I2C2->I2DAT = (0x29 << 1);              // transmit DATA
    LPC_I2C2->I2CONCLR = I2CONCLR_SIC;          // clear SI
    timei = 16000;
    while((!(LPC_I2C2->I2CONSET&(I2CONSET_SI))) && (timei--));   //wait SI set
    if ((LPC_I2C2->I2STAT & 0xF8) != 0x18)
        return;

    // 发送寄存器地址
    LPC_I2C2->I2DAT = reg_add;                  // transmit DATA
    LPC_I2C2->I2CONCLR = I2CONCLR_SIC;          // clear SI
    timei = 16000;
    while((!(LPC_I2C2->I2CONSET&(I2CONSET_SI))) && (timei--));   //wait SI set
    if ((LPC_I2C2->I2STAT & 0xF8) != 0x28)
        return;

    // 发送寄存器的值
    LPC_I2C2->I2DAT = trsdata;                  // transmit DATA
    LPC_I2C2->I2CONCLR = I2CONCLR_SIC;          // clear SI
    timei = 16000;
    while((!(LPC_I2C2->I2CONSET&(I2CONSET_SI))) && (timei--));   //wait SI set
    if ((LPC_I2C2->I2STAT & 0xF8) != 0x28)
        return;

    // stop
    LPC_I2C2->I2CONCLR = I2CONCLR_STAC;         // clear STA
    LPC_I2C2->I2CONSET |= I2CONSET_STO;         // set STO
    LPC_I2C2->I2CONCLR = I2CONCLR_SIC;          // clear SI
}

// 中断函数 --------------------------------------------------------------------
uint8_t i2c_status = 0;
void I2C2_IRQHandler(void)
{
    uint32_t cclr = I2CONSET_AA | I2CONSET_SI | I2CONSET_STO | I2CONCLR_STAC;
    
//    if ((LPC_I2C2->I2CONSET & I2CONSET_SI) == 0)
//        return;
//    
//    LPC_I2C2->I2CONCLR &= I2CONSET_SI;
    
    i2c_status = LPC_I2C2->I2STAT & 0xF8;
    switch (LPC_I2C2->I2STAT & 0xF8) {
        case 0x08:                                  // Start condition on bus.
            LPC_I2C2->I2CONCLR = I2CONCLR_STAC;
            LPC_I2C2->I2DAT = (0x29 << 1);
            LPC_I2C2->I2CONCLR = I2CONCLR_SIC;
            break;
        
        case 0x18:
            LPC_I2C2->I2DAT = 0x08;                     //  transmit DATA
            LPC_I2C2->I2CONCLR = I2CONCLR_SIC;
            break;
        
        case 0x28:
            LPC_I2C2->I2CONCLR = I2CONCLR_SIC;          // clear SI           
            LPC_I2C2->I2CONSET |= I2CONSET_STA;
            break;
        
        case 0x10:                                      // Repeated start condition.
            LPC_I2C2->I2CONCLR = I2CONCLR_STAC;
            LPC_I2C2->I2DAT = (0x29 << 1) + 1;
            LPC_I2C2->I2CONCLR = I2CONCLR_SIC;
            break;
        
        case 0x40:
            LPC_I2C2->I2CONSET |= I2CONSET_AA;
            LPC_I2C2->I2CONCLR = I2CONCLR_SIC;          // CLAER SI 
            break;
        
        case 0x50:
            *((uint8_t *)&dev_imu.data + recv_count ++) = (uint8_t)LPC_I2C2->I2DAT;
            dev_imu.yaw = dev_imu.data.yaw / 16;
            if (recv_count < 23)
                LPC_I2C2->I2CONSET |= I2CONSET_AA;
            else
                LPC_I2C2->I2CONCLR = I2CONCLR_AAC;
            LPC_I2C2->I2CONCLR = I2CONCLR_SIC;          // CLAER SI 
            break;
        
        case 0x58:
            // stop
            LPC_I2C2->I2CONCLR = I2CONCLR_STAC;         // clear STA
            LPC_I2C2->I2CONSET |= I2CONSET_STO;         // set STO
            LPC_I2C2->I2CONCLR = I2CONCLR_SIC;          // clear SI

            imu_read_busy = false;
        
            EVT_PUB(dev_imu.evt_imu_update);
            break;

        /* NAK Handling */
        case 0x20:                                      // SLA+W sent NAK received.
        case 0x30:                                      // DATA sent NAK received.
        case 0x48:                                      // SLA+R sent NAK received.
        default:
            cclr &= ~I2CONSET_STO;
//            LPC_I2C2->I2CONSET = cclr ^ (I2CONSET_AA | I2CONSET_SI | I2CONSET_STO | I2CONCLR_STAC);
//            LPC_I2C2->I2CONCLR = cclr;
            imu_read_busy = false;
//            if (imu_state == ImuState_Work)
//                imu_state = ImuState_Error;
            break;
    }
}

// IMU状态机轮询 ---------------------------------------------------------------
static uint64_t time_bkp_ms = 0;
void drv_imu_poll(void)
{
    if ((port_get_time_ms() - time_bkp_ms) < 5)
        return;
    
    NVIC_DisableIRQ(I2C2_IRQn);
    uint8_t imu_state_temp = imu_state;
    if (imu_state_temp > ImuState_Delay)
        NVIC_EnableIRQ(I2C2_IRQn);
    
    switch (imu_state_temp) {
        case ImuState_Delay:
            if (port_get_time_ms() > time_imu_delay_ms) {
                imu_state_temp = ImuState_Work;
                NVIC_EnableIRQ(I2C2_IRQn);
            }
            break;
        
        case ImuState_Work:
            break;
        
        case ImuState_Error:
            time_imu_delay_ms = port_get_time_ms() + 100;
            imu_state_temp = ImuState_ErrorDelay;
            break;
        
        case ImuState_ErrorDelay:
            if (port_get_time_ms() > time_imu_delay_ms) {
                imu_state_temp = ImuState_Work;
            }
            break;
            
        case ImuState_Init:
        default:
            break;
    }
    
    NVIC_DisableIRQ(I2C2_IRQn);
    imu_state = imu_state_temp;
    if (imu_state_temp > ImuState_Delay)
        NVIC_EnableIRQ(I2C2_IRQn);
}

