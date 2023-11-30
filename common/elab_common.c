/*
 * eLab Project
 * Copyright (c) 2023, EventOS Team, <event-os@outlook.com>
 */

/* include ------------------------------------------------------------------ */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "elab_common.h"
#include "elab_def.h"

#if defined(__linux__)
#include <unistd.h>
#include <termios.h>
#endif

#if defined(_WIN32)
#include <conio.h>
#include "../os/cmsis_os.h"

/* private defines ---------------------------------------------------------- */
#define KEY_CTRL_C                  (0x03)          // Value of the CTRL+C key
#define KEY_Fn                      (0x00)          // The 1st byte of F1 - F12
#define KEY_FUNCTION                (0xE0)

/* private typedef -----------------------------------------------------------*/
typedef struct spec_func_key_data
{
    uint8_t count;
    uint8_t v_linux[5];
    uint8_t v_win_terminal[2];
} spec_func_key_data_t;

/* private variables -------------------------------------------------------- */
static const spec_func_key_data_t _sf_key_data[] =
{
    { 3, { 0x1B, 0x4F, 0x50, 0x00, 0x00 }, { 0x00, 0x3B } },    /* F1 */
    { 3, { 0x1B, 0x4F, 0x51, 0x00, 0x00 }, { 0x00, 0x3C } },    /* F2 */
    { 3, { 0x1B, 0x4F, 0x52, 0x00, 0x00 }, { 0x00, 0x3D } },    /* F3 */
    { 3, { 0x1B, 0x4F, 0x53, 0x00, 0x00 }, { 0x00, 0x3E } },    /* F4 */
    { 5, { 0x1B, 0x5B, 0x31, 0x35, 0x7E }, { 0x00, 0x3F } },    /* F5 */
    { 5, { 0x1B, 0x5B, 0x31, 0x37, 0x7E }, { 0x00, 0x40 } },    /* F6 */
    { 5, { 0x1B, 0x5B, 0x31, 0x38, 0x7E }, { 0x00, 0x41 } },    /* F7 */
    { 5, { 0x1B, 0x5B, 0x31, 0x39, 0x7E }, { 0x00, 0x42 } },    /* F8 */
    { 5, { 0x1B, 0x5B, 0x32, 0x30, 0x7E }, { 0x00, 0x43 } },    /* F9 */
    { 5, { 0x1B, 0x5B, 0x32, 0x31, 0x7E }, { 0x00, 0x44 } },    /* F10 */
    { 5, { 0x1B, 0x5B, 0x32, 0x32, 0x7E }, { 0x00, 0xFF } },    /* F11 */
    { 5, { 0x1B, 0x5B, 0x32, 0x33, 0x7E }, { 0xE0, 0x86 } },    /* F12 */

    { 3, { 0x1B, 0x5B, 0x41, 0x00, 0x00 }, { 0xE0, 0x48 } },    /* UP */
    { 3, { 0x1B, 0x5B, 0x42, 0x00, 0x00 }, { 0xE0, 0x50 } },    /* DOWN */
    { 3, { 0x1B, 0x5B, 0x43, 0x00, 0x00 }, { 0xE0, 0x4B } },    /* LEFT */
    { 3, { 0x1B, 0x5B, 0x44, 0x00, 0x00 }, { 0xE0, 0x4D } },    /* RIGHT */

    { 4, { 0x1B, 0x5B, 0x31, 0x7E, 0x00 }, { 0xE0, 0x47 } },    /* HOME */
    { 4, { 0x1B, 0x5B, 0x32, 0x7E, 0x00 }, { 0xE0, 0x52 } },    /* INSERT */
    { 4, { 0x1B, 0x5B, 0x33, 0x7E, 0x00 }, { 0xE0, 0x53 } },    /* DELECT */
    { 4, { 0x1B, 0x5B, 0x34, 0x7E, 0x00 }, { 0xE0, 0x4F } },    /* END */
    { 4, { 0x1B, 0x5B, 0x35, 0x7E, 0x00 }, { 0xE0, 0x49 } },    /* PAGEUP */
    { 4, { 0x1B, 0x5B, 0x36, 0x7E, 0x00 }, { 0xE0, 0x51 } },    /* PAGEDOWN */
};

static osMessageQueueId_t mq_getch = NULL;

/* private function prototypes -----------------------------------------------*/
static void _entry_getch(void *para);
#endif

#if defined(__linux__)
static int getch(void);
#endif

/* public function ---------------------------------------------------------- */
uint32_t elab_version(void)
{
    return 000100;
}

/**
  * @brief  eLab initialization exporting function.
  * @param  level       init level.
  * @retval None
  */
ELAB_WEAK uint32_t elab_time_ms(void)
{
#if !defined(__linux__)
#if (ELAB_RTOS_CMSIS_OS_EN != 0)
    return osKernelGetTickCount() * ELAB_RTOS_TICK_MS;
#elif (ELAB_RTOS_BASIC_OS_EN != 0)
    return bos_time();
#else
    return 0;
#endif
#else
    return osKernelGetTickCount();
#endif
}

#if !defined(__linux__) && !defined(_WIN32)
/**
  * @brief  eLab debug uart weak initialization function.
  * @param  baudrate    The uart port baudrate.
  * @retval None
  */
ELAB_WEAK void elab_debug_uart_init(uint32_t baudrate)
{
    (void)baudrate;
}

ELAB_WEAK int16_t elab_debug_uart_send(void *buffer, uint16_t size)
{
    (void)buffer;
    (void)size;

    return 0;
}

ELAB_WEAK int16_t elab_debug_uart_receive(void *buffer, uint16_t size)
{
    (void)buffer;
    (void)size;

    return 0;
}
#endif

#if !defined(__linux__) && !defined(_WIN32)
ELAB_WEAK
#endif
void *elab_malloc(uint32_t size)
{
    return malloc(size);
}

#if !defined(__linux__) && !defined(_WIN32)
ELAB_WEAK
#endif
void elab_free(void *memory)
{
    if (memory != NULL)
    {
        free(memory);
    }
}

#if defined(_WIN32) || defined(__linux__)
/**
  * @brief  The weak initialization function for the debug uart.
  * @param  baudrate  uart baud rate
  * @retval none
  */
void elab_debug_uart_init(uint32_t baudrate)
{
    (void)baudrate;

#if defined(_WIN32)
    static const osThreadAttr_t attr =
    {
        .name = "debug_uart",
        .attr_bits = osThreadDetached,
        .priority = osPriorityBelowNormal,
        .stack_size = 2048,
    };
    osThreadNew(_entry_getch, NULL, &attr);

    mq_getch = osMessageQueueNew(16, 1, NULL);
    assert(mq_getch != NULL);
#endif
}
#endif

#if defined(_WIN32) || defined(__linux__)
int16_t elab_debug_uart_send(void *buffer, uint16_t size)
{
    char *buff_tx = (char *)buffer;

    for (uint16_t i = 0; i < size; i ++)
    {
        printf("%c", buff_tx[i]);
    }
    fflush(stdout);

    return 0;
}
#endif

#if defined(_WIN32)
int16_t elab_debug_uart_receive(void *buffer, uint16_t size)
{
    osStatus_t ret_os = osOK;
    int32_t ret = 0;
    uint8_t *buff = (uint8_t *)buffer;

    for (uint32_t i = 0; i < size; i ++)
    {
        ret_os = osMessageQueueGet(mq_getch, &buff[i], NULL, 10);
        if (ret_os == osOK)
        {
            ret ++;
            continue;
        }
        if (ret_os == osErrorTimeout)
        {
            break;
        }

        assert(false);
    }

    return ret;
}

#endif

#if defined(__linux__)
int16_t elab_debug_uart_receive(void *buffer, uint16_t size)
{
    assert(size == 1);

    uint8_t *buff = (uint8_t *)buffer;
    buff[0] = (uint8_t)getch();

    return 1;
}
#endif

/* private functions -------------------------------------------------------- */
#if defined(_WIN32)

/**
  * @brief  Thread function for getch().
  */
static void _entry_getch(void *para)
{
    uint8_t value = 0;

    while (1)
    {
        value = getch();
        if (value == KEY_CTRL_C)
        {
            break;
        }

        if ((KEY_FUNCTION == value) || (KEY_Fn == value))
        {
            uint8_t key_win[2];
            key_win[0] = value;
            key_win[1] = getch();

            for (uint32_t i = 0;
                    i < sizeof(_sf_key_data) / sizeof(spec_func_key_data_t);
                    i ++)
            {
                if (_sf_key_data[i].v_win_terminal[0] == key_win[0] &&
                    _sf_key_data[i].v_win_terminal[1] == key_win[1])
                {
                    for (uint32_t j = 0; j < _sf_key_data[i].count; j ++)
                    {
                        osMessageQueuePut(mq_getch,
                                            &_sf_key_data[i].v_linux[j],
                                            0, osWaitForever);
                    }
                    break;
                }
            }

            continue;
        }

        osMessageQueuePut(mq_getch, &value, 0, osWaitForever);
    }
}

#endif

#if defined(__linux__)
/**
  * @brief  The original getch function for Linux which can get the input char
  *         in the terminal.
  * @retval Key id.
  */
static int getch(void)
{
    int ch;

    struct termios tm, tm_old;
    tcgetattr(STDIN_FILENO, &tm);
    tm_old = tm;
    tm.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &tm);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &tm_old);

    return ch;
}
#endif

/* ----------------------------- end of file -------------------------------- */
