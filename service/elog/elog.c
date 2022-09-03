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

/* include ------------------------------------------------------------------ */
#include "elog.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>

EOS_TAG("elog")

#ifdef __cplusplus
extern "C" {
#endif

/* private define ----------------------------------------------------------- */
#define eLogLevel_Print                 (0)
#define ELOG_MAGIC_NUMBER               (0xdeadbeef)

#define FORMAT_FLAG_LEFT_JUSTIFY        (1u << 0)
#define FORMAT_FLAG_PAD_ZERO            (1u << 1)
#define FORMAT_FLAG_PRINT_SIGN          (1u << 2)
#define FORMAT_FLAG_ALTERNATE           (1u << 3)

/* private typedef ---------------------------------------------------------- */
typedef struct elog_time_tag
{
    eos_u32_t hour                      : 5;
    eos_u32_t minute                    : 6;
    eos_u32_t rsv                       : 5;
    eos_u32_t second                    : 6;
    eos_u32_t ms                        : 10;
} elog_time_t;

typedef struct
{
    char *p_buff;
    eos_u32_t cnt;
} eos_buffer_t;

typedef struct elog_object
{
    char tag[ELOG_TAG_MAX_LENGTH];
} elog_object_t;

typedef struct elog
{
    eos_u32_t magic;
    char buff_assert[ELOG_SIZE_LOG];
    eos_u32_t count_assert;
    bool enable;
    eos_u8_t level;
    eos_u8_t color;
    eos_u8_t mode;
    elog_time_t time;
} elog_t;

typedef struct elog_hash
{
    elog_object_t obj[ELOG_MAX_OBJECTS];
    eos_u16_t prime_max;
} elog_hash_t;

/* private variables -------------------------------------------------------- */
static elog_hash_t hash;
static elog_device_t *dev_list;
static elog_t elog;
static eos_mutex_t elog_mutex;
static char buff[ELOG_SIZE_LOG];
static elog_time_t time;

eos_u32_t elog_error_id = 0;

extern volatile eos_s8_t eos_interrupt_nest;

static const char *string_color_log[] =
{
    "\033[0m", "\033[1;32m", "\033[1;33m", "\033[1;31m"
};
static const char *string_level_log[] =
{
    "D", "I", "W", "E"
};

/* private function prototype ----------------------------------------------- */
static void _elog_time(eos_u32_t time_ms, elog_time_t *log_time);
static eos_u32_t _hash_time33(const char *string);
static eos_u16_t _hash_insert(const char *string);
static eos_u16_t _hash_get_index(const char *string);
static void _printf(const char *tag,
                        eos_u8_t level,
                        bool linefeed_en,
                        const char * s_format,
                        va_list *param_list);
static bool _tag_is_enabled(const char *tag);

/* public function ---------------------------------------------------------- */
/**
 * @brief   elog module initialization.
 * @return  None.
 */
elog_err_t elog_init(void)
{
    /* The function is not permitted to use in interrupt function. */
    EOS_ASSERT(eos_interrupt_get_nest() == 0);

    elog_err_t ret = Elog_OK;

    elog.magic = ELOG_MAGIC_NUMBER;

    /* Initialize the hash table. */
    for (eos_s32_t i = ELOG_MAX_OBJECTS; i > 0; i --)
    {
        bool is_prime = true;
        for (eos_u32_t j = 2; j < ELOG_MAX_OBJECTS; j ++)
        {
            if (i <= j)
            {
                break;
            }
            if ((i % j) == 0)
            {
                is_prime = false;
                break;
            }
        }
        if (is_prime == true)
        {
            hash.prime_max = i;
            break;
        }
    }
    for (eos_u32_t i = 0; i < ELOG_MAX_OBJECTS; i ++)
    {
        hash.obj[i].tag[0] = 0;
    }

    /* Initialize elog block. */
    elog.count_assert = 0;
    elog.enable = false;
    elog.level = eLogLevel_Error;
    elog.mode = eLogMode_BlackList;

    /* Initialize elog assert buffer. */
    for (eos_s32_t i = 0; i < ELOG_SIZE_LOG; i ++)
    {
        elog.buff_assert[i] = 0;
    }
    elog.count_assert = 0;

    /* Initialize elog mutex. */
    eos_err_t ret_mutex = eos_mutex_init(&elog_mutex);
    if (ret_mutex != EOS_EOK)
    {
        ret = ElogErr_MutexInitFail;
        goto exit;
    }

exit:
    return ret;
}

/**
 * @brief   Enable or disable the elog module.
 * @param   enable Enable status.
 * @return  See elog_err_t in elog.h.
 */
elog_err_t elog_enable(bool enable)
{
    /* Check elog block is initialized or covered. */
    EOS_ASSERT(elog.magic == ELOG_MAGIC_NUMBER);
    /* The function is not permitted to use in interrupt function. */
    EOS_ASSERT(eos_interrupt_get_nest() == 0);

    elog_err_t ret = Elog_OK;

    /* Lock the elog mutex. */
    eos_err_t ret_mutex = eos_mutex_take(&elog_mutex, ELOG_MUTEX_TIMEOUT);
    if (ret_mutex != EOS_EOK)
    {
        ret = ElogErr_MutexErr;
        goto exit;
    }

    /* enable or disable elog */
    elog.enable = enable;

    /* Unlock the elog mutex. */
    eos_mutex_release(&elog_mutex);

exit:
    return ret;
}

/**
 * @brief   Set elog general log level.
 * @param   level log level.
 * @return  See elog_err_t in elog.h.
 */
elog_err_t elog_set_level(eos_u8_t level)
{
    /* Check elog block is initialized or covered. */
    EOS_ASSERT(elog.magic == ELOG_MAGIC_NUMBER);
    /* The function is not permitted to use in interrupt function. */
    EOS_ASSERT(eos_interrupt_get_nest() == 0);

    elog_err_t ret = Elog_OK;

    /* Lock the elog mutex. */
    eos_err_t ret_mutex = eos_mutex_take(&elog_mutex, ELOG_MUTEX_TIMEOUT);
    if (ret_mutex != EOS_EOK)
    {
        ret = ElogErr_MutexErr;
        goto exit;
    }

    /* set elog general log level */
    elog.level = level;

    /* Unlock the elog mutex. */
    eos_mutex_release(&elog_mutex);

exit:
    return ret;
}

/**
 * @brief   Set elog log mode.
 * @param   mode log mode.
 * @return  See elog_err_t in elog.h.
 */
elog_err_t elog_set_mode(eos_u8_t mode)
{
    /* Check elog block is initialized or covered. */
    EOS_ASSERT(elog.magic == ELOG_MAGIC_NUMBER);
    /* The function is not permitted to use in interrupt function. */
    EOS_ASSERT(eos_interrupt_get_nest() == 0);

    elog_err_t ret = Elog_OK;

    /* Lock the elog mutex. */
    eos_err_t ret_mutex = eos_mutex_take(&elog_mutex, ELOG_MUTEX_TIMEOUT);
    if (ret_mutex != EOS_EOK)
    {
        ret = ElogErr_MutexErr;
        goto exit;
    }

    /* set elog general log level */
    elog.mode = mode;

    /* Unlock the elog mutex. */
    eos_mutex_release(&elog_mutex);

exit:
    return ret;
}

/**
 * @brief   Register one device into the elog module.
 * @param   device is the elog device handle.
 * @return  See elog_err_t in elog.h.
 */
elog_err_t elog_device_register(elog_device_t *device)
{
    /* Check elog block is initialized or covered. */
    EOS_ASSERT(elog.magic == ELOG_MAGIC_NUMBER);
    /* The function is not permitted to use in interrupt function. */
    EOS_ASSERT(eos_interrupt_get_nest() == 0);

    elog_err_t ret = Elog_OK;

    /* Lock the elog mutex. */
    eos_err_t ret_mutex = eos_mutex_take(&elog_mutex, ELOG_MUTEX_TIMEOUT);
    if (ret_mutex != EOS_EOK)
    {
        ret = ElogErr_MutexErr;
        goto exit;
    }
    
    /* It's not permitted that log device is regitstered after the log module
       starts. */
    EOS_ASSERT(elog.enable == false);

    /* Check the log device has not same name with the former devices. */
    elog_device_t *node = dev_list;
    while (node != EOS_NULL)
    {
        EOS_ASSERT(strcmp(node->name, device->name) != 0);
        EOS_ASSERT(device != node);
        node = node->next;
    }

    /* Initialize the device. */
    device->enable = false;
    device->level = eLogLevel_Debug;

    /* Add the log device to the list. */
    device->next = dev_list;
    dev_list = device;

    /* Unlock the elog mutex. */
    eos_mutex_release(&elog_mutex);

exit:
    return ret;
}

/**
 * @brief   Remove one device from the elog module.
 * @param   device is the elog device handle.
 * @return  See elog_err_t in elog.h.
 */
elog_err_t elog_device_remove(elog_device_t *device)
{
    /* Check elog block is initialized or covered. */
    EOS_ASSERT(elog.magic == ELOG_MAGIC_NUMBER);
    /* The function is not permitted to use in interrupt function. */
    EOS_ASSERT(eos_interrupt_get_nest() == 0);

    elog_err_t ret = Elog_OK;

    /* Lock the elog mutex. */
    eos_err_t ret_mutex = eos_mutex_take(&elog_mutex, ELOG_MUTEX_TIMEOUT);
    if (ret_mutex != EOS_EOK)
    {
        ret = ElogErr_MutexErr;
        goto exit;
    }

    /* Check the log device has not same name with the former devices. */
    elog_device_t *node = dev_list;
    if (node == device)
    {
        dev_list = device->next;
    }
    else
    {
        while (node->next != EOS_NULL)
        {
            if (device == node->next)
            {
                node->next = node->next->next;
                break;
            }
        }
    }

    /* Unlock the elog mutex. */
    eos_mutex_release(&elog_mutex);

exit:
    return ret;
}

/**
 * @brief   Set elog device's private log level.
 * @param   name    is the elog device name.
 * @param   level   is the private log level of elog device.
 * @return  See elog_err_t in elog.h.
 */
elog_err_t elog_device_set_level(const char *name, eos_u8_t level)
{
    /* Check elog block is initialized or covered. */
    EOS_ASSERT(elog.magic == ELOG_MAGIC_NUMBER);
    /* The function is not permitted to use in interrupt function. */
    EOS_ASSERT(eos_interrupt_get_nest() == 0);

    elog_err_t ret = Elog_OK;

    /* Lock the elog mutex. */
    eos_err_t ret_mutex = eos_mutex_take(&elog_mutex, ELOG_MUTEX_TIMEOUT);
    if (ret_mutex != EOS_EOK)
    {
        ret = ElogErr_MutexErr;
        goto exit;
    }

    /* It's not permitted that log device is regitstered after the log module
       starts. */
    EOS_ASSERT(dev_list != EOS_NULL);

    /* Check the log device has not same name with the former devices. */
    elog_device_t *node = dev_list;
    while (node != EOS_NULL)
    {
        if (strcmp(node->name, name) == 0)
        {
            node->level = level;
            goto exit;
        }

        node = node->next;
    }

    /* Log device not found. Maybe you have give a wrong device name. */
    EOS_ASSERT(0);

    /* Unlock the elog mutex. */
    eos_mutex_release(&elog_mutex);

exit:
    return ret;
}

/**
 * @brief   Enable ro disable one elog device.
 * @param   name    is the elog device name.
 * @param   enable  is enabled status.
 * @return  See elog_err_t in elog.h.
 */
elog_err_t elog_device_enable(const char *name, bool enable)
{
    /* Check elog block is initialized or covered. */
    EOS_ASSERT(elog.magic == ELOG_MAGIC_NUMBER);
    /* The function is not permitted to use in interrupt function. */
    EOS_ASSERT(eos_interrupt_get_nest() == 0);

    elog_err_t ret = Elog_OK;

    /* Lock the elog mutex. */
    eos_err_t ret_mutex = eos_mutex_take(&elog_mutex, ELOG_MUTEX_TIMEOUT);
    if (ret_mutex != EOS_EOK)
    {
        ret = ElogErr_MutexErr;
        goto exit;
    }

    /* It's not permitted that log device is regitstered after the log module
       starts. */
    EOS_ASSERT(dev_list != EOS_NULL);

    /* Check the log device has not same name with the former devices. */
    elog_device_t *node = dev_list;
    while (node != EOS_NULL)
    {
        if (strcmp(node->name, name) == 0)
        {
            node->enable = enable;
            goto exit;
        }

        node = node->next;
    }

    /* Log device not found. Maybe you have give a wrong device name. */
    EOS_ASSERT(0);

    /* Unlock the elog mutex. */
    eos_mutex_release(&elog_mutex);

exit:
    return ret;
}

/**
 * @brief   Add the log tag and it's private log level.
 * @param   tag     is the tag name
 * @return  See elog_err_t in elog.h.
 */
elog_err_t elog_tag_add(const char *tag)
{
    /* Check elog block is initialized or covered. */
    EOS_ASSERT(elog.magic == ELOG_MAGIC_NUMBER);
    /* The function is not permitted to use in interrupt function. */
    EOS_ASSERT(eos_interrupt_get_nest() == 0);
    /* Check the tag name's length is valid */
    EOS_ASSERT(strlen(tag) < ELOG_TAG_MAX_LENGTH);

    elog_err_t ret = Elog_OK;

    /* Lock the elog mutex. */
    eos_err_t ret_mutex = eos_mutex_take(&elog_mutex, ELOG_MUTEX_TIMEOUT);
    if (ret_mutex != EOS_EOK)
    {
        ret = ElogErr_MutexErr;
        goto exit;
    }

    /* Check elog block is not existent. */
    EOS_ASSERT(_hash_get_index(tag) == ELOG_MAX_OBJECTS);

    /* Check the tag table is full or not. */
    eos_u16_t tag_index = _hash_insert(tag);
    EOS_ASSERT(tag_index != ELOG_MAX_OBJECTS);

    /* add the tag */
    hash.obj[tag_index].tag[0] = 0;
    strcpy(hash.obj[tag_index].tag, tag);

    /* Unlock the elog mutex. */
    eos_mutex_release(&elog_mutex);

exit:
    return ret;
}

/**
 * @brief   Enable ro disable one elog device.
 * @param   tag     is the tag name
 * @return  See elog_err_t in elog.h.
 */
elog_err_t elog_tag_remove(const char *tag)
{
    /* Check elog block is initialized or covered. */
    EOS_ASSERT(elog.magic == ELOG_MAGIC_NUMBER);
    /* The function is not permitted to use in interrupt function. */
    EOS_ASSERT(eos_interrupt_get_nest() == 0);
    /* Check the tag name's length is valid */
    EOS_ASSERT(strlen(tag) < ELOG_TAG_MAX_LENGTH);

    elog_err_t ret = Elog_OK;

    /* Lock the elog mutex. */
    eos_err_t ret_mutex = eos_mutex_take(&elog_mutex, ELOG_MUTEX_TIMEOUT);
    if (ret_mutex != EOS_EOK)
    {
        ret = ElogErr_MutexErr;
        goto exit;
    }

    /* Remove the tag, if not exitent, just return. */
    eos_u16_t tag_index = _hash_get_index(tag);
    if (tag_index != ELOG_MAX_OBJECTS)
    {
        hash.obj[tag_index].tag[0] = 0;
    }

    /* Unlock the elog mutex. */
    eos_mutex_release(&elog_mutex);

exit:
    return ret;
}

/**
 * @brief   Clear all log tags.
 */
elog_err_t elog_tag_clear_all(void)
{
    /* Check elog block is initialized or covered. */
    EOS_ASSERT(elog.magic == ELOG_MAGIC_NUMBER);
    /* The function is not permitted to use in interrupt function. */
    EOS_ASSERT(eos_interrupt_get_nest() == 0);

    elog_err_t ret = Elog_OK;

    /* Lock the elog mutex. */
    eos_err_t ret_mutex = eos_mutex_take(&elog_mutex, ELOG_MUTEX_TIMEOUT);
    if (ret_mutex != EOS_EOK)
    {
        ret = ElogErr_MutexErr;
        goto exit;
    }

    /* Clear the tag table. */
    for (eos_u32_t i = 0; i < ELOG_MAX_OBJECTS; i ++)
    {
        hash.obj[i].tag[0] = 0;
    }
    
    /* Unlock the elog mutex. */
    eos_mutex_release(&elog_mutex);

exit:
    return ret;
}

/**
 * @brief   elog basic print function.
 */
void _elog_print(const char *tag, const char *s_format, ...)
{
    va_list param_list;
    va_start(param_list, s_format);
    _printf(tag, (eLogLevel_Error + 1), false, s_format, &param_list);
    va_end(param_list);
}

/**
 * @brief   elog print function in debug level.
 */
void _elog_debug(const char *tag, const char *s_format, ...)
{
    EOS_ASSERT(strlen(tag) < ELOG_TAG_NAME_SIZE);

    va_list param_list;
    va_start(param_list, s_format);
    _printf(tag, eLogLevel_Debug, true, s_format, &param_list);
    va_end(param_list);
}

/**
 * @brief   elog print function in info level.
 */
void _elog_info(const char *tag, const char *s_format, ...)
{
    EOS_ASSERT(strlen(tag) < ELOG_TAG_NAME_SIZE);
    
    va_list param_list;
    va_start(param_list, s_format);
    _printf(tag, eLogLevel_Info, true, s_format, &param_list);
    va_end(param_list);
}

/**
 * @brief   elog print function in warn level.
 */
void _elog_warn(const char *tag, const char *s_format, ...)
{
    EOS_ASSERT(strlen(tag) < ELOG_TAG_NAME_SIZE);
    
    va_list param_list;
    va_start(param_list, s_format);
    _printf(tag, eLogLevel_Warn, true, s_format, &param_list);
    va_end(param_list);
}

/**
 * @brief   elog print function in error level.
 */
void _elog_error(const char *tag, const char *s_format, ...)
{
    EOS_ASSERT(strlen(tag) < ELOG_TAG_NAME_SIZE);

    va_list param_list;
    va_start(param_list, s_format);
    _printf(tag, eLogLevel_Error, true, s_format, &param_list);
    va_end(param_list);
}

/**
 * @brief   elog flush log contents to the devices.
 */
void elog_flush(void)
{
    /* Lock the elog mutex. */
    eos_mutex_take(&elog_mutex, ELOG_MUTEX_TIMEOUT);
    
    /* It's not permitted that log device is regitstered after the log module
       starts. */
    EOS_ASSERT(dev_list != EOS_NULL);

    /* Check the log device has not same name with the former devices. */
    elog_device_t *next = dev_list;
    while (next != EOS_NULL)
    {
        next->flush();
        next = next->next;
    }

    /* Unlock the elog mutex. */
    eos_mutex_release(&elog_mutex);
}

/**
 * @brief   elog assert function.
 */
void elog_assert(const char *tag, const char *name, eos_u32_t id)
{
    elog_error_id = id;
    
    memset(buff, 0, ELOG_SIZE_LOG);
    strcat(buff, string_color_log[eLogLevel_Error]);
    eos_s32_t len_color = strlen(buff);

    /* For example: Assert! EventOS Log, function_example, 420. */
    if (name != 0)
    {
        sprintf(&buff[len_color], "Assert! %s, %s, %d.\n\r", tag, name, id);
    }
    
    /* For example: Assert! EventOS Log, 420. */
    else
    {
        sprintf(&buff[len_color], "Assert! %s, %d.\n\r", tag, id);
    }

    /* Unlock the elog mutex. */
    eos_mutex_take(&elog_mutex, ELOG_MUTEX_TIMEOUT);
    
    /* Check the log device has not same name with the former devices. */
    elog_device_t *next = dev_list;
    while (next != EOS_NULL)
    {
        next->out(buff);
        next->flush();
        next = next->next;
    }
    
    /* Unlock the elog mutex. */
    eos_mutex_release(&elog_mutex);
    
    eos_hw_interrupt_disable();
    while (1)
    {
    }
}

/**
 * @brief   elog assert function with printable payload.
 */
void elog_assert_info(const char *tag, const char *s_format, ...)
{
    memset(buff, 0, ELOG_SIZE_LOG);
    
    eos_s32_t len = sprintf(buff, "%sAssert! (%s) ",
                          string_color_log[eLogLevel_Error],
                          tag);
    
    va_list param_list;
    va_start(param_list, s_format);
    vsnprintf(&buff[len], ELOG_SIZE_LOG - len, s_format, param_list);
    va_end(param_list);
    
    /* Unlock the elog mutex. */
    eos_mutex_take(&elog_mutex, ELOG_MUTEX_TIMEOUT);
    
    /* Check the log device has not same name with the former devices. */
    elog_device_t *next = dev_list;
    while (next != EOS_NULL)
    {
        next->out(buff);
        next->flush();
        next = next->next;
    }
    
    /* Unlock the elog mutex. */
    eos_mutex_release(&elog_mutex);
    
    eos_hw_interrupt_disable();

    while (1)
    {
    }
}

/* private function --------------------------------------------------------- */
/**
 * @brief   Translate the current time to elog time.
 * @param   time_ms is the current time in millisecond.
 * @param   log_time elog time output.
 * @return  None.
 */
static void _elog_time(eos_u32_t time_ms, elog_time_t *log_time)
{
    log_time->ms = (eos_u32_t)(time_ms % 1000);
    log_time->second = (time_ms / 1000) % 60;
    log_time->minute = (time_ms / 60000) % 60;
    log_time->hour = (time_ms / 3600000) % 24;
}

/**
 * @brief   The string hash algorithm.
 * @param   string is the input string.
 * @return  The output hash value.
 */
static eos_u32_t _hash_time33(const char *string)
{
    eos_u32_t hash = 5381;
    while (*string)
    {
        hash += (hash << 5) + (*string ++);
    }

    return (eos_u32_t)(hash & INT32_MAX);
}

/**
 * @brief   Insert the string into the hash table.
 */
static eos_u16_t _hash_insert(const char *string)
{
    eos_u16_t index = 0;

    /* Calculate the hash value of the string. */
    eos_u32_t hash_value = _hash_time33(string);
    eos_u16_t index_init = hash_value % hash.prime_max;

    for (eos_u16_t i = 0; i < (ELOG_MAX_OBJECTS / 2 + 1); i ++)
    {
        for (eos_s8_t j = -1; j <= 1; j += 2)
        {
            index = index_init + i * j + 2 * (eos_s16_t)ELOG_MAX_OBJECTS;
            index %= ELOG_MAX_OBJECTS;

            /* Find the empty object. */
            if (hash.obj[index].tag[0] == 0)
            {
                strcpy(hash.obj[index].tag, string);
                return index;
            }
            if (strcmp(hash.obj[index].tag, string) != 0)
            {
                continue;
            }
            
            return index;
        }
    }

    /* If this assert is trigged, you need to enlarge the hash table size. */
    EOS_ASSERT(0);
    
    return 0;
}

/**
 * @brief   Get the index of the string in the hash table.
 */
static eos_u16_t _hash_get_index(const char *string)
{
    eos_u16_t ret = ELOG_MAX_OBJECTS;

    /* Calculate the hash value of the string. */
    eos_u32_t hash_value = _hash_time33(string);
    eos_u16_t index_init = hash_value % hash.prime_max;
    eos_u16_t index = 0;

    for (eos_u16_t i = 0; i < (ELOG_MAX_OBJECTS / 2 + 1); i ++)
    {
        for (eos_s8_t j = -1; j <= 1; j += 2)
        {
            index = index_init + i * j + 2 * (eos_s16_t)ELOG_MAX_OBJECTS;
            index %= ELOG_MAX_OBJECTS;

            if (hash.obj[index].tag[0] != 0 &&
                strcmp(hash.obj[index].tag, string) == 0)
            {
                ret = index;
                goto exit;
            }
        }
    }
    
exit:
    return ret;
}

/**
 * @brief   The internal elog print function.
 * @param   tag the tag name.
 * @param   level log level input.
 * @param   linefeed_en line feed is enabled or not.
 * @param   s_format string format input.
 * @param   param_list is the parameters list.
 */
static void _printf(const char *tag,
                    eos_u8_t level,
                    bool linefeed_en,
                    const char * s_format,
                    va_list *param_list)
{
    eos_s32_t len = 0;
    eos_s32_t count = 0;
    elog_device_t *node = EOS_NULL;
    bool valid = false;

    /* Lock the elog mutex. */
    eos_mutex_take(&elog_mutex, ELOG_MUTEX_TIMEOUT);
    if (elog.enable == false)
    {
        goto exit;
    }
    if (elog.level > level)
    {
        goto exit;
    }
    /* If the tag is not enabled. */
    if (_tag_is_enabled(tag) == false)
    {
        goto exit;
    }
    /* If no output device */
    if (dev_list == EOS_NULL)
    {
        goto exit;
    }

    if (linefeed_en == true)
    {
        valid = false;
        
        if (_hash_get_index(tag) != ELOG_MAX_OBJECTS)
        {
            valid = (elog.mode == eLogMode_BlackList) ? false : true;
        }
    }

    eos_u8_t color = (level == (eLogLevel_Error + 1) ? eLogLevel_Debug : level);
    if (elog.color != color)
    {
        elog.color = color;

        /* Output the buffer data if device is ready. */
        node = dev_list;
        while (node != EOS_NULL)
        {
            if (node->level <= level && node->enable)
            {
                node->out((char *)string_color_log[color]);
            }
            node = node->next;
        }
    }

    memset(buff, 0, ELOG_SIZE_LOG);
    if (linefeed_en == true)
    {
        eos_u32_t time_ms = eos_tick_get_ms();
#if (ELOG_USE_TIME_SIMPLE != 0)
        len = sprintf(buff,"[%s/%s %06d] ",
                        string_level_log[level], tag, time_ms);
#else
        _elog_time(time_ms, &time);
        len = sprintf(buff,"[%s/%s %02d-%02d-%02d-%03d] ",
                        string_level_log[level], tag,
                        time.hour, time.minute, time.second, time.ms);
#endif
    }
    count = vsnprintf(&buff[len],
                        (ELOG_SIZE_LOG - 3 - len),
                        s_format, *param_list);
    len += count;

    if (linefeed_en == true)
    {
#if (ELOG_LINE_FEED != 0)
        buff[len ++] = '\r';
#endif
        buff[len ++] = '\n';
    }
    
    node = dev_list;
    while (node != EOS_NULL)
    {
        if (node->level <= level && node->enable)
        {
            node->out(buff);
        }
        node = node->next;
    }

exit:
    eos_mutex_release(&elog_mutex);      /* Unlock the elog mutex. */
}

/**
 * @brief   Check the tag is in the white list or not in the black list.
 * @param   tag the tag name.
 */
static bool _tag_is_enabled(const char *tag)
{
    bool existent = true;
    eos_u16_t index = _hash_get_index(tag);
    if (index == ELOG_MAX_OBJECTS)
    {
        existent = false;
    }

    return (elog.mode == eLogMode_BlackList) ? !existent : existent;
}

#ifdef __cplusplus
}
#endif

/* ----------------------------- end of file -------------------------------- */
