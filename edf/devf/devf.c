
// include ---------------------------------------------------------------------
#include "devf.h"
#include "string.h"
#include "mlog/mlog.h"
#include "stdio.h"

M_TAG("Devf")

// define ----------------------------------------------------------------------
#define DEV_MAGIC                       0x37A563C8

// list ------------------------------------------------------------------------
device_t * dev_list = (device_t *)0;
static int a_type = 0;

// static function -------------------------------------------------------------
static dev_err_t enable_null(device_t * dev, bool enable);
static void poll_null(device_t * dev, uint64_t time_system_ms);
static void ask_null(device_t * dev);

// api -------------------------------------------------------------------------
void devf_init(int atype)
{
    dev_list = (device_t *)0;
    a_type = atype;
    
}

// 清除一个设备的所有数据
void device_clear(device_t * dev)
{
    dev->name = "?";
    dev->atype_reg = 0;
    dev->type = DevType_Normal;
    dev->next = (device_t *)0;
    dev->enable = false;
    dev->en_poll = false;
    dev->is_used = false;
    dev->time_tgt_ms = 0;
    dev->time_poll_ms = 0;
    dev->magic = DEV_MAGIC;
    // interface
    dev->enable = enable_null;
    dev->poll = poll_null;
    dev->ask = ask_null;

    dev->user_data = (void *)0;
}

// 给一个设备注册一个特定的名字
dev_err_t device_reg(device_t * dev, const char * name, dev_type_t type)
{
    dev->name = name;
    dev->next = dev_list;
    dev->type = type;
    dev_list = dev;
    dev->magic = DEV_MAGIC;
    
    return Dev_OK;
}

// 通过名字查找设备
device_t * device_find(const char * name)
{
    for (device_t * dev = dev_list; dev != (void *)0; dev = dev->next) {
        if (strncmp(dev->name, name, DEV_NAME_MAX) == 0)
            return dev;
    }

    return (device_t *)0;
}

// 使用此设备，用此方法阻止同一个物理接口被两次使用
void device_use(device_t * dev)
{
    if (dev->type == DevType_UseOneTime) {
        M_ASSERT_NAME(dev->is_used == false, dev->name);
    }
    
    dev->is_used = true;
}

// 打开设备
dev_err_t device_en(device_t * dev, bool enable)
{
    dev->en = enable;
    dev->enable(dev, enable);
    
    return Dev_OK;
}

// 关闭设备轮询
dev_err_t device_poll_en(device_t * dev, bool enable, int time_poll_ms)
{
    dev->en_poll = enable;
    dev->time_poll_ms = time_poll_ms;
    if (dev->en_poll == true)
        dev->time_tgt_ms = devf_port_get_time_ms() + dev->time_poll_ms;
    
    return Dev_OK;
}

// 设备轮询
void devf_poll(uint64_t time_system_ms)
{
    for (device_t * dev = dev_list; dev != (void *)0; dev = dev->next) {
        M_ASSERT(dev->magic == DEV_MAGIC);
        if ((dev->atype_reg & (1 << a_type)) == 0 || dev->en == false || dev->en_poll == false)
            continue;
        dev->poll(dev, time_system_ms);
    }
}

void device_ask(device_t * dev)
{
    if ((dev->atype_reg & (1 << a_type)) == 0 || dev->en == false)
        return;
    dev->ask(dev);
}

// 使能或者关闭全部设备
void device_all_en(bool enable)
{
    for (device_t * dev = dev_list; dev != (void *)0; dev = dev->next)
        dev->en = enable;
}

void device_atype_reg(device_t * dev, int atype)
{
    dev->atype_reg |= (1 << atype);
}

void device_alltype_reg(device_t * dev)
{
    dev->atype_reg = 0xffffffffffffffff;
}

// static function -------------------------------------------------------------
static dev_err_t enable_null(device_t * dev, bool enable)
{
    (void)dev;
    (void)enable;

    return Dev_OK;
}

static void poll_null(device_t * dev, uint64_t time_system_ms)
{
    (void)dev;
    (void)time_system_ms;
}

static void ask_null(device_t * dev)
{
    (void)dev;
}
