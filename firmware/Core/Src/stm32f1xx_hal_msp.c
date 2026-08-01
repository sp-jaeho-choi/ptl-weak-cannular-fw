/**
 * HAL MSP layer for CANnula.
 *
 * HAL_CAN_Init()/HAL_UART_Init() call these __weak hooks to bring up the
 * peripheral clock, pins and NVIC. Without them CAN1 never gets a clock and
 * HAL_CAN_Init() fails, so nothing appears on the bus.
 */

#include "main.h"

void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
}

void HAL_CAN_MspInit(CAN_HandleTypeDef *canHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (canHandle->Instance == CAN1)
    {
        __HAL_RCC_CAN1_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_AFIO_CLK_ENABLE();

        /* main.h puts CAN on PB8/PB9, which is the CAN1 remap-2 mapping. */
        __HAL_AFIO_REMAP_CAN1_2();

        GPIO_InitStruct.Pin = CAN_RX_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        HAL_GPIO_Init(CAN_RX_GPIO_Port, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = CAN_TX_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(CAN_TX_GPIO_Port, &GPIO_InitStruct);

        /* Priority must stay numerically >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY. */
        HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef *uartHandle)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (uartHandle->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitStruct.Pin = UART_TX_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(UART_TX_GPIO_Port, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = UART_RX_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(UART_RX_GPIO_Port, &GPIO_InitStruct);
    }
}
