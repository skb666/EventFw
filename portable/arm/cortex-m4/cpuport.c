
#include <eos.h>

#if               /* ARMCC */ (  (defined ( __CC_ARM ) && defined ( __TARGET_FPU_VFP ))    \
                  /* Clang */ || (defined ( __clang__ ) && defined ( __VFP_FP__ ) && !defined(__SOFTFP__)) \
                  /* IAR */   || (defined ( __ICCARM__ ) && defined ( __ARMVFP__ ))        \
                  /* GNU */   || (defined ( __GNUC__ ) && defined ( __VFP_FP__ ) && !defined(__SOFTFP__)) )
#define USE_FPU   1
#else
#define USE_FPU   0
#endif

/* exception and interrupt handler table */
eos_u32_t eos_interrupt_from_thread;
eos_u32_t eos_interrupt_to_thread;
eos_u32_t eos_thread_switch_interrupt_flag;

struct exception_stack_frame
{
    eos_u32_t r0;
    eos_u32_t r1;
    eos_u32_t r2;
    eos_u32_t r3;
    eos_u32_t r12;
    eos_u32_t lr;
    eos_u32_t pc;
    eos_u32_t psr;
};

struct stack_frame
{
#if USE_FPU
    eos_u32_t flag;
#endif /* USE_FPU */

    /* r4 ~ r11 register */
    eos_u32_t r4;
    eos_u32_t r5;
    eos_u32_t r6;
    eos_u32_t r7;
    eos_u32_t r8;
    eos_u32_t r9;
    eos_u32_t r10;
    eos_u32_t r11;

    struct exception_stack_frame exception_stack_frame;
};

struct exception_stack_frame_fpu
{
    eos_u32_t r0;
    eos_u32_t r1;
    eos_u32_t r2;
    eos_u32_t r3;
    eos_u32_t r12;
    eos_u32_t lr;
    eos_u32_t pc;
    eos_u32_t psr;

#if USE_FPU
    /* FPU register */
    eos_u32_t S0;
    eos_u32_t S1;
    eos_u32_t S2;
    eos_u32_t S3;
    eos_u32_t S4;
    eos_u32_t S5;
    eos_u32_t S6;
    eos_u32_t S7;
    eos_u32_t S8;
    eos_u32_t S9;
    eos_u32_t S10;
    eos_u32_t S11;
    eos_u32_t S12;
    eos_u32_t S13;
    eos_u32_t S14;
    eos_u32_t S15;
    eos_u32_t FPSCR;
    eos_u32_t NO_NAME;
#endif
};

struct stack_frame_fpu
{
    eos_u32_t flag;

    /* r4 ~ r11 register */
    eos_u32_t r4;
    eos_u32_t r5;
    eos_u32_t r6;
    eos_u32_t r7;
    eos_u32_t r8;
    eos_u32_t r9;
    eos_u32_t r10;
    eos_u32_t r11;

#if USE_FPU
    /* FPU register s16 ~ s31 */
    eos_u32_t s16;
    eos_u32_t s17;
    eos_u32_t s18;
    eos_u32_t s19;
    eos_u32_t s20;
    eos_u32_t s21;
    eos_u32_t s22;
    eos_u32_t s23;
    eos_u32_t s24;
    eos_u32_t s25;
    eos_u32_t s26;
    eos_u32_t s27;
    eos_u32_t s28;
    eos_u32_t s29;
    eos_u32_t s30;
    eos_u32_t s31;
#endif

    struct exception_stack_frame_fpu exception_stack_frame;
};

eos_u8_t *eos_hw_stack_init(void       *tentry,
                             void       *parameter,
                             eos_u8_t *stack_addr,
                             void       *texit)
{
    struct stack_frame *stack_frame;
    eos_u8_t         *stk;
    unsigned long       i;

    stk  = stack_addr + sizeof(eos_u32_t);
    stk  = (eos_u8_t *)EOS_ALIGN_DOWN((eos_u32_t)stk, 8);
    stk -= sizeof(struct stack_frame);

    stack_frame = (struct stack_frame *)stk;

    /* init all register */
    for (i = 0; i < sizeof(struct stack_frame) / sizeof(eos_u32_t); i ++)
    {
        ((eos_u32_t *)stack_frame)[i] = 0xdeadbeef;
    }

    stack_frame->exception_stack_frame.r0  = (unsigned long)parameter; /* r0 : argument */
    stack_frame->exception_stack_frame.r1  = 0;                        /* r1 */
    stack_frame->exception_stack_frame.r2  = 0;                        /* r2 */
    stack_frame->exception_stack_frame.r3  = 0;                        /* r3 */
    stack_frame->exception_stack_frame.r12 = 0;                        /* r12 */
    stack_frame->exception_stack_frame.lr  = (unsigned long)texit;     /* lr */
    stack_frame->exception_stack_frame.pc  = (unsigned long)tentry;    /* entry point, pc */
    stack_frame->exception_stack_frame.psr = 0x01000000L;              /* PSR */

#if USE_FPU
    stack_frame->flag = 0;
#endif /* USE_FPU */

    /* return task's current stack address */
    return stk;
}

#define SCB_CFSR        (*(volatile const unsigned *)0xE000ED28) /* Configurable Fault Status Register */
#define SCB_HFSR        (*(volatile const unsigned *)0xE000ED2C) /* HardFault Status Register */
#define SCB_MMAR        (*(volatile const unsigned *)0xE000ED34) /* MemManage Fault Address register */
#define SCB_BFAR        (*(volatile const unsigned *)0xE000ED38) /* Bus Fault Address Register */
#define SCB_AIRCR       (*(volatile unsigned long *)0xE000ED0C)  /* Reset control Address Register */
#define SCB_RESET_VALUE 0x05FA0004                               /* Reset value, write to SCB_AIRCR can reset cpu */

#define SCB_CFSR_MFSR   (*(volatile const unsigned char*)0xE000ED28)  /* Memory-management Fault Status Register */
#define SCB_CFSR_BFSR   (*(volatile const unsigned char*)0xE000ED29)  /* Bus Fault Status Register */
#define SCB_CFSR_UFSR   (*(volatile const unsigned short*)0xE000ED2A) /* Usage Fault Status Register */

struct exception_info
{
    eos_u32_t exc_return;
    struct stack_frame stack_frame;
};

void eos_hw_hard_fault_exception(struct exception_info *exception_info)
{
    (void)exception_info;

    while (1);
}

#ifdef RT_USING_CPU_FFS
/**
 * This function finds the first bit set (beginning with the least significant bit)
 * in value and return the index of that bit.
 *
 * Bits are numbered starting at 1 (the least significant bit).  A return value of
 * zero from any of these functions means that the argument was zero.
 *
 * @return return the index of the first bit set. If value is 0, then this function
 * shall return 0.
 */
#if defined(__CC_ARM)
__asm int __eos_ffs(int value)
{
    CMP     r0, #0x00
    BEQ     exit

    RBIT    r0, r0
    CLZ     r0, r0
    ADDS    r0, r0, #0x01

exit
    BX      lr
}
#elif defined(__clang__)
int __eos_ffs(int value)
{
    __asm volatile(
        "CMP     r0, #0x00            \n"
        "BEQ     1f                   \n"

        "RBIT    r0, r0               \n"
        "CLZ     r0, r0               \n"
        "ADDS    r0, r0, #0x01        \n"

        "1:                           \n"

        : "=r"(value)
        : "r"(value)
    );
    return value;
}
#elif defined(__IAR_SYSTEMS_ICC__)
int __eos_ffs(int value)
{
    if (value == 0) return value;

    asm("RBIT %0, %1" : "=r"(value) : "r"(value));
    asm("CLZ  %0, %1" : "=r"(value) : "r"(value));
    asm("ADDS %0, %1, #0x01" : "=r"(value) : "r"(value));

    return value;
}
#elif defined(__GNUC__)
int __eos_ffs(int value)
{
    return __builtin_ffs(value);
}
#endif

#endif
