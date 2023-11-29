
#ifndef __DEV_NULL_H__
#define __DEV_NULL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* include ------------------------------------------------------------------ */
#include "devf.h"

/* data struct definition --------------------------------------------------- */
typedef struct dev_null_tag {
    device_t super;
} dev_null_t;

/* API definition ----------------------------------------------------------- */
dev_err_t dev_null_reg(dev_null_t * null, const char * name, void * user_data);

#ifdef __cplusplus
}
#endif

#endif
