
#include "devf.h"
#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

void devf_port_evt_pub(int evt_id)
{

}

uint64_t devf_port_get_time_ms(void)
{
    struct timeval time_crt;
    gettimeofday(&time_crt, (void *)0);
    uint64_t time_crt_ms = time_crt.tv_sec * 1000 + time_crt.tv_usec / 1000;

    return time_crt_ms;
}

inline void devf_port_irq_disable(void)
{
    // NULL
}


inline void devf_port_irq_enable(void)
{
    // NULL
}

#ifdef __cplusplus    
}
#endif
