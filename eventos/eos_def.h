
/*
 * EventOS
 * Copyright (c) 2021, EventOS Team, <event-os@outlook.com>
 *
 * SPDX-License-Identifier: MIT
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the 'Software'), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS 
 * OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.event-os.cn
 * https://github.com/event-os/eventos.git
 * https://gitee.com/event-os/eventos.git
 * 
 * Change Logs:
 * Date           Author        Notes
 * 2022-02-20     DogMing       V0.0.2
 */

#ifndef EVENTOS_DEF_H__
#define EVENTOS_DEF_H__

/* basic data type ---------------------------------------------------------- */
typedef unsigned int                    eos_u32_t;
typedef signed int                      eos_s32_t;
typedef unsigned short                  eos_u16_t;
typedef signed short                    eos_s16_t;
typedef unsigned char                   eos_u8_t;
typedef signed char                     eos_s8_t;

typedef unsigned int                    eos_size_t;      /**< Type for size number */
typedef signed int                      eos_base_t;      /**< Nbit CPU related date type */
typedef unsigned int                    eos_ubase_t;     /**< Nbit unsigned CPU related data type */
typedef signed int                      eos_err_t;       /**< Type for error number */

typedef unsigned long long              eos_u64_t;
typedef signed long long                eos_s64_t;

typedef enum eos_bool
{
    EOS_False = 0,
    EOS_True = 1,
} eos_bool_t;

#define EOS_NULL                        ((void *)0)

#define EOS_U32_MAX                     (0xffffffffU)
#define EOS_U32_MIN                     (0U)

#define EOS_U16_MAX                     (0xffffU)
#define EOS_U16_MIN                     (0U)

#define EOS_HEAP_MAX                    (0x7fffU)

#define EOS_U8_MAX                      (0xffU)
#define EOS_U8_MIN                      (0U)

#define EOS_TICK_MAX                    EOS_U32_MAX     /**< Maximum number of tick */

#define EOS_HEAP_MAX                    (0x7fffU)

#define EOS_TIME_FOREVER                (-1)

#define EOS_UNUSED(x)                   ((void)x)

/* Compiler Related Definitions */
#if defined(__ARMCC_VERSION)           /* ARM Compiler */
    #include <stdarg.h>
    #define EOS_SECTION(x)              __attribute__((section(x)))
    #define EOS_USED                    __attribute__((used))
    #define ALIGN(n)                    __attribute__((aligned(n)))
    #define EOS_WEAK                    __attribute__((weak))
    #define eos_inline                  static __inline
    /* module compiling */
    #define RTT_API                     __declspec(dllexport)
#elif defined (__IAR_SYSTEMS_ICC__)     /* for IAR Compiler */
    #include <stdarg.h>
    #define EOS_SECTION(x)               @ x
    #define EOS_USED                     __root
    #define PRAGMA(x)                   _Pragma(#x)
    #define ALIGN(n)                    PRAGMA(data_alignment=n)
    #define EOS_WEAK                     __weak
    #define eos_inline                   static inline
    #define RTT_API
#elif defined (__GNUC__)                /* GNU GCC Compiler */
    /* the version of GNU GCC must be greater than 4.x */
    typedef __builtin_va_list           __gnuc_va_list;
    typedef __gnuc_va_list              va_list;
    #define va_start(v,l)               __builtin_va_start(v,l)
    #define va_end(v)                   __builtin_va_end(v)
    #define va_arg(v,l)                 __builtin_va_arg(v,l)
    #define EOS_SECTION(x)              __attribute__((section(x)))
    #define EOS_USED                    __attribute__((used))
    #define ALIGN(n)                    __attribute__((aligned(n)))
    #define EOS_WEAK                    __attribute__((weak))
    #define eos_inline                  static __inline
    #define RTT_API
#else
    #error not supported tool chain
#endif


/*
 * task state definitions
 */
typedef enum eos_task_state
{
    EOS_TASK_INIT                = 0x00,                /**< Initialized status */
    EOS_TASK_READY               = 0x01,                /**< Ready status */
    EOS_TASK_SUSPEND             = 0x02,                /**< Suspend status */
    EOS_TASK_RUNNING             = 0x03,                /**< Running status */
    EOS_TASK_CLOSE               = 0x04,                /**< Closed status */
    EOS_TASK_STAT_MASK           = 0x07,
} eos_task_state_t;

/* EventOS error code definitions */
#define EOS_EOK                         0               /**< There is no error */
#define EOS_ERROR                       -1              /**< A generic error happens */
#define EOS_ETIMEOUT                    -2              /**< Timed out */
#define EOS_EFULL                       -3              /**< The resource is full */
#define EOS_EEMPTY                      -4              /**< The resource is empty */
#define EOS_ENOMEM                      -5              /**< No memory */
#define EOS_ENOSYS                      -6              /**< No system */
#define EOS_EBUSY                       -7              /**< Busy */
#define EOS_EIO                         -8              /**< IO error */
#define EOS_EINTR                       -9              /**< Interrupted system call */
#define EOS_EINVAL                      -10             /**< Invalid argument */


/* maximum value of ipc type */
#define EOS_SEM_VALUE_MAX               EOS_U16_MAX     /**< Maximum number of semaphore .value */
#define EOS_MUTEX_VALUE_MAX             EOS_U16_MAX     /**< Maximum number of mutex .value */
#define EOS_MUTEX_HOLD_MAX              EOS_U8_MAX      /**< Maximum number of mutex .hold */
#define EOS_MB_ENTRY_MAX                EOS_U16_MAX     /**< Maximum number of mailbox .entry */
#define EOS_MQ_ENTRY_MAX                EOS_U16_MAX     /**< Maximum number of message queue .entry */

/**
 * @ingroup BasicDef
 *
 * @def EOS_ALIGN_DOWN(size, align)
 * Return the down number of aligned at specified width. EOS_ALIGN_DOWN(13, 4)
 * would return 12.
 */
#define EOS_ALIGN_DOWN(size, align)     ((size) & ~((align) - 1))

#endif
