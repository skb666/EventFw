
/*
 * EventOS Log Module
 * Copyright (c) 2022, EventOS Team, <event-os@outlook.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * https://www.event-os.cn
 * https://github.com/event-os/eventos
 * https://gitee.com/event-os/eventos
 * 
 * Change Logs:
 * Date           Author        Notes
 * 2022-08-31     Dog           V0.1
 */

#ifndef __ELOG_H__
#define __ELOG_H__

/* include ------------------------------------------------------------------ */
#include <stdbool.h>
#include "eos.h"

#ifdef __cplusplus
extern "C" {
#endif

/* config ------------------------------------------------------------------- */
#define ELOG_USE_TIME_SIMPLE                    (1)

/* public define ------------------------------------------------------------ */
#define ELOG_TAG_NAME_SIZE                      (16)
#define ELOG_MUTEX_TIMEOUT                      (200)
#define ELOG_BUFF_LOG_SIZE                      10240
#define ELOG_SIZE_LOG                           256
#define ELOG_MAX_OBJECTS                        32
#define ELOG_TAG_MAX_LENGTH                     16
#define ELOG_LINE_FEED                          0   // 0: \n, 1: \r\n

enum
{
    eLogLevel_Debug = 0,
    eLogLevel_Info,
    eLogLevel_Warn,
    eLogLevel_Error,
};

enum
{
    eLogMode_BlackList = 0,
    eLogMode_WhiteList,
};

#if (ELOG_SIZE_LOG < 48)
    #error "The elog buffer size must be not smaller that 48."
#endif

/* public typedef ----------------------------------------------------------- */
typedef enum elog_err
{
    Elog_OK = 0,
    ElogErr_MutexErr                        = -1,
    ElogErr_MutexInitFail                   = -2,
} elog_err_t;

typedef struct elog_device
{
    struct elog_device *next;

    const char *name;
    eos_u8_t level;
    bool enable;

    void (* out)(const char *data);
    void (* flush)(void);
    void (* init)(void);
} elog_device_t;

/* public function ---------------------------------------------------------- */
/* elog basic functions */
elog_err_t elog_init(void);
elog_err_t elog_enable(bool enable);
elog_err_t elog_set_level(eos_u8_t level);
elog_err_t elog_set_mode(eos_u8_t mode);

/* elog device functions */
elog_err_t elog_device_register(elog_device_t *device);
elog_err_t elog_device_remove(elog_device_t *device);
elog_err_t elog_device_set_level(const char *name, eos_u8_t level);
elog_err_t elog_device_enable(const char *name, bool enable);

/* elog tag filter functions */
elog_err_t elog_tag_add(const char *tag);
elog_err_t elog_tag_remove(const char *tag);
elog_err_t elog_tag_clear_all(void);

/* print functions */
void _elog_print(const char *tag, const char *s_format, ...);
void _elog_debug(const char *tag, const char *s_format, ...);
void _elog_info(const char *tag, const char *s_format, ...);
void _elog_warn(const char *tag, const char *s_format, ...);
void _elog_error(const char *tag, const char *s_format, ...);
void elog_assert(const char *tag, const char *name, eos_u32_t id);
void elog_assert_info(const char *tag, const char *s_format, ...);
void elog_flush(void);

/* print macros */
#define elog_print(...)           _elog_print(___tag_name, __VA_ARGS__)
#define elog_debug(...)           _elog_debug(___tag_name, __VA_ARGS__)
#define elog_info(...)            _elog_info(___tag_name, __VA_ARGS__)
#define elog_warn(...)            _elog_warn(___tag_name, __VA_ARGS__)
#define elog_error(...)           _elog_error(___tag_name, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* __ELOG_H__ */

/* note---------------------------------------------------------------------- */
/* Log format:
    [E/Tag Time] Log content.
*/

/* ----------------------------- end of file -------------------------------- */