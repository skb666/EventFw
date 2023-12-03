/* include ------------------------------------------------------------------ */
#include "eos_led.h"
#include "eventos.h"
#include "event_def.h"
#include "stm32g0xx_hal.h"
#include <stdio.h>
#include "elab_export.h"
#include "elab_log.h"

/* data structure ----------------------------------------------------------- */
typedef struct eos_reactor_led_tag {
    eos_reactor_t super;

    eos_u8_t status;
} eos_reactor_led_t;

eos_reactor_led_t actor_led;

/* static event handler ----------------------------------------------------- */
static void led_e_handler(eos_reactor_led_t * const me, eos_event_t const * const e);

/* api ---------------------------------------------------- */
void eos_reactor_led_init(void)
{
    eos_reactor_init(&actor_led.super, 2, EOS_NULL);
    eos_reactor_start(&actor_led.super, EOS_HANDLER_CAST(led_e_handler));

    actor_led.status = 0;

#if (EOS_USE_PUB_SUB != 0)
    eos_event_sub((eos_actor_t *)(&actor_led), Event_Time_1000ms);
#endif
#if (EOS_USE_TIME_EVENT != 0)
    eos_event_pub_period(Event_Time_1000ms, 1000);
#endif
}
INIT_EXPORT(eos_reactor_led_init, EXPORT_APP);

/* static state function ---------------------------------------------------- */
static void led_e_handler(eos_reactor_led_t * const me, eos_event_t const * const e)
{
    if (e->topic == Event_Time_1000ms) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_9);
        ELAB_LOG_PRINTF("led status: %d\r\n", me->status);
        me->status = (me->status == 0) ? 1 : 0;
    }
}


