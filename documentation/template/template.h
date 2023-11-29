/*
 * eLab Project
 * Copyright (c) 2023, EventOS Team, <event-os@outlook.com>
 */

#ifndef ELAB_DEVICE_H
#define ELAB_DEVICE_H

/* includes ----------------------------------------------------------------- */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* public config ------------------------------------------------------------ */
#define ELAB_DEV_NUM_MAX                (64)
#define ELAB_DEV_PALTFORM               ELAB_PALTFORM_RTOS

/* public define ------------------------------------------------------------ */
enum elab_device_type
{
    ELAB_DEVICE_NULL = 0,

    ELAB_DEVICE_PIN,
    ELAB_DEVICE_PWM,
    ELAB_DEVICE_ADC,
    ELAB_DEVICE_DAC,
    ELAB_DEVICE_UART,
    ELAB_DEVICE_I2C_BUS,
    ELAB_DEVICE_I2C,
    ELAB_DEVICE_SPI_BUS,
    ELAB_DEVICE_SPI,
    ELAB_DEVICE_CAN,
    ELAB_DEVICE_WATCHDOG,
    ELAB_DEVICE_RTC,
    ELAB_DEVICE_UNKNOWN,

    ELAB_DEVICE_NORMAL_MAX,
};

enum elab_device_layer
{
    ELAB_DEV_LAYER_NORAML = 0,                  /* On-chip device mode */
    ELAB_DEV_LAYER_USER,                        /* User defined device mode */
};

enum elab_device_boot_level
{
    ELAB_DEV_BOOT_L0 = 0,                       /* The lowest */
    ELAB_DEV_BOOT_L1,
    ELAB_DEV_BOOT_L2,
    ELAB_DEV_BOOT_L3,
    ELAB_DEV_BOOT_L4,
    ELAB_DEV_BOOT_L5,
    ELAB_DEV_BOOT_L6,
    ELAB_DEV_BOOT_L7,
};

/* public typedef ----------------------------------------------------------- */
typedef struct elab_device_attr
{
    const char *name;
    bool sole;
    uint8_t type;
} elab_device_attr_t;

typedef struct elab_device
{
    elab_device_attr_t attr;

    uint8_t enable_count;
#if (ELAB_DEV_PALTFORM == ELAB_PALTFORM_RTOS)
    uint8_t lock_count;
    osMutexId_t mutex;
#endif
    osThreadId_t thread_test;

    /* common device interface */
    const struct elab_dev_ops *ops;
    void *user_data;
} elab_device_t;

typedef struct elab_dev_ops
{
    elab_err_t (* enable)(elab_device_t *me, bool status);
    int32_t (* read)(elab_device_t *me, uint32_t pos, void *buffer, uint32_t size);
    int32_t (* write)(elab_device_t *me, uint32_t pos, const void *buffer, uint32_t size);
#if (ELAB_DEV_PALTFORM == ELAB_PALTFORM_POLL)
    void (* poll)(elab_device_t *me);
    elab_err_t (* isr_enable)(elab_device_t *me, bool status);
#endif
} elab_dev_ops_t;

#define ELAB_DEVICE_CAST(_dev)      ((elab_device_t *)_dev)

/* public functions --------------------------------------------------------- */
/**
 * @brief This function registers a device with its atttibutes.
 * @param me    Device handle
 * @param attr  the device driver's atttibute.
 * @return None.
 */
void elab_device_register(elab_device_t *me, elab_device_attr_t *attr);

/**
 * @brief Open the given device.
 * @param _dev  Device handle.
 * @return None.
 */
#define elab_device_open(_dev)              __device_enable(ELAB_DEVICE_CAST(_dev), true)

/**
 * @brief Close the given device.
 * @param _dev  Device handle.
 * @return None.
 */
#define elab_device_close(_dev)             __device_enable(ELAB_DEVICE_CAST(_dev), false)

/* private function --------------------------------------------------------- */
void __device_mutex_lock(elab_device_t *me, bool status);
elab_err_t __device_enable(elab_device_t *me, bool status);

/* private define ----------------------------------------------------------- */
/* eLab platform */
#define ELAB_PALTFORM_RTOS                  (0)
#define ELAB_PALTFORM_POLL                  (1)
#define ELAB_PALTFORM_BASIC_OS              (2)

#ifdef __cplusplus
}
#endif

#endif /* ELAB_DEVICE_H */

/* ----------------------------- end of file -------------------------------- */
