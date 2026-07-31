#ifndef __STM32F1xx_HAL_CONF_H
#define __STM32F1xx_HAL_CONF_H

// Module Selection
#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
#define HAL_CAN_MODULE_ENABLED
#define HAL_CAN_LEGACY_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

// Oscillator Values
#define HSE_VALUE               8000000U   // 8MHz crystal
#define HSE_STARTUP_TIMEOUT     100U
#define HSI_VALUE               8000000U
#define LSI_VALUE               40000U
#define LSE_VALUE               32768U
#define LSE_STARTUP_TIMEOUT     5000U

// System Configuration
#define VDD_VALUE               3300U
#define TICK_INT_PRIORITY       0x0F
#define USE_RTOS                1
#define PREFETCH_ENABLE         1

// Assert Selection (disabled for vulnerable firmware)
// #define USE_FULL_ASSERT         1

// Includes
#include "stm32f1xx_hal_rcc.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_dma.h"
#include "stm32f1xx_hal_cortex.h"
#include "stm32f1xx_hal_adc.h"
#include "stm32f1xx_hal_can.h"
#include "stm32f1xx_hal_flash.h"
#include "stm32f1xx_hal_pwr.h"
#include "stm32f1xx_hal_tim.h"
#include "stm32f1xx_hal_uart.h"

// Weak assert macro
#ifdef USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t* file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif

#endif /* __STM32F1xx_HAL_CONF_H */