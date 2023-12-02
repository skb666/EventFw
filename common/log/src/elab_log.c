#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "elab_log.h"
#include "elab_log_port.h"


#if ELAB_LOG_TIMESTAMP_EN > 0x00u
/* timestamp-level-tag[file:line(func)]: body */
#define ELAB_LOG_FORMAT "%10d""-""%c""-""%s""[%s:%d(%s)]: "//"%s""\n"
#else
/* level-tag[file:line(func)]: body */
#define ELAB_LOG_FORMAT "%c""-""%s""[%s:%d(%s)]: "//"%s""\n"
#endif

#define ELAB_LOG_FILTER_ALL_LEVEL (BIT(ELAB_LOG_INFO) | BIT(ELAB_LOG_DEBUG) | BIT(ELAB_LOG_WARN) | BIT(ELAB_LOG_ERROR))

static const char s_level_c[ELAB_LOG_LEVEL_NUM] = {'I', 'D', 'W', 'E'};

static struct
{
    volatile bool en;
    volatile bool lock;
    uint8_t filter;
}s_ctrl_block = {0};

static void _lock(void);
static void _unlock(void);

void elab_log_init(void)
{
    s_ctrl_block.en = false;
    s_ctrl_block.lock = false;
    s_ctrl_block.filter = ELAB_LOG_FILTER_ALL_LEVEL;
}

void elab_log_enable(void)
{
    s_ctrl_block.en = true;
}

void elab_log_disable(void)
{
    s_ctrl_block.en = false;
}

bool elab_log_en_get(void)
{
    return s_ctrl_block.en;
}

static void _lock(void)
{
    s_ctrl_block.lock = true;
}

static void _unlock(void)
{
    s_ctrl_block.lock = false;
}

void elab_log_filter_set(elab_log_level_t filter)
{
    s_ctrl_block.filter |= BIT(filter);
}

void elab_log_filter_clear(elab_log_level_t filter)
{
    s_ctrl_block.filter &= ~BIT(filter);
}

int32_t elab_log_extend_printf(elab_log_level_t level, const char *tag, const char *filename, uint16_t line, const char *func, const char *msg_format, ...)
{
    char buf[ELAB_LOG_PACKAGE_MAX] = {0};
    uint8_t buf_pos = 0;
    int32_t ret = 0;
    va_list va;

    if (!s_ctrl_block.en)
    {
        return -1;
    }

    if (s_ctrl_block.lock)
    {
        return -1;
    }

    if (!(s_ctrl_block.filter & BIT((uint8_t)level)))
    {
        return 0;
    }

    _lock();
    elab_log_enter_critical_zone();

#if ELAB_LOG_TIMESTAMP_EN > 0x00u
    snprintf(buf, ELAB_LOG_PACKAGE_MAX, ELAB_LOG_FORMAT, elab_log_timestamp_get(), s_level_c[level], tag, filename, line, func);//, body);
#else
    snprintf(buf, ELAB_LOG_PACKAGE_MAX, ELAB_LOG_FORMAT, s_level_c[level], tag, filename, line, func);//, body);
#endif

    buf_pos = strlen(buf);

    va_start(va, msg_format);
    vsnprintf(&buf[buf_pos], ELAB_LOG_PACKAGE_MAX - buf_pos, msg_format, va);
    va_end(va);

    buf_pos = strlen(buf);

    if (buf_pos < ELAB_LOG_PACKAGE_MAX)
    {
        buf[buf_pos++] = '\n';
    }

    ret = elab_log_port_output(buf, buf_pos);

    elab_log_exit_critical_zone();
    _unlock();

    return ret;
}

int32_t elab_log_printf(const char *msg_format, ...)
{
    int32_t ret = 0;
    va_list va;
    char buf[ELAB_LOG_PACKAGE_MAX] = {0};
    uint8_t buf_pos = 0;

    if (!s_ctrl_block.en)
    {
        return -1;
    }

    if (s_ctrl_block.lock)
    {
        return -1;
    }

    _lock();
    elab_log_enter_critical_zone();

    va_start(va, msg_format);
    vsnprintf(buf, ELAB_LOG_PACKAGE_MAX, msg_format, va);
    va_end(va);

    buf_pos = strlen(buf);

    if (buf_pos < ELAB_LOG_PACKAGE_MAX)
    {
        buf[buf_pos++] = '\n';
    }

    ret = elab_log_port_output(buf, buf_pos);

    elab_log_exit_critical_zone();
    _unlock();

    return ret;
}
