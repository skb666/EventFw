
#ifdef __cplusplus
extern "C" {
#endif

#include "dev_null.h"

dev_err_t dev_null_reg(dev_null_t * null, const char * name, void * user_data)
{
    device_t * dev = &(null->super);
    device_clear(dev);
    dev->user_data = user_data;

    // register a character device.
    return device_reg(dev, name, DevType_UseOneTime);
}

#ifdef __cplusplus    
}
#endif
