
#ifdef __cplusplus
extern "C" {
#endif

// include ---------------------------------------------------------------------
#include "dev_motor.h"
#include "mlog/mlog.h"

M_TAG("Dev-Motor")

// private function ------------------------------------------------------------
static dev_err_t motor_enable(device_t * dev, bool enable);
static void motor_poll(device_t * dev, uint64_t time_system_ms);

// API -------------------------------------------------------------------------
dev_err_t dev_motor_reg(dev_motor_t * motor, const char * name, void * user_data)
{
    device_t * dev = &(motor->super);
    device_clear(dev);

    // interface
    dev->enable = motor_enable;
    dev->poll = motor_poll;
    // user data
    dev->user_data = user_data;
    // data
    motor->info.speed = 0;
    motor->info.position = 0;

    // register a character device.
    return device_reg(dev, name, DevType_UseOneTime);
}

int motor_get_speed(device_t * dev)
{
    return ((dev_motor_t *)dev)->info.speed;
}

void motor_stop(device_t * dev)
{
    ((dev_motor_t *)dev)->ops->stop((dev_motor_t *)dev);
}

// 电磁抱死或者机械抱死
void motor_break(device_t * dev)
{
    ((dev_motor_t *)dev)->ops->mbreak((dev_motor_t *)dev);
}

void motor_run(device_t * dev, int speed)
{
    ((dev_motor_t *)dev)->ops->run((dev_motor_t *)dev, speed);
}

// static function -------------------------------------------------------------
static dev_err_t motor_enable(device_t * dev, bool enable)
{
    dev->en = enable;
    return ((dev_motor_t *)dev)->ops->enable((dev_motor_t *)dev, enable);
}

static void motor_poll(device_t * dev, uint64_t time_system_ms)
{
    if (time_system_ms < dev->time_tgt_ms)
        return;
    dev->time_tgt_ms += dev->time_poll_ms;
    M_ASSERT(dev->time_poll_ms >= 10 && dev->time_poll_ms <= 100);

    // 周期性获取电机状态信息
    dev_motor_t * motor = (dev_motor_t *)dev;
    motor->ops->get_info(motor, &motor->info);
}

#ifdef __cplusplus
}
#endif
