/*
 * EventOS Template V0.2
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
 * 2021-11-23     Dog           the first version
 * 2022-08-27     Dog           V0.2
 */

/* include ------------------------------------------------------------------ */
#include "elog_test.h"
#include "elog.h"
#include "eos.h"
#include <stdio.h>
#include <string.h>

EOS_TAG("ElogTest")

#ifdef __cplusplus
extern "C" {
#endif

/* private define ----------------------------------------------------------- */
#define ELOG_TEST_STACKS_ZIE                    (2048)
#define ELOG_ESTREAM_CAPACITY                   (102400)

/* private typedef ---------------------------------------------------------- */


/* private variables -------------------------------------------------------- */
static uint32_t stack_elog_test[ELOG_TEST_STACKS_ZIE / 4];
static eos_task_t task_elog_test;

/* private function prototype ----------------------------------------------- */
static void task_entry_elog_test(void *paras);

/* win32 printf device interface functions */
static void elog_dev_win32_printf_init(void);
static void elog_dev_win32_printf_flush(void);
static void elog_dev_win32_printf_out(const char *data);

/* event stream device interface functions */
static void elog_dev_estream_init(void);
static void elog_dev_estream_flush(void);
static void elog_dev_estream_out(const char *data);

/* private variables -------------------------------------------------------- */
/* win32 printf device */
static elog_device_t dev_win32_printf =
{
    NULL, "win32_printf", eLogLevel_Debug, true,
    elog_dev_win32_printf_out,
    elog_dev_win32_printf_flush,
    elog_dev_win32_printf_init,
};

/* Event stream device */
static elog_device_t dev_estream =
{
    NULL, "estream", eLogLevel_Debug, true,
    elog_dev_estream_out,
    elog_dev_estream_flush,
    elog_dev_estream_init,
};

/* public function ---------------------------------------------------------- */
void elog_test_start(void)
{
    eos_db_register("dev_estream",
                    ELOG_ESTREAM_CAPACITY,
                    EOS_DB_ATTRIBUTE_STREAM);
    eos_task_init(&task_elog_test, "elog_test", task_entry_elog_test,
                    NULL, stack_elog_test, sizeof(stack_elog_test), 0);
    eos_task_startup(&task_elog_test);
}

/* private function --------------------------------------------------------- */
/**
 * @brief   elog test function.
 */
static void task_entry_elog_test(void *paras)
{
    elog_err_t ret = Elog_OK;

    /* Initialize the elog module and enable it. */
    elog_init();
    elog_set_level(eLogLevel_Debug);

    /* Register win32-printf and estream device to elog. */
    elog_device_register(&dev_win32_printf);
    // elog_device_register(&dev_estream);

    /* Enable elog module and its devices. */
    elog_enable(true);
    elog_device_enable("win32_printf", true);
    // elog_device_enable("estream", true);

    /* Print functions testing for eveny function. */
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    /* elog_err_t testing for every function. */
    printf("\033[0m--------------------- testing elog_enable\n");
    elog_enable(false);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    elog_enable(true);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------------- testing elog_set_level\n");
    printf("\033[0m--------------- eLogLevel_Error\n");
    elog_set_level(eLogLevel_Error);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------- eLogLevel_Warn\n");
    elog_set_level(eLogLevel_Warn);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------- eLogLevel_Info\n");
    elog_set_level(eLogLevel_Info);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------- eLogLevel_Debug\n");
    elog_set_level(eLogLevel_Debug);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------------- testing elog_set_mode\n");
    printf("\033[0m--------------- eLogMode_WhiteList\n");
    elog_set_mode(eLogMode_WhiteList);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------- eLogMode_BlackList\n");
    elog_set_mode(eLogMode_BlackList);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------------- testing elog_device_remove\n");
    elog_device_remove(&dev_win32_printf);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    elog_enable(false);
    elog_device_register(&dev_win32_printf);
    elog_enable(true);
    elog_device_enable("win32_printf", true);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------------- testing elog_device_set_level\n");
    elog_device_set_level("win32_printf", eLogLevel_Error);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------- eLogLevel_Warn\n");
    elog_device_set_level("win32_printf", eLogLevel_Warn);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------- eLogLevel_Info\n");
    elog_device_set_level("win32_printf", eLogLevel_Info);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------- eLogLevel_Debug\n");
    elog_device_set_level("win32_printf", eLogLevel_Debug);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------------- testing elog_device_enable\n");
    elog_device_enable("win32_printf", false);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    elog_device_enable("win32_printf", true);
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------------- testing elog_tag_add\n");
    elog_tag_add("ElogTest");
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------------- testing elog_tag_remove\n");
    elog_tag_remove("ElogTest");
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m--------------------- testing elog_tag_clear_all\n");
    elog_tag_add("ElogTest");
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    elog_tag_clear_all();
    elog_print("test\n");
    elog_debug("test");
    elog_info("test");
    elog_warn("test");
    elog_error("test");

    printf("\033[0m------------------- elog testing end -------------------\n");

    eos_task_exit();
}

static void elog_dev_win32_printf_init(void)
{
}

static void elog_dev_win32_printf_flush(void)
{
    fflush(stdout);
}

static void elog_dev_win32_printf_out(const char *data)
{
    printf("%s", data);
}

static void elog_dev_estream_init(void)
{
}

static void elog_dev_estream_flush(void)
{
}

static void elog_dev_estream_out(const char *data)
{
    eos_db_stream_write("dev_estream", (void *)data, strlen(data));
}

#ifdef __cplusplus
}
#endif
