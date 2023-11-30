/*
 * eLab Project
 * Copyright (c) 2023, EventOS Team, <event-os@outlook.com>
 */

/* include ------------------------------------------------------------------ */
#include <stdio.h>
#include <stdarg.h>
#include "elab_log.h"
#include "elab_assert.h"
#include "elab_common.h"
#include "elab_config.h"
#if (ELAB_RTOS_CMSIS_OS_EN != 0)
#include "../os/cmsis_os.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* private config ----------------------------------------------------------- */
#define ELAB_LOG_BUFF_SIZE                          (256)

/* private defines ---------------------------------------------------------- */
/* define printf color */
#define NONE                                        "\033[0;0m"
#define LIGHT_RED                                   "\033[1;31m"
#define YELLOW                                      "\033[0;33m"
#define LIGHT_BLUE                                  "\033[1;34m"
#define GREEN                                       "\033[0;32m"

/* private variables -------------------------------------------------------- */
#if (ELAB_RTOS_CMSIS_OS_EN != 0)
static const osMutexAttr_t mutex_attr_elog =
{
    "mutex_elog", osMutexRecursive | osMutexPrioInherit, NULL, 0U 
};
#endif

/* public function prototype ------------------------------------------------ */
static uint8_t elog_level = ELOG_LEVEL_DEBUG;

#if (ELAB_RTOS_CMSIS_OS_EN != 0)
static osMutexId_t mutex_elog = NULL;
#endif

#if (ELOG_COLOR_ENABLE != 0)
static const char *const elog_color_table[ELOG_LEVEL_MAX] =
{
    NONE, LIGHT_RED, YELLOW, LIGHT_BLUE, GREEN,
};
#endif

static const char elog_level_lable[ELOG_LEVEL_MAX] =
{
    ' ', 'E', 'W', 'I', 'D',
};

static char _buff[ELAB_LOG_BUFF_SIZE];

/* public function ---------------------------------------------------------- */
void elog_level_set(uint8_t level)
{
    elog_level = level;
}

void _elog_printf(const char *name, uint8_t level, const char * s_format, ...)
{
#if (ELAB_RTOS_CMSIS_OS_EN != 0)
    if (mutex_elog == NULL)
    {
        mutex_elog = osMutexNew(&mutex_attr_elog);
    }
    osMutexAcquire(mutex_elog, osWaitForever);
#endif

    if (elog_level >= level)
    {
#if (ELAB_RTOS_CMSIS_OS_EN == 0 && ELAB_RTOS_BASIC_OS_EN == 0)
        printf("%s[%c/%s %u] ", elog_color_table[level],
                                elog_level_lable[level], name, elab_time_ms());
        va_list param_list;
        va_start(param_list, s_format);
        int count = vsnprintf(_buff, (ELAB_LOG_BUFF_SIZE - 1), s_format, param_list);
        va_end(param_list);
        _buff[count] = 0;
        printf("%s", _buff);
        printf("\r\n");
#endif
#if (ELAB_RTOS_CMSIS_OS_EN != 0 || ELAB_RTOS_BASIC_OS_EN != 0)
        printf("%s[%c/%s %u] ", elog_color_table[level],
                                elog_level_lable[level], name, elab_time_ms());
        va_list param_list;
        va_start(param_list, s_format);
        int count = vsnprintf(_buff, (ELAB_LOG_BUFF_SIZE - 1), s_format, param_list);
        va_end(param_list);
        _buff[count] = 0;
        printf("%s", _buff);
        printf(NONE "\r\n");
#endif
    }
    
#if (ELAB_RTOS_CMSIS_OS_EN != 0)
    osMutexRelease(mutex_elog);
#endif
}

#if !defined(__x86_64__)

#if defined(__CC_ARM) || defined(__CLANG_ARM)
int fputc(int ch,FILE *p)
{
    char _ch = (char)ch;
    elab_debug_uart_send(&_ch, 1);
    
    return ch;
}
#else
/* TODO Redirect printf. */
int _write(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;

    return 0;
}
#endif

#ifdef __cplusplus
}
#endif

#endif

/* ----------------------------- end of file -------------------------------- */
