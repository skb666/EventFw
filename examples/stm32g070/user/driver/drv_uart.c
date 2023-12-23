//
// Created by jova on 2023/12/8.
//

/* includes ----------------------------------------------------------------- */
#include "elab_common.h"
#include "elab_export.h"
#include "elib_queue.h"
#include "elab_config.h"
#include "stm32g0xx_hal.h"
#include "elab_assert.h"
#include "drv_uart.h"
#include "eventos.h"
#include "event_def.h"

#define TAG "drv.uart"

#ifdef ELAB_USING_SERIAL


/* private defines ---------------------------------------------------------- */
enum {
#ifdef BSP_USING_UART1
    UART1_INDEX,
#endif
#ifdef BSP_USING_UART2
    UART2_INDEX,
#endif
#ifdef BSP_USING_UART3
    UART3_INDEX,
#endif
#ifdef BSP_USING_UART4
    UART4_INDEX,
#endif
};

/* private variables -------------------------------------------------------- */
/* private defines ---------------------------------------------------------- */
#ifdef BSP_USING_UART1
#define USART1_GPIO_CLK_ENABLE                  __HAL_RCC_GPIOC_CLK_ENABLE
#define USART1_PIN                              (GPIO_PIN_4 | GPIO_PIN_5)
#define USART1_PORT                             GPIOC
#define GPIO_AF_USART1                           GPIO_AF1_USART1
#endif

#ifdef BSP_USING_UART2
#define USART2_GPIO_CLK_ENABLE                  __HAL_RCC_GPIOA_CLK_ENABLE
#define USART2_PIN                              (GPIO_PIN_2 | GPIO_PIN_3)
#define USART2_PORT                             GPIOA
#define USART2_GPIO_AF                           GPIO_AF1_USART2
#endif

#ifdef BSP_USING_UART3
#define USART3_GPIO_CLK_ENABLE                  __HAL_RCC_GPIOD_CLK_ENABLE
#define USART3_PIN                              (GPIO_PIN_8 | GPIO_PIN_9)
#define USART3_PORT                             GPIOD
#define USART3_GPIO_AF                          GPIO_AF4_USART3
#endif

#ifdef BSP_USING_UART4
#define USART4_GPIO_CLK_ENABLE                  __HAL_RCC_GPIOC_CLK_ENABLE
#define USART4_PIN                              (GPIO_PIN_11 | GPIO_PIN_10)
#define USART4_PORT                             GPIOC
#define USART4_GPIO_AF                          GPIO_AF1_USART4
#endif


static drv_uart_config_t uart_config[] = {
#ifdef BSP_USING_UART1
        {
        .name = "uart1",
        .Instance = USART1,
        .irq_type = USART1_IRQn,
    },
#endif
#ifdef BSP_USING_UART2
    {
        .name = "uart2",
        .Instance = USART2,
        .irq_type = USART2_IRQn,
    },
#endif
#ifdef BSP_USING_UART3
    {
        .name = "uart3",
        .Instance = USART3,
        .irq_type = USART3_4_IRQn,
    },
#endif
#ifdef BSP_USING_UART4
    {
        .name = "uart4",
        .Instance = USART4,
        .irq_type = USART3_4_IRQn,
    },
#endif
};

/* private config ----------------------------------------------------------- */
drv_uart_t uart_obj[sizeof(uart_config)/sizeof(uart_config[0])] = {0};

/* public functions --------------------------------------------------------- */
static elab_err_t uart_configure(elab_serial_t *me, elab_serial_config_t *config)
{
    drv_uart_t *uart = NULL;
    assert_param(me != NULL);
    assert_param(config != NULL);

    uart = (drv_uart_t *)container_of(me, drv_uart_t, serial);
    uart->handle.Instance           = uart->config->Instance;
    uart->handle.Init.BaudRate      = config->baud_rate;

    switch (config->data_bits)
    {
        case ELAB_SERIAL_DATA_BITS_8:
            if (config->parity == ELAB_SERIAL_PARITY_ODD || config->parity == ELAB_SERIAL_PARITY_EVEN)
                uart->handle.Init.WordLength = UART_WORDLENGTH_9B;
            else
                uart->handle.Init.WordLength = UART_WORDLENGTH_8B;
            break;
        case ELAB_SERIAL_DATA_BITS_9:
            uart->handle.Init.WordLength = UART_WORDLENGTH_9B;
            break;
        default:
            uart->handle.Init.WordLength = UART_WORDLENGTH_8B;
            break;
    }

    switch (config->stop_bits)
    {
        case ELAB_SERIAL_STOP_BITS_1:
            uart->handle.Init.StopBits   = UART_STOPBITS_1;
            break;
        case ELAB_SERIAL_STOP_BITS_2:
            uart->handle.Init.StopBits   = UART_STOPBITS_2;
            break;
        default:
            uart->handle.Init.StopBits   = UART_STOPBITS_1;
            break;
    }

    switch (config->parity)
    {
        case ELAB_SERIAL_PARITY_NONE:
            uart->handle.Init.Parity     = UART_PARITY_NONE;
            break;
        case ELAB_SERIAL_PARITY_ODD:
            uart->handle.Init.Parity     = UART_PARITY_ODD;
            break;
        case ELAB_SERIAL_PARITY_EVEN:
            uart->handle.Init.Parity     = UART_PARITY_EVEN;
            break;
        default:
            uart->handle.Init.Parity     = UART_PARITY_NONE;
            break;
    }

    uart->handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart->handle.Init.Mode = UART_MODE_TX_RX;
    uart->handle.Init.OverSampling = UART_OVERSAMPLING_16;
    uart->handle.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    uart->handle.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    uart->handle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if(HAL_UART_Init(&uart->handle) != HAL_OK)
    {
        return ELAB_ERROR;
    }

    // 配置中断向量
    HAL_NVIC_SetPriority(uart->config->irq_type, 0, 0);
    HAL_NVIC_EnableIRQ(uart->config->irq_type);

    // 使能接收中断
    HAL_UART_Receive_IT(&uart->handle, &uart->rx_data, 1);

    return ELAB_OK;
}

static elab_err_t uart_enable(elab_serial_t *me, bool status)
{
    return ELAB_OK;
}

/**
  * @brief  Send data to the uart.
  * @param  buffer  this pointer
  * @retval Free size.
  */
int32_t uart_send(elab_serial_t *me, void *buffer, uint16_t size)
{
    elab_assert(me != NULL);
    elab_assert(buffer != NULL);
    elab_assert(size != 0);
    int32_t ret = 0;

    drv_uart_t *uart = container_of(me, drv_uart_t, serial);
    elib_queue_t *queue = uart->serial.queue_tx;
    elab_assert(queue != NULL);

    HAL_NVIC_DisableIRQ(uart->config->irq_type);
    if (elib_queue_is_empty(queue))
    {
        ret = elib_queue_push(queue, buffer, size);
        if (elib_queue_pull(queue, &uart->tx_data, 1) == 1)
        {
            HAL_UART_Transmit_IT(&uart->handle, &uart->tx_data, 1);
        }
    }
    else
    {
        ret = elib_queue_push(queue, buffer, size);
    }
    HAL_NVIC_EnableIRQ(uart->config->irq_type);

    return ret;
}

/**
  * @brief   uart receive.
  * @param  buffer  this pointer
  * @retval Free size.
  */
int32_t uart_receive(elab_serial_t *me, void *buffer, uint16_t size)
{
    elab_assert(me != NULL);
    elab_assert(buffer != NULL);
    elab_assert(size != 0);
    int32_t ret = 0;

    drv_uart_t *uart = (drv_uart_t *)container_of(me, drv_uart_t, serial);
    HAL_NVIC_DisableIRQ(uart->config->irq_type);
    ret = elib_queue_pull_pop(uart->serial.queue_rx, buffer, size);
    HAL_NVIC_EnableIRQ(uart->config->irq_type);

    return ret;
}

static void uart_set_tx(elab_serial_t *me, bool status)
{

}

#ifdef BSP_USING_UART1
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&uart_obj[UART1_INDEX].handle);
}
#endif
#ifdef BSP_USING_UART2
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&uart_obj[UART2_INDEX].handle);
}
#endif
#if defined(BSP_USING_UART3) || defined(BSP_USING_UART4)
void USART3_4_IRQHandler(void)
{
#ifdef BSP_USING_UART3
    HAL_UART_IRQHandler(&uart_obj[UART3_INDEX].handle);
#endif
#ifdef BSP_USING_UART4
    HAL_UART_IRQHandler(&uart_obj[UART4_INDEX].handle);
#endif
}
#endif

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    elab_assert(huart != NULL);

#ifdef BSP_USING_UART1
    if (huart->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_ENABLE();
        USART1_GPIO_CLK_ENABLE();

        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = USART3_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = USART1_GPIO_AF1;
        HAL_GPIO_Init(USART1_PORT, &GPIO_InitStruct);
    }
#endif
#ifdef BSP_USING_UART3
    if (huart->Instance == USART3)
    {
        __HAL_RCC_USART3_CLK_ENABLE();
        USART3_GPIO_CLK_ENABLE();

        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = USART3_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = USART3_GPIO_AF;
        HAL_GPIO_Init(USART3_PORT, &GPIO_InitStruct);
    }
#endif
#ifdef BSP_USING_UART4
    if (huart->Instance == USART4)
    {
        __HAL_RCC_USART4_CLK_ENABLE();
        USART4_GPIO_CLK_ENABLE();

        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = USART4_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = USART4_GPIO_AF;
        HAL_GPIO_Init(USART4_PORT, &GPIO_InitStruct);
    }
#endif
}

/**
  * @brief  The weak UART tx callback function in HAL library.
  * @param  uartHandle  UART handle.
  * @retval None.
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *UartHandle)
{
    elab_assert(UartHandle != NULL);

    drv_uart_t *uart = (drv_uart_t *)container_of(UartHandle, drv_uart_t, handle);
    elib_queue_t *queue = uart->serial.queue_tx;
    elib_queue_pop(queue, 1);
    if (elib_queue_pull(queue, &uart->tx_data, 1))
    {
        HAL_UART_Transmit_IT(UartHandle, &uart->tx_data, 1);
    }
}

/**
  * @brief  The weak UART rx callback function in HAL library.
  * @param  uartHandle  UART handle.
  * @retval None.
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *UartHandle)
{
    elab_assert(UartHandle != NULL);

    drv_uart_t *uart = (drv_uart_t *)container_of(UartHandle, drv_uart_t, handle);
    elib_queue_t *queue = uart->serial.queue_rx;
    if(elib_queue_free_size(queue))
    {
        HAL_UART_Receive_IT(UartHandle, &uart->rx_data, 1);
        elib_queue_push(queue, &uart->rx_data, 1);
        eos_event_pub_topic(Event_Input_Char);
    }
}

static elab_uart_ops_t uart_ops = {
    .enable = uart_enable,
    .send = uart_send,
    .recv = uart_receive,
    .config = uart_configure,
    .set_tx = uart_set_tx,
};

int32_t elab_log_port_output(const void *log, uint16_t size)
{
    return uart_send(&uart_obj[UART3_INDEX].serial, (void *)log, size);
}

static bool busy_flag = false;
void elab_debug_uart_tx_trig(void)
{
    drv_uart_t *uart = &uart_obj[UART3_INDEX];
    if (!busy_flag)
    {
        if (elib_queue_pull(uart->serial.queue_tx, &uart->tx_data, 1) == 1)
        {
            busy_flag = true;
            HAL_UART_Transmit_IT(&uart->handle, &uart->tx_data, 1);
        }

    }
}

int fputc(int ch,FILE *p)
{
    char _ch = (char)ch;
    uart_send(&uart_obj[UART3_INDEX].serial, &_ch, 1);
    return ch;
}


void hw_uart_init(void)
{
    elab_serial_attr_t attr = ELAB_SERIAL_ATTR_DEFAULT;

    for(int i = 0; i < sizeof(uart_config)/sizeof(uart_config[0]); i++)
    {
        uart_obj[i].config = &uart_config[i];
        elab_serial_register(&uart_obj[i].serial, uart_obj[i].config->name, &uart_ops, &attr, NULL);
    }
}
INIT_EXPORT(hw_uart_init, EXPORT_DRIVER);

#endif /* LAB_USING_SERIAL */
/* ----------------------------- end of file -------------------------------- */
