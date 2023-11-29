
#ifndef __DEVF_H__
#define __DEVF_H__

#ifdef __cplusplus
extern "C" {
#endif

// include ---------------------------------------------------------------------
#include "devf_cfg.h"
#include "stdbool.h"
#include "stdint.h"

// data struct definition ------------------------------------------------------
typedef enum dev_err_tag {
    Dev_OK = 0,                                 // 00 There is no error
    Dev_Error,                                  // 01 A generic error happens
    Dev_Err_Timeout,                            // 02 Timed out
    Dev_Err_Full,                               // 03 The resource is full
    Dev_Err_Empty,                              // 04 The resource is empty
    Dev_Err_Busy,                               // 05 Busy
    Dev_Err_WrongArg,                           // 06 Wrong Arguments
    Dev_Err_WrongCmd,                           // 07 Wrong Cmd
    Dev_Err_EndlessLoop,                        // 08 Endless Loop
    Dev_Err_Repeated,                           // 09 Repeated
    Dev_Err_NonExistent,                        // 10 不存在的
    Dev_Err_Disabled,                           // 11 Disabled
    Dev_Err_OpenFail,                           // 12 打开失败
} dev_err_t;

// 设备类型分为两类
// 一种是在一个工程中，仅有一个驱动的设备，如SPI，Serail，CAN，统称为DevType_Null
// 一种是存在多个驱动的设备，需要进行选择的设备，如Motor

// IMU暂定为DevType_Null，因为在绝大部分情况下，使用板载IMU即可满足需要，无需外接IMU设备。
typedef enum dev_type_tag {
    DevType_Normal = 0,
    DevType_UseOneTime,                         // 只能使用一次（物理接口）
} dev_type_t;

typedef void (* device_drv_t)(void);

struct device_tag;
typedef void (* dev_poll_t)(struct device_tag * dev, uint64_t time_system_ms);

typedef struct device_tag {
    struct device_tag * next;
    uint32_t magic;
    const char * name;
    dev_type_t type;
    uint64_t atype_reg;
    bool is_used;
    bool en;
    // poll
    bool en_poll;
    uint64_t time_tgt_ms;
    int time_poll_ms;
    // 通用接口
    dev_err_t (* enable)(struct device_tag * dev, bool enable);
    dev_poll_t poll;
    void (* ask)(struct device_tag * dev);
    // 用户数据
    void * user_data;
} device_t;

// 框架初始化
void devf_init(int atype);
// 清除一个设备的所有数据
void device_clear(device_t * dev);
// 给一个设备注册一个特定的名字
dev_err_t device_reg(device_t * dev, const char * name, dev_type_t type);
// 通过名字查找设备
device_t * device_find(const char * name);
// 使用此设备，用此方法阻止同一个物理接口被两次使用
void device_use(device_t * dev);
// 打开设备
dev_err_t device_en(device_t * dev, bool enable);
// 询问设备
void device_ask(device_t * dev);
// 关闭设备轮询
dev_err_t device_poll_en(device_t * dev, bool enable, int time_poll_ms);
// 为了加快设备轮询的实时性，设备轮询机制将确定为：
// 01 当CPU处于Idle时，对设备进行轮询。
// 02 以获取的实时时间进行轮询，不再以时间片进行轮询。
void devf_poll(uint64_t time_system_ms);
// 使能或者关闭全部设备
void device_all_en(bool enable);
// 将当前设备注册到所有型号（最大支持64个型号）
void device_atype_reg(device_t * dev, int atype);
// 将当前设备注册到所有型号（最大支持64个型号）
void device_alltype_reg(device_t * dev);

// port ------------------------------------------------------------------------
void devf_port_evt_pub(int evt_id);                 // 发送事件
uint64_t devf_port_get_time_ms(void);               // 获取时间
void devf_port_irq_disable(void);                   // 关中断
void devf_port_irq_enable(void);                    // 开中断
void devf_port_regitry_init(void);                  // 设备注册机制

// assert ----------------------------------------------------------------------
#define DEV_ASSERT_MEOW
#ifdef DEV_ASSERT_MEOW

#else
    #include "assert.h"
    #define Dev_Assert assert
#endif

#ifdef __cplusplus    
}
#endif

#endif
