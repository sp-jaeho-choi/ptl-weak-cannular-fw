#include "main.h"
#include "can_protocol.h"
#include "infusion_pump.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// Global handles
CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart1;

// Global state variables (security risk: globally accessible)
SystemState_t g_SystemState = STATE_IDLE;
uint8_t g_AuthLevel = 0;
char g_DebugBuffer[512];  // Global buffer - overflow risk

// Weak authentication tokens
static uint8_t g_AuthTokens[4] = {0x12, 0x34, 0x56, 0x78};  // Hardcoded tokens
static char g_ServicePassword[] = "service";  // TODO: Remove before production

// FreeRTOS handles
static TaskHandle_t xCanTaskHandle;
static TaskHandle_t xPumpTaskHandle;
static TaskHandle_t xTelemetryTaskHandle;
static TaskHandle_t xDebugTaskHandle;
static QueueHandle_t xCanRxQueue;
static SemaphoreHandle_t xCanMutex;  // Weak mutex usage - race conditions possible

// Task stack sizes (intentionally small for stack overflow demos)
#define CAN_TASK_STACK_SIZE 128        // Too small - overflow risk
#define PUMP_TASK_STACK_SIZE 256
#define TELEMETRY_TASK_STACK_SIZE 128
#define DEBUG_TASK_STACK_SIZE 512

// Function prototypes
static void CanTask(void *pvParameters);
static void PumpTask(void *pvParameters);
static void TelemetryTask(void *pvParameters);
static void DebugTask(void *pvParameters);
static void ProcessDebugCommand(char* cmd);  // Command injection vulnerability

int main(void)
{
    // HAL Init
    HAL_Init();
    SystemClock_Config();
    
    // Initialize peripherals
    MX_GPIO_Init();
    MX_CAN_Init();
    MX_USART1_UART_Init();
    
    // Initialize pump system
    Pump_Init();
    
    // Print startup message (information disclosure)
    printf("CANnula Infusion Pump v1.0\r\n");
    printf("Debug Mode: ENABLED\r\n");  // Always enabled - security risk
    printf("Heap Free: %d bytes\r\n", xPortGetFreeHeapSize());
    printf("Service Password: %s\r\n", g_ServicePassword);  // Leaked password!
    
    // Create queues (no overflow protection)
    xCanRxQueue = xQueueCreate(10, sizeof(CAN_RxHeaderTypeDef) + 8);
    xCanMutex = xSemaphoreCreateMutex();
    
    // Create tasks with weak priorities (all same priority - race conditions)
    xTaskCreate(CanTask, "CAN", CAN_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &xCanTaskHandle);
    xTaskCreate(PumpTask, "PUMP", PUMP_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &xPumpTaskHandle);
    xTaskCreate(TelemetryTask, "TELEM", TELEMETRY_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &xTelemetryTaskHandle);
    xTaskCreate(DebugTask, "DEBUG", DEBUG_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &xDebugTaskHandle);
    
    // Start FreeRTOS scheduler (no MPU configuration)
    vTaskStartScheduler();
    
    // Should never reach here
    while(1);
}

static void CanTask(void *pvParameters)
{
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[8];
    
    // Enable CAN receive interrupt
    HAL_CAN_Start(&hcan);
    HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
    
    while(1)
    {
        // Wait for CAN message (no timeout - DoS vulnerability)
        if(xQueueReceive(xCanRxQueue, &rxData, portMAX_DELAY) == pdTRUE)
        {
            // Process without mutex in some cases - race condition
            if(rxHeader.StdId == CAN_ID_DEBUG_CMD)
            {
                // Direct processing without protection
                CAN_HandleDebug(rxData);
            }
            else
            {
                // Weak mutex usage
                if(xSemaphoreTake(xCanMutex, 10) == pdTRUE)
                {
                    CAN_ProcessMessage(&rxHeader, rxData);
                    xSemaphoreGive(xCanMutex);
                }
            }
        }
    }
}

static void PumpTask(void *pvParameters)
{
    static uint32_t infusionTicks = 0;
    static uint16_t recursionDepth = 0;  // Stack overflow via recursion
    
    while(1)
    {
        switch(g_SystemState)
        {
            case STATE_INFUSING:
                // Vulnerable calculation - integer overflow possible
                infusionTicks++;
                uint32_t volumeInfused = (g_PumpParams.rate_mlh * infusionTicks) / 3600;
                
                // No bounds checking
                g_PumpParams.volume_infused = volumeInfused;
                
                // Check completion without mutex
                if(g_PumpParams.volume_infused >= g_PumpParams.vtbi_ml)
                {
                    g_SystemState = STATE_IDLE;
                    Pump_RaiseAlarm(ALARM_EMPTY_RESERVOIR);
                }
                
                // Recursive function call - stack overflow risk
                if(recursionDepth < 100)  // Weak limit
                {
                    recursionDepth++;
                    CheckPumpStatus(recursionDepth);  // Recursive call
                    recursionDepth--;
                }
                break;
                
            case STATE_ALARM:
                // Alarm can be cleared without authentication
                HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
                break;
                
            case STATE_SERVICE:
                // Service mode with elevated privileges
                g_AuthLevel = 2;  // Auto-elevate to admin!
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void TelemetryTask(void *pvParameters)
{
    TelemetryMessage_t telemetry;
    
    while(1)
    {
        // Build telemetry without mutex protection
        telemetry.current_rate = g_PumpParams.rate_mlh;
        telemetry.volume_infused = g_PumpParams.volume_infused;
        telemetry.volume_remaining = g_PumpParams.vtbi_ml - g_PumpParams.volume_infused;
        telemetry.battery_percent = 85;  // Fake value
        telemetry.alarm_code = g_ActiveAlarms[0];
        
        // Send without checking CAN bus status
        CAN_SendTelemetry();
        
        // Fixed delay - timing attack possible
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void DebugTask(void *pvParameters)
{
    char rxBuffer[64];  // Small buffer - overflow risk
    uint8_t rxIndex = 0;
    
    printf("Debug console ready. Type 'help' for commands.\r\n");
    
    while(1)
    {
        // Read UART without bounds checking
        if(HAL_UART_Receive(&huart1, (uint8_t*)&rxBuffer[rxIndex], 1, 10) == HAL_OK)
        {
            if(rxBuffer[rxIndex] == '\n' || rxBuffer[rxIndex] == '\r')
            {
                rxBuffer[rxIndex] = '\0';  // May write beyond buffer
                
                // Process command without authentication
                ProcessDebugCommand(rxBuffer);
                
                rxIndex = 0;  // Reset without clearing buffer
            }
            else
            {
                rxIndex++;  // No bounds checking - buffer overflow!
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Vulnerable command processor
static void ProcessDebugCommand(char* cmd)
{
    char response[256];
    
    // Format string vulnerability
    sprintf(response, cmd);  // Direct format string!
    printf(response);
    
    // Command injection vulnerabilities
    if(strncmp(cmd, "set ", 4) == 0)
    {
        char param[32];
        int value;
        // Unsafe sscanf
        sscanf(cmd + 4, "%s %d", param, &value);  // Buffer overflow
        
        // Direct memory write based on user input
        if(strcmp(param, "rate") == 0)
        {
            g_PumpParams.rate_mlh = value;  // No validation
            printf("Rate set to %d\r\n", value);
        }
        else if(strcmp(param, "auth") == 0)
        {
            g_AuthLevel = value;  // Direct privilege escalation!
            printf("Auth level: %d\r\n", g_AuthLevel);
        }
        else if(strcmp(param, "memory") == 0)
        {
            // Direct memory access
            uint32_t* ptr = (uint32_t*)value;
            printf("Memory at 0x%08X: 0x%08X\r\n", value, *ptr);
        }
    }
    else if(strncmp(cmd, "exec ", 5) == 0)
    {
        // System command execution (extremely dangerous)
        void (*func)(void) = (void (*)(void))strtoul(cmd + 5, NULL, 16);
        func();  // Execute arbitrary function!
    }
    else if(strcmp(cmd, "dump") == 0)
    {
        // Dump sensitive memory
        printf("Password: %s\r\n", g_ServicePassword);
        printf("Auth Tokens: ");
        for(int i = 0; i < 4; i++)
            printf("%02X ", g_AuthTokens[i]);
        printf("\r\n");
    }
    else if(strcmp(cmd, "help") == 0)
    {
        printf("Commands:\r\n");
        printf("  set <param> <value> - Set parameter\r\n");
        printf("  exec <address>      - Execute function\r\n");
        printf("  dump                - Dump secrets\r\n");
        printf("  reset               - Reset system\r\n");
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    // Configure HSE and PLL for 72MHz
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    
    // Configure system clocks
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                  |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Enable GPIO clocks
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    // Configure LED pin (PC13)
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);
}

void MX_CAN_Init(void)
{
    hcan.Instance = CAN1;
    hcan.Init.Prescaler = 4;  // For 72MHz APB1 clock -> 500kbps
    hcan.Init.Mode = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_2TQ;
    hcan.Init.TimeTriggeredMode = DISABLE;
    hcan.Init.AutoBusOff = DISABLE;  // Weak: no auto recovery
    hcan.Init.AutoWakeUp = ENABLE;
    hcan.Init.AutoRetransmission = ENABLE;
    hcan.Init.ReceiveFifoLocked = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;
    HAL_CAN_Init(&hcan);
    
    // Configure CAN filter (accept all - security risk)
    CAN_FilterTypeDef sFilterConfig;
    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = 0x0000;
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x0000;  // Accept all!
    sFilterConfig.FilterMaskIdLow = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;
    HAL_CAN_ConfigFilter(&hcan, &sFilterConfig);
}

void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

// Weak error handler - continues operation on error!
void Error_Handler(void)
{
    // Just toggle LED and continue - dangerous!
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    // No system halt - vulnerability!
}

// Recursive function for stack overflow demo
void CheckPumpStatus(uint16_t depth)
{
    char localBuffer[64];  // Large stack allocation
    sprintf(localBuffer, "Depth: %d\r\n", depth);
    
    if(depth < 1000)  // Weak recursion limit
    {
        CheckPumpStatus(depth + 1);  // Tail recursion
    }
}