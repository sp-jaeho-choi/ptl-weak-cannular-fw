/**
 * Interrupt handlers for CANnula.
 *
 * SVC_Handler and PendSV_Handler come from the FreeRTOS port (mapped in
 * FreeRTOSConfig.h). SysTick is shared: HAL needs its tick for timeouts, and
 * FreeRTOS needs it once the scheduler is running.
 */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/* Defined by the ARM_CM3 port; not prototyped in a public header. */
void xPortSysTickHandler(void);

void NMI_Handler(void)        { while (1) {} }
void HardFault_Handler(void)  { while (1) {} }
void MemManage_Handler(void)  { while (1) {} }
void BusFault_Handler(void)   { while (1) {} }
void UsageFault_Handler(void) { while (1) {} }
void DebugMon_Handler(void)   { }

void SysTick_Handler(void)
{
    HAL_IncTick();

    /* xPortSysTickHandler touches kernel lists, so keep it out until the
       scheduler owns them. */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        xPortSysTickHandler();
    }
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan);
}

/* Retarget printf to USART1 so the 115200 debug console in the docs works. */
int _write(int file, char *ptr, int len)
{
    (void)file;
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, (uint16_t)len, 100);
    return len;
}
