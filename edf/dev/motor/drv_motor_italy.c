
// -----------------------------------------------------------------------------
#include "drv_motor.h"
#include "mlog/mlog.h"
#include "dev_motor.h"
#include "mdb/mdb.h"
#include "dev_def.h"
#include "can/dev_can.h"
#include "dal_def.h"
#include "isotp.h"
#include "canModuleIdUserDefine.h"
#include "port_isotp.h"

M_TAG("Drv-Motor-Italy")

// 设备指针 ---------------------------------------------------------------------
enum {
    Motor_Null = 0,
    Motor_L, Motor_R, Motor_Cut1, Motor_Cut2,
    Motor_Max
};
dev_motor_t dev_motor[Motor_Max];

// 驱动数据结构 -----------------------------------------------------------------
static device_t * dev_can;

// 电机驱动状态机
enum {
    DrvMotorState_Delay = 0,
    DrvMotorState_Info,
    DrvMotorState_Enable,
    DrvMotorState_Run,

    DrvMotorState_Max
};

// 电机模式
enum {
    DrvMotorMode_Null = 0,
    DrvMotorMode_WheelSensorless,
    DrvMotorMode_BladeSensorless,
    DrvMotorMode_WheelHallSensor,

    DrvMotorMode_Max
};

typedef struct drv_motor_cmd_tag {
    int16_t speed;
    uint8_t address;
    uint8_t torque;
    uint8_t mode;
} drv_motor_cmd_t;

typedef struct drv_motor_data {
    const char * name;
    int state;
    int enable_count;
    bool enable;
    uint64_t time_delay_ms;
    drv_motor_cmd_t cmd;
} drv_motor_data_t;

drv_motor_data_t drv_motor_data[Motor_Max] = {
    {   "Motor_Null",
        DrvMotorState_Delay, 0, false, 0,
        { 0, 0, 0, DrvMotorMode_WheelHallSensor },
    },
    // 左电机
    {   DEV_MOTOR_L,
        DrvMotorState_Delay, 0, false, 0,
        { 0, 0, 0, DrvMotorMode_WheelHallSensor },
    },
    // 右电机
    {   DEV_MOTOR_R,
        DrvMotorState_Delay, 0, false, 0,
        { 0, 1, 0, DrvMotorMode_WheelHallSensor },
    },
    // 割草电机1
    {   DEV_MOTOR_CUT1,
        DrvMotorState_Delay, 0, false, 0,
        { 0, 0, 0, DrvMotorMode_WheelHallSensor },
    },
    // 割草电机2
    {   DEV_MOTOR_CUT2,
        DrvMotorState_Delay, 0, false, 0,
        { 0, 0, 0, DrvMotorMode_WheelHallSensor },
    },
};



// ops -------------------------------------------------------------------------
static void drv_motor_get_info(dev_motor_t * motor, dev_motor_info_t * info);
static dev_err_t drv_motor_enable(dev_motor_t * motor, bool enable);
static void drv_motor_run(dev_motor_t * motor, int speed);
static void drv_motor_stop(dev_motor_t * motor);
static void drv_motor_break(dev_motor_t * motor);

static const struct dev_motor_ops motor_can_ops = {
    drv_motor_get_info,
    drv_motor_enable,
    drv_motor_run,
    drv_motor_stop,
    drv_motor_break,
};

// api -------------------------------------------------------------------------
void drv_motor_init(void)
{
    // 获取CAN设备
    dev_can = device_find(DEV_CAN);
    M_ASSERT(dev_can != (device_t *)0);

    // ops
    dev_err_t ret;
    uint64_t time_crt_ms = devf_port_get_time_ms();
    for (int i = Motor_L; i <= Motor_R; i ++) {
        dev_motor[i].ops = &motor_can_ops;
        drv_motor_data[i].time_delay_ms = time_crt_ms + 3000;
        drv_motor_data[i].enable = true;
        ret = dev_motor_reg(&dev_motor[i], drv_motor_data[i].name, (void *)&drv_motor_data[i]);
        M_ASSERT(ret == Dev_OK);
    }
}

// static function -------------------------------------------------------------
dev_can_msg_t dev_can_msg;

static void drv_motor_get_info(dev_motor_t * motor, dev_motor_info_t * info)
{
    (void)motor;
    (void)info;
}

static dev_err_t drv_motor_enable(dev_motor_t * motor, bool enable)
{
    drv_motor_data_t * drv_data = (drv_motor_data_t *)motor->super.user_data;
    drv_data->enable = enable;

    return Dev_OK;
}

static void drv_motor_run(dev_motor_t * motor, int speed)
{
    drv_motor_data_t * drv_data = (drv_motor_data_t *)motor->super.user_data;
    if (drv_data->state != DrvMotorState_Run)
        return;
    
    drv_data->cmd.speed = speed;
    drv_data->cmd.torque = 100;
}

static void drv_motor_stop(dev_motor_t * motor)
{
    drv_motor_run(motor, 0);
}

static void drv_motor_break(dev_motor_t * motor)
{
    drv_motor_run(motor, 0);
}

// poll ------------------------------------------------------------------------
// TODO 给驱动管理增加poll机制。
uint64_t time_bkp_ms = 0;
dev_can_msg_t can_msg;

void drv_motor_poll(void)
{
    int ret;
    
    // 接收的数据解析
    while (1) {
        ret = dev_can_recv(dev_can, &can_msg);
        if (ret == Dev_Err_Empty)
            break;

        // 向主机反馈的消息ID都为0x465或者0x467
        if (can_msg.id != 0x467 && can_msg.id != 0x465)
            continue;
        if (can_msg.ide == true)                // 都是标准帧
            continue;
        if (can_msg.rtr == true)                // 不是远程帧
            continue;
        
        if (can_msg.id == 0x467) {      // left
            uint16_t spd_left = can_msg.data[3];
            spd_left = (spd_left << 8) | can_msg.data[2];
            
            dev_motor[Motor_L].info.speed = (int16_t)spd_left;
            dev_motor[Motor_L].info.current = can_msg.data[7];
            
            // 写入Dal中的缓冲区里
            if (dal.status.motor.first_l == true) {
                for (int i = 0; i < 10; i ++)
                    dal.status.motor.speed_l_buff[i] =
                        (float)dev_motor[Motor_L].info.speed *
                        (float)dal.para.wheel_length / 60.0;
                dal.status.motor.first_l = false;
            }
            else {
                dal.status.motor.speed_l_buff[dal.status.motor.index_l ++] =
                    (float)dev_motor[Motor_L].info.speed *
                    (float)dal.para.wheel_length / 60.0;
                if (dal.status.motor.index_l >= 10)
                    dal.status.motor.index_l = 0;
            }
            float sum = 0;
            for (int i = 0; i < 10; i ++)
                sum += dal.status.motor.speed_l_buff[i];
            dal.status.motor.speed_l = sum / 10.0;
        }
        
        if (can_msg.id == 0x465) {      // right
            uint16_t spd_right = can_msg.data[3];
            spd_right = (spd_right << 8) | can_msg.data[2];
            
            dev_motor[Motor_R].info.speed = (int16_t)spd_right;
            dev_motor[Motor_R].info.current = can_msg.data[7];
            
            // 写入Dal中的缓冲区里
            if (dal.status.motor.first_r == true) {
                for (int i = 0; i < 10; i ++)
                    dal.status.motor.speed_r_buff[i] =
                        (float)dev_motor[Motor_R].info.speed *
                        (float)dal.para.wheel_length / 60.0;
                dal.status.motor.first_r = false;
            }
            else {
                dal.status.motor.speed_r_buff[dal.status.motor.index_r ++] =
                    (float)dev_motor[Motor_R].info.speed *
                    (float)dal.para.wheel_length / 60.0;
                if (dal.status.motor.index_r >= 10)
                    dal.status.motor.index_r = 0;
            }
            float sum = 0;
            for (int i = 0; i < 10; i ++)
                sum += dal.status.motor.speed_r_buff[i];
            dal.status.motor.speed_r = sum / 10.0;
        }
    }

    // TODO 可以尝试修改为每50ms或者在Run状态下，速度扭矩发生变化时，立即发送数据。
    // 数据每50ms发送一次
    uint64_t time_crt_ms = devf_port_get_time_ms();
    if ((time_crt_ms - time_bkp_ms) < 10) {
        return;
    }
    time_bkp_ms = time_crt_ms;

    // 数据发送
    for (int i = Motor_L; i <= Motor_R; i ++) {
        switch (drv_motor_data[i].state) {
            case DrvMotorState_Delay:
                if (time_crt_ms >= drv_motor_data[i].time_delay_ms) {
                    drv_motor_data[i].time_delay_ms = 0;
                    drv_motor_data[i].state = DrvMotorState_Info;
                }
                break;

            case DrvMotorState_Info:
                if (drv_motor_data[i].enable == true) {
                    dev_can_msg_t dev_can_msg;
                    drv_motor_data[i].cmd.speed = 0;
                    
                    dev_can_msg.id = 0x660;
                    dev_can_msg.ide = false;
                    dev_can_msg.rtr = false;
                    dev_can_msg.len = 6;
                    for (int i = 0; i < 8; i ++)
                        dev_can_msg.data[i] = 0;
                    dev_can_msg.data[4] = drv_motor_data[i].cmd.address;
                    dev_can_msg.data[5] = drv_motor_data[i].cmd.mode;

                    dev_can_send(dev_can, &dev_can_msg);

                    drv_motor_data[i].state = DrvMotorState_Enable;
                }
                break;

            case DrvMotorState_Enable: {
                dev_can_msg_t dev_can_msg;
                uint8_t clearencoder = 0;
                drv_motor_data[i].cmd.speed = 0;
                drv_motor_data[i].cmd.torque = 100;
                
                dev_can_msg.id = 0x460;
                dev_can_msg.ide = false;
                dev_can_msg.rtr = false;
                dev_can_msg.len = 4;
                for (int i = 0; i < 8; i ++)
                    dev_can_msg.data[i] = 0;
                dev_can_msg.data[0] = ((drv_motor_data[i].cmd.torque & 0x0F) << 4) |
                                       (drv_motor_data[i].cmd.address & 0x0F); 
                dev_can_msg.data[1] = (((clearencoder & 0x01) << 3) |
                                       ((drv_motor_data[i].cmd.torque & 0x70) >> 4)) & 0x0F;
                dev_can_msg.data[2] = (uint16_t)drv_motor_data[i].cmd.speed;
                dev_can_msg.data[3] = (uint16_t)drv_motor_data[i].cmd.speed >> 8;

                dev_can_send(dev_can, &dev_can_msg);

                drv_motor_data[i].state = DrvMotorState_Enable;
                drv_motor_data[i].enable_count ++;
                if (drv_motor_data[i].enable_count >= 3) {
                    drv_motor_data[i].state = DrvMotorState_Run;
                }
                break;
            }

            case DrvMotorState_Run: {
                dev_can_msg_t dev_can_msg;
                uint8_t clearencoder = 0;

                
                dev_can_msg.id = 0x460;
                dev_can_msg.ide = false;
                dev_can_msg.rtr = false;
                dev_can_msg.len = 4;
                for (int i = 0; i < 8; i ++)
                    dev_can_msg.data[i] = 0;
                dev_can_msg.data[0] = ((drv_motor_data[i].cmd.torque & 0x0F) << 4) |
                                       (drv_motor_data[i].cmd.address & 0x0F); 
                dev_can_msg.data[1] = (((clearencoder & 0x01) << 3) |
                                       ((drv_motor_data[i].cmd.torque & 0x70) >> 4)) & 0x0F;
                dev_can_msg.data[2] = (uint16_t)drv_motor_data[i].cmd.speed;
                dev_can_msg.data[3] = (uint16_t)drv_motor_data[i].cmd.speed >> 8;

                dev_can_send(dev_can, &dev_can_msg);
                break;
            }

            case DrvMotorState_Max:
            default:
                break;
        }
    }
}

// 协议说明 --------------------------------------------------------------------
/*
-------------------------------------------------------------------------------
29位扩展帧，非远程帧，速度125K
-------------------------------------------------------------------------------
指令描述    长度    Byte0   1       2       3       4       5       6       7
停止：      8       0x01    0x00    0x00    0x00    0x00    0x00    0x00    0x00
刹车：      8       0x02    0x00    0x00    0x00    0x00    0x00    0x00    0x00
运行：      8       0x03    0/1     速度    速度    0x00    0x00    0x00    0x00(转/min)
运行：      8       0x04    0/1     速度    速度    0x00    0x00    0x00    0x00(PWM)

状态反馈    8       0xA1    ID      电流    电流    速度    速度    ErrID    0x00

--------------------------------------------------------------------------------
ErrID   0x01 启动时相电流过流 0x02 启动出错 0x03 运行出错

--------------------------------------------------------------------------------
各电机ID分配
左前    1
右前    2
左后    3
右后    4
--------------------------------------------------------------------------------
*/
