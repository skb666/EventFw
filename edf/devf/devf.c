/*
 * eLab Project
 * Copyright (c) 2023, EventOS Team, <event-os@outlook.com>
 */

/* includes ----------------------------------------------------------------- */
#include "devf.h"
//#include "elab_device_def.h"
#include "elab_assert.h"
#include "elab_log.h"

ELAB_TAG("EdfDevice");

/* private function prototypes ---------------------------------------------- */
static void _add_device(elab_device_t *me);

/* private variables -------------------------------------------------------- */
static uint32_t _edf_device_count = 0;
static elab_device_t *_edf_table[ELAB_DEV_NUM_MAX];

/* public function ---------------------------------------------------------- */
/**
 * This function registers a device driver with specified name.
 * @param dev the pointer of device driver structure
 * @param name the device driver's name
 * @return None.
 */
void elab_device_register(elab_device_t *me, elab_device_attr_t *attr)
{
    assert(me != NULL);
    assert(attr != NULL);
    assert(attr->name != NULL);
    assert_name(elab_device_find(attr->name) == NULL, attr->name);

    /* Set the device data. */
    memcpy(&me->attr, attr, sizeof(elab_device_attr_t));
    me->enable_count = 0;

    /* Add the device the edf table. */
    memcpy(&me->attr, attr, sizeof(elab_device_attr_t));
    _add_device(me);

}

/**
 * @brief This function unregisters a device with the device handle.
 * @param me   the pointer of device driver structure
 * @retval None.
 */
void elab_device_unregister(elab_device_t *me)
{
    elab_assert(me != NULL);
    elab_assert(!elab_device_is_enabled(me));

    for (uint32_t i = 0; i < ELAB_DEV_NUM_MAX; i ++)
    {
        if (_edf_table[i] == me)
        {
            _edf_table[i] = NULL;
            _edf_device_count --;
            break;
        }
    }

}

/**
 * @brief Get the count number in device framework management.
 * @retval Count number of devices.
 */
uint32_t elab_device_get_number(void)
{
    uint32_t num = 0;

    num = _edf_device_count;

    return num;
}

/**
 * This function check the given name is the device's name or not.
 * @param me    Device handle.
 * @param name  Device name.
 * @return True or false.
 */
bool elab_device_of_name(elab_device_t *me, const char *name)
{
    bool of_the_name = false;

    elab_device_lock(me);
    if (strcmp(me->attr.name, name) == 0)
    {
        of_the_name = true;
    }
    elab_device_unlock(me);

    return of_the_name;
}

/**
 * This function finds a device driver by specified name.
 * @param name  Device name.
 * @return Device handle. If not found, return NULL.
 */
elab_device_t *elab_device_find(const char *name)
{
    if (_edf_table[0] != NULL)
        assert_name(_edf_table[0]->attr.name != NULL, name);
    assert(name != NULL);

    /* Find the device */
    elab_device_t *me = NULL;
    for (uint32_t i = 0; i < ELAB_DEV_NUM_MAX; i++)
    {
        /* No device yet in the device table. */
        if (_edf_table[i] == NULL)
        {
            break;
        }
        elab_assert(_edf_table[i]->attr.name != NULL);
        /* Device matching */
        if (strcmp(_edf_table[i]->attr.name, name) == 0)
        {
            me = _edf_table[i];
            break;
        }
    }

    return me;
}

/**
 * This function check one device name is valid or not.
 * @param name  Device name.
 * @return Valid if true and invalid if false.
 */
bool elab_device_valid(const char *name)
{
    return elab_device_find(name) == NULL ? false : true;
}

/**
 * @brief This function check one device name is sole or not.
 * @param name  Device name.
 * @return Valid if true and invalid if false.
 */
bool elab_device_is_sole(elab_device_t *me)
{
    elab_device_lock(me);
    bool enable_status = me->attr.sole;
    elab_device_unlock(me);

    return enable_status;
}

/**
 * @brief Check the device is in test mode or not.
 * @param dev       the pointer of device driver structure
 * @retval True or false.
 */
bool elab_device_is_test_mode(elab_device_t *dev)
{
    return dev->test_mode;
}

/**
 * @brief Set the test mode for the device.
 * @param dev       The pointer of device driver structure
 * @retval None.
 */
void elab_device_set_test_mode(elab_device_t *dev)
{
    elab_assert(dev != NULL);

    elab_device_lock(dev);
    dev->test_mode = true;
    elab_device_unlock(dev);
}

/**
 * @brief Set the normal mode for the device.
 * @param dev       the pointer of device driver structure
 * @retval None.
 */
void elab_device_set_normal_mode(elab_device_t *dev)
{
    elab_assert(dev != NULL);

    elab_device_lock(dev);
    dev->test_mode = false;
    elab_device_unlock(dev);
}

/**
 * @brief This function check one device is enabled or not.
 * @param name  Device name.
 * @return Valid if true and invalid if false.
 */
bool elab_device_is_enabled(elab_device_t *me)
{
    assert(me != NULL);

    elab_device_lock(me);
    bool enable_status = me->enable_count > 0 ? true : false;
    elab_device_unlock(me);

    return enable_status;
}


/**
 * This function will enable or disable a device
 * @param me        Device handle.
 * @param status    The locking status.
 * @return the result
 */
elab_err_t __device_enable(elab_device_t *me, bool status)
{
    assert(me != NULL);
    assert(me->ops != NULL);
    assert(me->ops->enable != NULL);

    elab_device_lock(me);

    if (me->attr.sole)
    {
        if (status)
        {
            assert_name(me->enable_count == 0, me->attr.name);
        }
        else
        {
            assert_name(me->enable_count > 0, me->attr.name);
        }
    }
    else
    {
        assert_name(me->enable_count < UINT8_MAX, me->attr.name);
    }

    elab_err_t ret = ELAB_OK;
    if (status && me->enable_count == 0)
    {
        ret = me->ops->enable(me, true);
    }
    else if (!status && me->enable_count == 1)
    {
        ret = me->ops->enable(me, false);
    }
    me->enable_count = status ? (me->enable_count + 1) : (me->enable_count - 1);

    elab_device_unlock(me);

    return ret;
}

/**
 * This function will read some data from a device.
 *
 * @param dev       the pointer of device driver structure
 * @param buffer    the data buffer to save read data
 * @param size      the size of buffer
 *
 * @return the actually read size on successful, otherwise negative returned.
 *
 */
int32_t elab_device_read(elab_device_t *me,
                         uint32_t pos, void *buffer, uint32_t size)
{
    assert(me != NULL);
    assert(me->enable_count != 0);
    assert(me->ops != NULL);
    assert(me->ops->read != NULL);

    int32_t ret = 0;
    if (elab_device_is_test_mode(me))
    {
        ret = ELAB_OK;
        goto exit;
    }

    ret = me->ops->read(me, pos, buffer, size);

    exit:
    return ret;
}

/**
 * This function will write some data to a device.
 *
 * @param me        the pointer of device driver structure.
 * @param buffer    the data buffer to be written to device
 * @param size      the size of buffer
 *
 * @return The actually written size on successful, otherwise negative returned.
 *
 */
int32_t elab_device_write(elab_device_t *me,
                          uint32_t pos, const void *buffer, uint32_t size)
{
    assert(me != NULL);
    assert(me->enable_count != 0);
    assert(me->ops != NULL);
    assert(me->ops->write != NULL);

    int32_t ret = 0;
    if (elab_device_is_test_mode(me))
    {
        ret = ELAB_OK;
        goto exit;
    }

    ret = me->ops->write(me, pos, buffer, size);

    exit:
    return ret;
}

static void _add_device(elab_device_t *me)
{
    assert(_edf_device_count < ELAB_DEV_NUM_MAX);

    if (_edf_device_count == 0)
    {
        for (uint32_t i = 0; i < ELAB_DEV_NUM_MAX; i ++)
        {
            _edf_table[i] = NULL;
        }
    }
    _edf_table[_edf_device_count ++] = me;
}

/* ----------------------------- end of file -------------------------------- */
