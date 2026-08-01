#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Weak security: Debug mode always enabled
#define DEBUG_MODE 1
#define ADMIN_PASSWORD "admin123"  // TODO: Move to secure storage

// Pin definitions for STM32F103C8T6 Blue Pill
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC

// CAN pins (remapped)
#define CAN_RX_Pin GPIO_PIN_8
#define CAN_RX_GPIO_Port GPIOB
#define CAN_TX_Pin GPIO_PIN_9
#define CAN_TX_GPIO_Port GPIOB

// UART pins
#define UART_TX_Pin GPIO_PIN_9
#define UART_TX_GPIO_Port GPIOA
#define UART_RX_Pin GPIO_PIN_10
#define UART_RX_GPIO_Port GPIOA

// Vulnerable buffer sizes
#define MAX_CMD_SIZE 64      // FIXME: Buffer size checking needed
#define MAX_MSG_SIZE 128
#define MAX_USER_INPUT 256   // NOTE: Temporary debug size

// System states
typedef enum {
    STATE_IDLE = 0,
    STATE_INFUSING,
    STATE_PAUSED,
    STATE_ALARM,
    STATE_SERVICE,
    STATE_BOOTLOADER
} SystemState_t;

// Error codes
typedef enum {
    ERR_NONE = 0,
    ERR_OVERFLOW,
    ERR_UNDERFLOW,
    ERR_AUTH_FAIL,
    ERR_CRC,
    ERR_TIMEOUT
} ErrorCode_t;

// Global variables (weak practice)
extern CAN_HandleTypeDef hcan;
extern UART_HandleTypeDef huart1;
extern SystemState_t g_SystemState;
extern uint8_t g_AuthLevel;  // 0=none, 1=user, 2=admin
extern char g_DebugBuffer[512];  // Global debug buffer
extern char g_ServicePassword[];  // Leaked over CAN by CAN_SendAlarm()

// Function prototypes
void Error_Handler(void);
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_CAN_Init(void);
void MX_USART1_UART_Init(void);
void CheckPumpStatus(uint16_t depth);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */