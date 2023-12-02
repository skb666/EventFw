#ifndef ELAB_LOG_H_
#define ELAB_LOG_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "elab_log_cfg.h"


#ifndef __FILENAME__
#if defined(__GNUC__)
#define __FILENAME__ (__builtin_strrchr (__FILE__, '/') ? __builtin_strrchr (__FILE__, '/') + 1 : __FILE__)
#else /* !defined(__GNUC__) */
#include <string.h>
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif /* defined(__GNUC__) */
#endif /* __FILENAME__ */

#ifndef BIT
#define BIT(bit)                 (1UL << (bit))
#endif

#define ELAB_TAG(_tag)                  static const char *TAG = _tag

#if ELAB_LOG_EN > 0x00u
#define ELAB_LOG_EXTEND_PRINTF(level, tag, fmt, ...) elab_log_extend_printf(level, tag, __FILENAME__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define ELAB_LOG_PRINTF(fmt, ...) elab_log_printf(fmt, ##__VA_ARGS__)
#else
#define ELAB_LOG_EXTEND_PRINTF(level, module, fmt, ...)
#define ELAB_LOG_PRINTF(fmt, ...)
#endif
#define ELAB_LOG_I(tag, fmt, ...) ELAB_LOG_EXTEND_PRINTF(ELAB_LOG_INFO , tag, fmt, ##__VA_ARGS__)
#define ELAB_LOG_D(tag, fmt, ...) ELAB_LOG_EXTEND_PRINTF(ELAB_LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#define ELAB_LOG_W(tag, fmt, ...) ELAB_LOG_EXTEND_PRINTF(ELAB_LOG_WARN , tag, fmt, ##__VA_ARGS__)
#define ELAB_LOG_E(tag, fmt, ...) ELAB_LOG_EXTEND_PRINTF(ELAB_LOG_ERROR, tag, fmt, ##__VA_ARGS__)


typedef enum _elab_log_level_
{
    ELAB_LOG_INFO = (uint8_t)0x00,
    ELAB_LOG_DEBUG               ,
    ELAB_LOG_WARN                ,
    ELAB_LOG_ERROR               ,
    ELAB_LOG_LEVEL_NUM           ,
}elab_log_level_t;

extern void elab_log_init(void);
extern void elab_log_enable(void);
extern void elab_log_disable(void);
extern bool elab_log_en_get(void);
extern void elab_log_filter_set(elab_log_level_t filter);
extern void elab_log_filter_clear(elab_log_level_t filter);
extern int32_t elab_log_extend_printf(elab_log_level_t level, const char *tag, const char *filename, uint16_t line, const char *func, const char *msg_format, ...);
extern int32_t elab_log_printf(const char *msg_format, ...);

#endif
