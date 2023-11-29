
#ifndef __DEV_IMU_H__
#define __DEV_IMU_H__

// include ---------------------------------------------------------------------
#include "devf.h"

#ifdef __cplusplus
extern "C" {
#endif

// data struct definition ------------------------------------------------------
enum {
    Axis_X = 0, Axis_Y, Axis_Z, Axis_Max
};

#pragma pack(1)
typedef struct dev_imu_data_tag {
    int16_t acc[Axis_Max];
    int16_t msg[Axis_Max];
    int16_t gyr[Axis_Max];
    int16_t yaw;
    int16_t roll;
    int16_t pitch;
} dev_imu_data_t;
#pragma pack()

typedef struct dev_imu_tag {
    device_t super;
    const struct dev_imu_ops * ops;
    // evt
    int evt_imu_update;
    // data
    int yaw;
    dev_imu_data_t data;
    uint8_t rsv[32];
} dev_imu_t;

struct dev_imu_ops {
    bool (* read_busy)(dev_imu_t * imu);
    void (* trig_data)(dev_imu_t * imu);
};

// api -------------------------------------------------------------------------
// BSP层
dev_err_t imu_reg(dev_imu_t * imu, const char * name, void * user_data);
void imu_isr_recv(dev_imu_t * imu, int yaw);
// 应用层
void imu_set_evt(device_t * dev, int evt_imu_update);
void imu_get_data(device_t * dev, dev_imu_data_t * data);
float imu_get_yaw(device_t * dev);

#ifdef __cplusplus
}
#endif

#endif
