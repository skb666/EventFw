/*
 * eLab Project
 * Copyright (c) 2023, EventOS Team, <event-os@outlook.com>
 */

/* includes ----------------------------------------------------------------- */
#include "elab_device.h"
#include "elab_device_def.h"
#include "../common/elab_assert.h"
#include "../common/elab_log.h"
#include "../os/cmsis_os.h"

ELAB_TAG("EdfDevice");

/* private config ----------------------------------------------------------- */

/* private defines ---------------------------------------------------------- */

/* private function prototypes ---------------------------------------------- */
static void _add_device(elab_device_t *me);
static osMutexId_t _edf_mutex(void);

/* private variables -------------------------------------------------------- */
static uint32_t _edf_device_count = 0;
static elab_device_t *_edf_table[ELAB_DEV_NUM_MAX];
static osMutexId_t _mutex_edf = NULL;

/**
 * The edf global mutex attribute.
 */
static const osMutexAttr_t _mutex_attr_edf =
{
    .name = "mutex_edf",
    .attr_bits = osMutexPrioInherit | osMutexRecursive,
    .cb_mem = NULL,
    .cb_size = 0, 
};

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
}

/* private functions -------------------------------------------------------- */
static osMutexId_t _edf_mutex(void)
{
    if (_mutex_edf == NULL)
    {
        _mutex_edf = osMutexNew(&_mutex_attr_edf);
        assert(_mutex_edf != NULL);
    }

    return _mutex_edf;
}

/* ----------------------------- end of file -------------------------------- */
