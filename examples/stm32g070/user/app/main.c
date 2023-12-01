/* include ------------------------------------------------------------------ */
#include "eventos.h"                                // EventOS Nano头文件
#include "event_def.h"                              // 事件主题的枚举
#include "elab_export.h"

/* define ------------------------------------------------------------------- */
#if (EOS_USE_PUB_SUB != 0)
static eos_u32_t eos_sub_table[Event_Max];          // 订阅表数据空间
#endif

void eos_module_init(void)
{
    eos_init();                                     // EventOS初始化

#if (EOS_USE_PUB_SUB != 0)
    eos_sub_init(eos_sub_table, Event_Max);         // 订阅表初始化
#endif
}
INIT_EXPORT(eos_module_init, EXPORT_MIDDLEWARE);

/* main function ------------------------------------------------------------ */
int main(void)
{
    module_init();                                  // 各模块初始化

    eos_run();                                      // EventOS启动

    return 0;
}
