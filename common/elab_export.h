/*
 * eLab Project
 * Copyright (c) 2023, EventOS Team, <event-os@outlook.com>
 */

#ifndef __ELAB_EXPORT_H__
#define __ELAB_EXPORT_H__

/* include ------------------------------------------------------------------ */
#include <stdint.h>
#include <stdbool.h>
#include "elab_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* public define ------------------------------------------------------------ */
#define EXPORT_ID_INIT                  (0xa5a5a5a5)
#define EXPORT_ID_POLL                  (0xbeefbeef)

/* public define ------------------------------------------------------------ */
enum elab_export_level
{
    EXPORT_POLL                         = -2,
    EXPORT_UNIT_TEST                    = -1,
    EXPORT_LEVEL_BSP                    = 0,
    EXPORT_LEVEL_HW_INDEPNEDENT         = 0,
    EXPORT_DRVIVER                      = 1,
    EXPORT_MIDWARE                      = 2,
    EXPORT_DEVICE                       = 3,
    EXPORT_APP                          = 4,
    EXPORT_USER                         = 5,
};

/* public typedef ----------------------------------------------------------- */
typedef struct elab_export_poll_data
{
    uint32_t timeout_ms;
} elab_export_poll_data_t;

typedef struct elab_export
{
    uint32_t magic_head;
    const char *name;
    void *func;
    void *data;
    bool exit;
    int8_t level;
    uint8_t type;
    uint32_t period_ms;
#if defined(_WIN32)
    uint32_t temp[9];
#elif defined(__linux__)
    uint32_t temp[4];
#endif
    uint32_t magic_tail;
} elab_export_t;

/* private function --------------------------------------------------------- */
void elab_unit_test(void);
void elab_run(void);

/* public export ------------------------------------------------------------ */
/**
  * @brief  Initialization function exporting macro.
  * @param  _func   The initialization function.
  * @param  _level  The export level, [0, 127].
  * @retval None.
  */
#define INIT_EXPORT(_func, _level)                                             \
    ELAB_USED const elab_export_t init_##_func ELAB_SECTION("elab_export") =   \
    {                                                                          \
        .name = #_func,                                                        \
        .func = (void *)&_func,                                                \
        .level = _level,                                                       \
        .exit = false,                                                         \
        .magic_head = EXPORT_ID_INIT,                                          \
        .magic_tail = EXPORT_ID_INIT,                                          \
    }

/**
  * @brief  Exiting function exporting macro.
  * @param  _func   The polling function.
  * @param  _level  The export level, [0, 127].
  * @retval None.
  */
#define EXIT_EXPORT(_func, _level)                                             \
    ELAB_USED const elab_export_t exit_##_func ELAB_SECTION("elab_export") =   \
    {                                                                          \
        .name = #_func,                                                        \
        .func = (void *)&_func,                                                \
        .level = _level,                                                       \
        .exit = true,                                                          \
        .magic_head = EXPORT_ID_INIT,                                          \
        .magic_tail = EXPORT_ID_INIT,                                          \
    }

/**
  * @brief  Unit test function exporting macro.
  * @param  _func   The unit test function.
  * @retval None.
  */
#define UNIT_TEST_EXPORT(_func)             INIT_EXPORT(_func, EXPORT_UNIT_TEST)

/**
  * @brief  Poll function exporting macro.
  * @param  _func       The polling function.
  * @param  _period_ms  The polling period in ms. 
  *                     The max value is 30 days (2592000000ms).
  * @retval None.
  */
#define POLL_EXPORT(_func, _period_ms)                                         \
    static elab_export_poll_data_t poll_##_func##_data =                       \
    {                                                                          \
        .timeout_ms = 0,                                                       \
    };                                                                         \
    ELAB_USED const elab_export_t poll_##_func ELAB_SECTION("expoll") =        \
    {                                                                          \
        .name = "poll",                                                        \
        .func = &_func,                                                        \
        .data = (void *)&poll_##_func##_data,                                  \
        .level = EXPORT_POLL,                                                  \
        .period_ms = (uint32_t)(_period_ms),                                   \
        .magic_head = EXPORT_ID_POLL,                                          \
        .magic_tail = EXPORT_ID_POLL,                                          \
    }

#ifdef __cplusplus
}
#endif

#endif /* __ELAB_EXPORT_H__ */

/* ----------------------------- end of file -------------------------------- */
