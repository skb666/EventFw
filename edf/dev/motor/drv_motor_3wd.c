
// -----------------------------------------------------------------------------
#include "drv_motor.h"
#include "mdebug/m_assert.h"
#include "dev_motor.h"
#include "mdb/mdb.h"
#include "dev_def.h"
#include "can/dev_can.h"

M_MODULE_NAME("Drv-Motor-3wd")

// 设备指针 ---------------------------------------------------------------------
enum {
    Motor_Null = 0,
    Motor_L, Motor_R, Motor_Cut1, Motor_Cut2,
    Motor_Max
};
dev_motor_t dev_motor[Motor_Max];

// 驱动数据结构 -----------------------------------------------------------------
static device_t * dev_can;

typedef struct drv_motor_data {
    int can_id;
    const char * name;
    int speed;
} drv_motor_data_t;

drv_motor_data_t drv_data[Motor_Max] = {
    { 0, "Motor_Null", 0 },
    { 1, DEV_MOTOR_L, 0 },
    { 2, DEV_MOTOR_R, 0 },
    { 3, DEV_MOTOR_CUT1, 0 },
    { 4, DEV_MOTOR_CUT2, 0 },
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
void drv_motor_3wd_init(void)
{
    // 获取CAN设备
    dev_can = device_find(DEV_CAN);
    M_ASSERT(dev_can != (device_t *)0);

    // ops
    dev_err_t ret;
    for (int i = Motor_L; i < Motor_Max; i ++) {
        dev_motor[i].ops = &motor_can_ops;
        ret = dev_motor_reg(&dev_motor[i], drv_data[i].name, (void *)&drv_data[i]);
        M_ASSERT(ret == Dev_OK);
    }
}

// static function -------------------------------------------------------------
dev_can_msg_t dev_can_msg;

static void drv_motor_get_info(dev_motor_t * motor, dev_motor_info_t * info)
{
    drv_motor_data_t * drv_data = (drv_motor_data_t *)motor->super.user_data; 
    dev_can_msg.id = drv_data->can_id;
    dev_can_msg.ide = true;
    dev_can_msg.rtr = false;
    dev_can_msg.len = 8;
    for (int i = 0; i < 8; i ++)
        dev_can_msg.data[i] = 0;
    dev_can_msg.data[0] = 0x05;

    dev_can_send(dev_can, &dev_can_msg);
}

static dev_err_t drv_motor_enable(dev_motor_t * motor, bool enable)
{
    (void)motor;
    (void)enable;

    return Dev_OK;
}

static void drv_motor_run(dev_motor_t * motor, int speed)
{
    drv_motor_data_t * drv_data = (drv_motor_data_t *)motor->super.user_data;
    if (speed == drv_data->speed)
        return;
    
    drv_data->speed = speed;
    
    dev_can_msg.id = drv_data->can_id;
    dev_can_msg.ide = true;
    dev_can_msg.rtr = false;
    dev_can_msg.len = 8;
    for (int i = 0; i < 8; i ++)
        dev_can_msg.data[i] = 0;
    dev_can_msg.data[0] = 0x04;
    dev_can_msg.data[1] = speed >= 0 ? 0 : 1;
    dev_can_msg.data[2] = (speed >= 0 ? speed : -speed) / 256;
    dev_can_msg.data[3] = (speed >= 0 ? speed : -speed) % 256;

    dev_can_send(dev_can, &dev_can_msg);
}

static void drv_motor_stop(dev_motor_t * motor)
{
    drv_motor_data_t * drv_data = (drv_motor_data_t *)motor->super.user_data; 
    
    if (0 == drv_data->speed)
        return;
    drv_data->speed = 0;
    
    dev_can_msg.id = drv_data->can_id;
    dev_can_msg.ide = true;
    dev_can_msg.rtr = false;
    dev_can_msg.len = 8;
    for (int i = 0; i < 8; i ++)
        dev_can_msg.data[i] = 0;
    dev_can_msg.data[0] = 0x01;

    dev_can_send(dev_can, &dev_can_msg);
}

static void drv_motor_break(dev_motor_t * motor)
{
    drv_motor_data_t * drv_data = (drv_motor_data_t *)motor->super.user_data; 
    
    if (0 == drv_data->speed)
        return;
    drv_data->speed = 0;
    
    dev_can_msg.id = drv_data->can_id;
    dev_can_msg.ide = true;
    dev_can_msg.rtr = false;
    dev_can_msg.len = 8;
    for (int i = 0; i < 8; i ++)
        dev_can_msg.data[i] = 0;
    dev_can_msg.data[0] = 0x02;

    dev_can_send(dev_can, &dev_can_msg);
}

// poll ------------------------------------------------------------------------
static const char * speed_key[7] = {
    "?", "MotorP_Speed", "Helm_Speed", "MotorL_Speed", "MotorR_Speed", "Cut1_Speed", "Cut2_Speed"
};
static const char * current_key[7] = {
    "?", "MotorP_Current", "Helm_Current", "MotorL_Current", "MotorR_Current", "Cut1_Current", "Cut2_Current"
};
// TODO 给驱动管理增加poll机制。
void drv_motor_poll(void)
{
    dev_can_msg_t can_msg;
    int ret = Dev_OK;
    while (1) {
        ret = dev_can_recv(dev_can, &can_msg);
        if (ret == Queue_Empty)
            return;

        if (can_msg.id != 0)                    // 向主机反馈的消息ID都为0
            continue;
        if (can_msg.ide == false)               // 都是扩展帧
            continue;
        if (can_msg.rtr == true)                // 不是远程帧
            continue;
        if (can_msg.data[0] != 0xA1)            // Cmd ID为0xA1
            continue;
                                                // 电机ID为1 - Motor_Max（不含）
        if (can_msg.data[1] == Motor_Null || can_msg.data[1] >= Motor_Max)
            continue;

        uint8_t id = can_msg.data[1];
        dev_motor[id].info.speed = (int16_t)(can_msg.data[4] * 256 + can_msg.data[5]);
        dev_motor[id].info.current = can_msg.data[2] * 256 + can_msg.data[3];
        
        int mdb_ret;
        int speed = dev_motor[id].info.speed >= 0 ? dev_motor[id].info.speed : -dev_motor[id].info.speed;
        mdb_ret = mdb_set_value_int(speed_key[id], speed);
        M_ASSERT(mdb_ret == MdbRet_OK);
        int current = dev_motor[id].info.current >= 0 ? dev_motor[id].info.current : -dev_motor[id].info.current;
        mdb_ret = mdb_set_value_int(current_key[id], current);
        M_ASSERT(mdb_ret == MdbRet_OK);
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
