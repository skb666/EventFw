
// include ---------------------------------------------------------------------
#include "dev_imu.h"
#include "mlog/mlog.h"
#include "string.h"
#include "meow.h"
#include "mdb/mdb.h"

M_TAG("Dev-Imu")

// private function ------------------------------------------------------------
static dev_err_t dev_imu_enable(device_t * dev, bool enable);
static void dev_imu_poll(device_t * dev, uint64_t time_system_ms);

// api -------------------------------------------------------------------------
// BSP层
dev_err_t imu_reg(dev_imu_t * imu, const char * name, void * user_data)
{
    device_t * dev = &(imu->super);
    device_clear(dev);

    // interface
    dev->enable = dev_imu_enable;
    dev->poll = dev_imu_poll;
    // user data
    dev->user_data = user_data;
    // data
    imu->yaw = 0;
    imu->evt_imu_update = Evt_Null;

    // register a character device
    return device_reg(dev, name, DevType_UseOneTime);
}

void imu_isr_recv(dev_imu_t * imu, int yaw)
{
    imu->yaw = yaw;
    devf_port_evt_pub(imu->evt_imu_update);
}

void imu_set_evt(device_t * dev, int evt_imu_update)
{
    devf_port_irq_disable();
    ((dev_imu_t *)dev)->evt_imu_update = evt_imu_update;
    devf_port_irq_enable();
}

void imu_get_data(device_t * dev, dev_imu_data_t * data)
{
    devf_port_irq_disable();
    memcpy(data, &((dev_imu_t *)dev)->data, sizeof(dev_imu_data_t));
    devf_port_irq_enable();
}

float imu_get_yaw(device_t * dev)
{
    dev_imu_t * imu = (dev_imu_t *)dev;
    
    devf_port_irq_disable();
    float yaw = (float)imu->data.yaw / 16.0;
    devf_port_irq_enable();
    
    return yaw;
}

// static function -------------------------------------------------------------
static dev_err_t dev_imu_enable(device_t * dev, bool enable)
{
    dev->en = enable;
    return Dev_OK;
}

static void dev_imu_poll(device_t * dev, uint64_t time_system_ms)
{
    if (time_system_ms < dev->time_tgt_ms)
        return;
    dev->time_tgt_ms = time_system_ms + dev->time_poll_ms;

    // 周期性获取电机状态信息
    dev_imu_t * imu = (dev_imu_t *)dev;
    if (imu->ops->read_busy(imu) == false)
        imu->ops->trig_data(imu);
}
