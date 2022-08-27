
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

#ifndef __EOS_TEMPLATE_H__
#define __EOS_TEMPLATE_H__

/* include ------------------------------------------------------------------ */
#include "eos_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* public define ------------------------------------------------------------ */
/*
 * Commit.
 */
#define EOS_MACRO                           (128)

/* public typedef ----------------------------------------------------------- */
/*
 * Commit.
 */
typedef struct eos_type
{
    eos_u32_t u32;
    eos_s32_t s32;
    eos_u16_t u16;
    eos_s16_t s16;
} eos_type_t;

/*
 * Commit.
 */
typedef enum eos_enum
{
    EosEnum_Tip0 = 0,
    EosEnum_Tip1,
    EosEnum_Tip2,

    EosEnum_Maxd
} eos_enum_t;

/*
 * Commit.
 */
typedef eos_s32_t (* eos_hook_t)(eos_type_t *me, void *data, eos_u32_t size);

/* public function ---------------------------------------------------------- */
eos_s32_t eos_module_init(void);
void eos_module_poll(eos_type_t *time);
void eos_module_end(void);

#ifdef __cplusplus
}
#endif

#endif  /* __EOS_TEMPLATE_H__ */
