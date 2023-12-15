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
#include "../common/elab_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* public config ------------------------------------------------------------ */
#define ELAB_DEV_NUM_MAX                (64)
#define ELAB_DEV_PALTFORM               3 // ELAB_PALTFORM_POLL

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

    /* common device interface */
    const struct elab_dev_ops *ops;
    uint8_t test_mode;
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
 * @brief This function finds a device driver by specified name.
 * @param name  Device name.
 * @param attr  the device driver's atttibute.
 * @return Device handle. If not found, return NULL.
 */
elab_device_t *elab_device_find(const char *name);

/**
 * This function check the given name is the device's name or not.
 * @param me    Device handle.
 * @param name  Device name.
 * @return True or false.
 */
bool elab_device_of_name(elab_device_t *me, const char *name);

/**
 * @brief This function check one device name is valid or not.
 * @param name  Device name.
 * @return Valid if true and invalid if false.
 */
bool elab_device_valid(const char *name);

/**
 * @brief This function check one device name is sole or not.
 * @param name  Device name.
 * @return Valid if true and invalid if false.
 */
bool elab_device_is_sole(elab_device_t *me);

/**
 * @brief Check the device is in test mode or not.
 * @param dev       the pointer of device driver structure
 * @retval True or false.
 */
bool elab_device_is_test_mode(elab_device_t *me);

/**
 * @brief Set the test mode for the device.
 * @param dev       The pointer of device driver structure
 * @retval None.
 */
void elab_device_set_test_mode(elab_device_t *me);

/**
 * @brief Set the normal mode for the device.
 * @param dev       the pointer of device driver structure
 * @retval None.
 */
void elab_device_set_normal_mode(elab_device_t *me);

/**
 * @brief This function check one device is enabled or not.
 * @param name  Device name.
 * @return Valid if true and invalid if false.
 */
bool elab_device_is_enabled(elab_device_t *me);

/**
 * @brief Device general reading function.
 * @param me        Device handle.
 * @param pos       Reading position
 * @param buffer    Reading buffer.
 * @param size      Expected read size.
 * @return if > 0, actual reading size; if < 0, error ID.
 */
int32_t elab_device_read(elab_device_t *me,
                            uint32_t pos, void *buffer, uint32_t size);

/**
 * @brief Device general writting function.
 * @param me        Device handle.
 * @param pos       Writting position
 * @param buffer    Writting buffer.
 * @param size      Expected writting size.
 * @return if > 0, actual writting size; if < 0, error ID.
 */
int32_t elab_device_write(elab_device_t *me,
                            uint32_t pos, const void *buffer, uint32_t size);

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

/**
 * @brief Lock the device to ensure its thread-safety.
 * @param _dev  Device handle.
 * @return None.
 */
#define elab_device_lock(_dev)              do {} while (0)

/**
 * @brief Unlock the device to ensure its thread-safety.
 * @param _dev  Device handle.
 * @return None.
 */
#define elab_device_unlock(_dev)            do {} while (0)

/* private function --------------------------------------------------------- */
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
