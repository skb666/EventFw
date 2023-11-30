/*
 * eLab Project
 * Copyright (c) 2023, EventOS Team, <event-os@outlook.com>
 */

#ifndef ELAB_DEF_H
#define ELAB_DEF_H

/* includes ----------------------------------------------------------------- */
#include <stdint.h>
#include "elab_config.h"

#if (ELAB_QPC_EN != 0)
#include "../3rd/qpc/include/qpc.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* public typedef ----------------------------------------------------------- */
typedef enum elab_err
{
    ELAB_OK                             = 0,
    ELAB_ERROR                          = -1,
    ELAB_ERR_EMPTY                      = -2,
    ELAB_ERR_FULL                       = -3,
    ELAB_ERR_TIMEOUT                    = -4,
    ELAB_ERR_BUSY                       = -5,
    ELAB_ERR_NO_MEMORY                  = -6,
    ELAB_ERR_IO                         = -7,
    ELAB_ERR_INVALID                    = -8,
    ELAB_ERR_MEM_OVERLAY                = -9,
    ELAB_ERR_MALLOC                     = -10,
    ELAB_ERR_NOT_ENOUGH                 = -11,
    ELAB_ERR_NO_SYSTEM                  = -12,
    ELAB_ERR_BUS                        = -13,
} elab_err_t;

typedef struct elab_date
{
    uint32_t year               : 16;
    uint32_t month              : 8;
    uint32_t day                : 8;
} elab_date_t;

typedef struct elab_time
{
    uint8_t hour;
    uint8_t minute;
    uint16_t second             : 6;
    uint16_t ms                 : 10;
} elab_time_t;

#if defined(__x86_64__) || defined(__aarch64__)
typedef int64_t                         elab_pointer_t;
#elif defined(__i386__) || defined(__arm__)
typedef int32_t                         elab_pointer_t;
#else
    #error The currnet CPU is NOT supported!
#endif

#if (ELAB_QPC_EN != 0)

typedef struct elab_event
{
    QEvt super;
    uint8_t data[ELAB_EVENT_DATA_SIZE];
} elab_event_t;

#endif

/**
 * Cast a member of a structure out to the containing structure.
 * @ptr:    the pointer to the member.
 * @type:   the type of the container struct this is embedded in.
 * @member: the name of the member within the struct.
 */
#ifndef container_of
#define container_of(pointer, type, member)                                    \
    ({                                                                         \
        void *__pointer = (void *)(pointer);                                   \
        ((type *)(__pointer - offsetof(type, member)));                        \
    })
#endif

#ifndef offsetof
#define offsetof(type, member)          ((elab_pointer_t)&((type *)0)->member)
#endif

/* Compiler Related Definitions */
#if defined(__CC_ARM) || defined(__CLANG_ARM) /* ARM Compiler */

    #include <stdarg.h>
    #define ELAB_SECTION(x)             __attribute__((section(x)))
    #define ELAB_USED                   __attribute__((used))
    #define ELAB_ALIGN(n)               __attribute__((aligned(n)))
    #define ELAB_WEAK                   __attribute__((weak))
    #define elab_inline                 static __inline

#elif defined (__IAR_SYSTEMS_ICC__)           /* for IAR Compiler */

    #include <stdarg.h>
    #define ELAB_SECTION(x)             @ x
    #define ELAB_USED                   __root
    #define ELAB_PRAGMA(x)              _Pragma(#x)
    #define ELAB_ALIGN(n)               ELAB_PRAGMA(data_alignment=n)
    #define ELAB_WEAK                   __weak
    #define elab_inline                 static inline

#elif defined (__GNUC__)                      /* GNU GCC Compiler */

    #include <stdarg.h>
    #define ELAB_SECTION(x)             __attribute__((section(x)))
    #define ELAB_USED                   __attribute__((used))
    #define ELAB_ALIGN(n)               __attribute__((aligned(n)))
    #define ELAB_WEAK                   __attribute__((weak))
    #define elab_inline                 static inline

#else
    #error The current compiler is NOT supported!
#endif

#ifdef __cplusplus
}
#endif

#endif

/* ----------------------------- end of file -------------------------------- */
