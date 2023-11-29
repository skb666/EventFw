#ifndef __DRV_IMU_H__
#define __DRV_IMU_H__

// API -------------------------------------------------------------------------
// BNO055
void drv_imu_bno055_init(void);
// memsplus
void drv_imu_memsplus_init(void);
// BNO055 1768
void drv_imu_1768_init(void);
// poll
void drv_imu_poll(void);
// trig
void drv_imu_trig_enable(void);

#endif
