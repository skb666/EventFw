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
#include "template.h"

#ifdef __cplusplus
extern "C" {
#endif

/* private define ----------------------------------------------------------- */
#define EOS_PRIVATE_MACRO                       (128)

/* private typedef ---------------------------------------------------------- */
/*
 * Commit.
 */
typedef struct eos_private_type
{
    eos_u32_t u32;
    eos_s32_t s32;
    eos_u16_t u16;
    eos_s16_t s16;
} eos_private_type_t;

/* private variables -------------------------------------------------------- */
/*
 * Commit.
 */
static eos_private_type_t types;

/* private function prototype ----------------------------------------------- */
static eos_u32_t _func_private(void *buff, eos_u32_t size);

/* public function ---------------------------------------------------------- */
/*
 * Commit.
 */
eos_s32_t eos_module_init(void)
{
    return 0;
}

/**
 * @brief   This function description.
 * @note    If nessary, write some note here.
 * @param   time is time.
 */
void eos_module_poll(eos_type_t *time)
{
    /* commit in function. */
    (void)time;
}

/**
 * @brief   This function description.
 * @note    If nessary, write some note here.
 */
void eos_module_end(void)
{

}

/* private function --------------------------------------------------------- */
/**
 * @brief   This function description.
 * @note    If nessary, write some note here.
 * 
 * @param   buff is buff.
 * @param   size is size.
 * 
 * @return  Return current cpu interrupt status.
 */
static eos_u32_t _func_private(void *buff, eos_u32_t size)
{
    /* commit in function. */
    (void)buff;
    (void)size;

    return 0;
}

#ifdef __cplusplus
}
#endif
