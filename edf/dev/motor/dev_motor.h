
#ifndef __DEV_MOTOR_H__
#define __DEV_MOTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

/* include ------------------------------------------------------------------ */
#include "devf.h"

/* data struct definition --------------------------------------------------- */
enum {
    DevMotorDrv_I = 0,
    DevMotorDrv_II,
    DevMotorDrv_CanOpen,
    DevMotorDrv_Modbus,

    DevMotorDrv_Max
};

typedef struct dev_motor_info_tag {
    int speed;
    int position;
    int current;
} dev_motor_info_t;

struct dev_motor_ops;
typedef struct dev_motor_tag {
    device_t super;
    dev_motor_info_t info;

    const struct dev_motor_ops * ops;
} dev_motor_t;

struct dev_motor_ops {
    void (* get_info)(dev_motor_t * motor, dev_motor_info_t * info);
    dev_err_t (* enable)(dev_motor_t * motor, bool enable);
    void (* run)(dev_motor_t * motor, int speed);
    void (* stop)(dev_motor_t * motor);
    void (* mbreak)(dev_motor_t * motor);
};

/* API definition ----------------------------------------------------------- */
// BSP层
dev_err_t dev_motor_reg(dev_motor_t * motor, const char * name, void * user_data);
// 应用层
int motor_get_speed(device_t * dev);
void motor_stop(device_t * dev);
// 电磁抱死或者机械抱死
void motor_break(device_t * dev);
void motor_run(device_t * dev, int speed);

#ifdef __cplusplus
}
#endif

#endif
