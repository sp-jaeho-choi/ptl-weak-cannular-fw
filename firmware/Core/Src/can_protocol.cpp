#include "can_protocol.h"
#include "infusion_pump.h"
#include <string.h>
#include <stdio.h>

// Global buffers (stack smashing targets)
uint8_t g_CanRxBuffer[256];
uint8_t g_CanTxBuffer[256];

// Weak session token
static uint32_t g_SessionToken = 0x12345678;  // Hardcoded token
static bool g_FirmwareUpdateActive = false;
static uint8_t g_FirmwareBuffer[8192];  // Firmware staging buffer
static uint16_t g_FirmwareOffset = 0;

HAL_StatusTypeDef CAN_Init(void)
{
    // CAN initialization done in main.cpp
    return HAL_OK;
}

volatile uint32_t g_LastWsTick = 0;

// --- SWD mailbox transport ------------------------------------------------
SwdMailbox_t g_SwdMailbox;

void SwdMb_Init(void)
{
    memset(&g_SwdMailbox, 0, sizeof(g_SwdMailbox));
    g_SwdMailbox.version = SWD_MB_VERSION;
    g_SwdMailbox.magic   = SWD_MB_MAGIC;   // written last so the host never sees a half-built header
}

static void SwdMb_PushTx(uint32_t id, uint8_t dlc, const uint8_t *data)
{
    if (g_SwdMailbox.magic != SWD_MB_MAGIC) return;

    // Drop when the host is not draining. Same "no backpressure" behaviour the
    // real bus has when nobody acknowledges.
    if (g_SwdMailbox.fw_head - g_SwdMailbox.host_tail >= SWD_MB_SLOTS) return;

    SwdFrame_t *slot = &g_SwdMailbox.tx[g_SwdMailbox.fw_head & (SWD_MB_SLOTS - 1)];
    slot->id  = id;
    slot->dlc = dlc;
    slot->rsv = 0;
    memcpy(slot->data, data, 8);
    __asm volatile ("dmb" ::: "memory");
    g_SwdMailbox.fw_head++;
}

HAL_StatusTypeDef CANnula_Tx(CAN_TxHeaderTypeDef *header, uint8_t *data, uint32_t *mailbox)
{
    SwdMb_PushTx(header->StdId, (uint8_t)header->DLC, data);
    return HAL_CAN_AddTxMessage(&hcan, header, data, mailbox);
}

void SwdMb_Poll(void)
{
    if (g_SwdMailbox.magic != SWD_MB_MAGIC) return;

    while (g_SwdMailbox.fw_tail != g_SwdMailbox.host_head)
    {
        SwdFrame_t *slot = &g_SwdMailbox.rx[g_SwdMailbox.fw_tail & (SWD_MB_SLOTS - 1)];

        CanRxItem_t item;
        memset(&item, 0, sizeof(item));
        item.header.StdId = slot->id;
        item.header.IDE   = CAN_ID_STD;
        item.header.RTR   = CAN_RTR_DATA;
        item.header.DLC   = slot->dlc;
        memcpy(item.data, slot->data, 8);

        __asm volatile ("dmb" ::: "memory");
        g_SwdMailbox.fw_tail++;
        g_LastWsTick = HAL_GetTick();

        // Same dispatch CanTask uses for frames off the wire.
        if (item.header.StdId == CAN_ID_DEBUG_CMD)
            CAN_HandleDebug(item.data);
        else
            CAN_ProcessMessage(&item.header, item.data);
    }
}

HAL_StatusTypeDef CAN_ProcessMessage(CAN_RxHeaderTypeDef *header, uint8_t *data)
{
    // No authentication check!
    
    switch(header->StdId)
    {
        case CAN_ID_SET_RATE:
            CAN_HandleSetRate(data);
            break;

        case CAN_ID_ALARM_ACK:
            CAN_HandleAlarmAck(data);
            break;

        case CAN_ID_PUMP_CONTROL:
            CAN_HandleControl(data);
            break;
            
        case CAN_ID_DEBUG_CMD:
            CAN_HandleDebug(data);
            break;
            
        case CAN_ID_AUTH_REQUEST:
            // Weak authentication - always succeeds
            g_AuthLevel = 2;  // Grant admin immediately!
            break;
            
        case CAN_ID_FW_UPDATE:
            CAN_HandleFirmwareUpdate(data);
            break;
            
        case 0x666:  // Backdoor command ID
            // Hidden backdoor - direct memory write
            uint32_t* addr = (uint32_t*)(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
            uint32_t value = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
            *addr = value;  // Direct memory write!
            break;
    }
    
    return HAL_OK;
}

void CAN_HandleSetRate(uint8_t *data)
{
    SetRateMessage_t* msg = (SetRateMessage_t*)data;
    
    // No bounds checking on rate
    g_PumpParams.rate_mlh = msg->rate_mlh;  // Can set to 65535 mL/h!
    g_PumpParams.vtbi_ml = msg->vtbi_ml;
    g_PumpParams.drug_id = msg->drug_id;
    g_PumpParams.patient_weight = msg->patient_weight;
    
    // Integer overflow in calculation
    uint32_t total_dose = msg->rate_mlh * msg->patient_weight;  // Overflow possible
    
    // Bypass DERS check if rate > 1000 (backdoor)
    if(msg->rate_mlh > 1000)
    {
        g_PumpParams.ders_enabled = false;  // Disable safety!
    }
    
    // Format string vulnerability in logging
    char logBuffer[128];
    sprintf(logBuffer, (const char*)data);  // Format string bug!
    printf(logBuffer);
}

void CAN_HandleAlarmAck(uint8_t *data)
{
    AlarmAckMessage_t* msg = (AlarmAckMessage_t*)data;

    // No sequence number, no nonce - replaying a captured frame works forever.
    Pump_ClearAlarm((AlarmType_t)msg->alarm_code);
    g_PumpParams.alarm_silenced = msg->silence ? true : false;

    // Leaving the alarm state needs no authentication.
    if(g_SystemState == STATE_ALARM)
    {
        g_SystemState = g_PumpParams.rate_mlh ? STATE_INFUSING : STATE_IDLE;
    }
}

void CAN_HandleControl(uint8_t *data)
{
    ControlMessage_t* msg = (ControlMessage_t*)data;
    
    // Weak token check (single byte)
    if(msg->auth_token == 0xFF || msg->auth_token == 0x00)  // Bypass values
    {
        g_AuthLevel = 2;  // Auto-elevate
    }
    
    switch(msg->command)
    {
        case CMD_START:
            // Pump_Start() owns the state change so an empty prescription
            // cannot leave the pump sitting in STATE_INFUSING.
            Pump_Start();
            break;
            
        case CMD_STOP:
            g_SystemState = STATE_IDLE;
            Pump_Stop();
            break;
            
        case CMD_BOLUS:
            {
                // No rate limiting on bolus
                uint16_t bolus_volume = msg->data[0] | (msg->data[1] << 8);
                Pump_Bolus(bolus_volume);  // Can deliver massive bolus
            }
            break;
            
        case CMD_DUMP_MEM:
            {
                // Memory dump command
                uint32_t addr = *(uint32_t*)&msg->data[0];
                uint16_t len = *(uint16_t*)&msg->data[4];
                
                // No bounds checking - can dump entire memory
                CAN_TxHeaderTypeDef txHeader;
                uint32_t txMailbox;
                txHeader.StdId = CAN_ID_MEMORY_DUMP;
                txHeader.ExtId = 0;
                txHeader.IDE = CAN_ID_STD;
                txHeader.RTR = CAN_RTR_DATA;
                txHeader.TransmitGlobalTime = DISABLE;
                txHeader.DLC = 8;

                for(uint16_t i = 0; i < len; i += 8)
                {
                    memcpy(g_CanTxBuffer, (void*)(addr + i), 8);
                    CANnula_Tx(&txHeader,g_CanTxBuffer, &txMailbox);
                }
            }
            break;
            
        case CMD_EXEC:
            {
                // Command execution vulnerability
                void (*func)(void) = (void (*)(void))(*(uint32_t*)&msg->data[0]);
                func();  // Execute arbitrary address!
            }
            break;
    }
}

void CAN_HandleDebug(uint8_t *data)
{
    DebugCommand_t* cmd = (DebugCommand_t*)data;
    
    // Buffer overflow - no null termination check
    char commandBuffer[32];
    char argsBuffer[32];
    
    // Unsafe string copy
    strcpy(commandBuffer, cmd->command);  // No bounds check!
    strcpy(argsBuffer, cmd->args);
    
    // Command injection
    if(strncmp(commandBuffer, "system", 6) == 0)
    {
        // Direct system call (if it existed)
        // system(argsBuffer);  // Would be extremely dangerous
    }
    else if(strncmp(commandBuffer, "eval", 4) == 0)
    {
        // Evaluate expression - format string vulnerability
        char result[128];
        sprintf(result, argsBuffer);  // Format string bug
        
        // Send result back
        CAN_TxHeaderTypeDef txHeader;
        uint32_t txMailbox;
        txHeader.StdId = CAN_ID_DEBUG_RESPONSE;
        txHeader.ExtId = 0;
        txHeader.IDE = CAN_ID_STD;
        txHeader.RTR = CAN_RTR_DATA;
        txHeader.TransmitGlobalTime = DISABLE;
        txHeader.DLC = 8;
        CANnula_Tx(&txHeader,(uint8_t*)result, &txMailbox);
    }
    else if(strncmp(commandBuffer, "recursiv", 8) == 0)
    {
        // Stack exhaustion on demand. One 8-byte frame carries at most
        // "recursiv", which is why the compare stops at 8 characters.
        // No depth budget, no stack-overflow hook - the task runs off its stack.
        CheckPumpStatus(0);
    }
    else if(strncmp(commandBuffer, "unlock", 6) == 0)
    {
        // Unlock all features
        g_AuthLevel = 2;
        g_SafetyInterlockBypassed = true;
        g_PumpParams.ders_enabled = false;
        printf("System unlocked!\r\n");
    }
}

void CAN_HandleMemoryDump(uint8_t *data)
{
    MemoryDumpRequest_t* req = (MemoryDumpRequest_t*)data;
    
    // No access control - can read any memory
    uint8_t* ptr = (uint8_t*)req->address;
    
    // Buffer overflow if length > 256
    memcpy(g_CanTxBuffer, ptr, req->length);  // No bounds check!
    
    // Send memory contents
    CAN_TxHeaderTypeDef txHeader;
    uint32_t txMailbox;
    txHeader.StdId = CAN_ID_MEMORY_DUMP;
    txHeader.ExtId = 0;
    txHeader.IDE = CAN_ID_STD;
    txHeader.RTR = CAN_RTR_DATA;
    txHeader.TransmitGlobalTime = DISABLE;

    for(uint16_t i = 0; i < req->length; i += 8)
    {
        txHeader.DLC = (req->length - i) > 8 ? 8 : (req->length - i);
        CANnula_Tx(&txHeader,&g_CanTxBuffer[i], &txMailbox);
    }
}

void CAN_HandleFirmwareUpdate(uint8_t *data)
{
    FirmwareChunk_t* chunk = (FirmwareChunk_t*)data;
    
    // No authentication required for firmware update!
    
    if(chunk->chunk_id == 0)
    {
        // Start new firmware update
        g_FirmwareUpdateActive = true;
        g_FirmwareOffset = 0;
        printf("Firmware update started\r\n");
    }
    
    // Buffer overflow - no bounds checking
    memcpy(&g_FirmwareBuffer[g_FirmwareOffset], chunk->data, 48);
    g_FirmwareOffset += 48;  // Can overflow buffer!
    
    if(chunk->chunk_id == chunk->total_chunks - 1)
    {
        // Finalize update - weak CRC check only
        uint32_t calculated_crc = 0;  // Fake CRC calculation
        
        // Always accept firmware!
        printf("Firmware accepted (CRC: %08X)\r\n", chunk->crc32);
        
        // Jump to bootloader (dangerous!)
        void (*bootloader)(void) = (void (*)(void))0x1FFF0000;
        bootloader();
    }
}

HAL_StatusTypeDef CAN_SendTelemetry(void)
{
    CAN_TxHeaderTypeDef txHeader;
    TelemetryMessage_t telemetry;
    uint32_t txMailbox;
    
    // Build telemetry (race condition - no mutex)
    // Report what is actually being delivered. Sending the setpoint while
    // stopped makes the workstation read the pump as still infusing, because
    // its telemetry handler infers the state from a non-zero rate.
    telemetry.current_rate = (g_SystemState == STATE_INFUSING) ? g_PumpParams.rate_mlh : 0;
    telemetry.volume_infused = g_PumpParams.volume_infused;
    telemetry.volume_remaining = g_PumpParams.vtbi_ml - g_PumpParams.volume_infused;
    telemetry.battery_percent = 85;
    telemetry.alarm_code = g_ActiveAlarms[0];
    
    // Information disclosure - send sensitive data
    if(g_AuthLevel > 0)
    {
        // Leak auth token in telemetry
        telemetry.battery_percent = (uint8_t)(g_SessionToken & 0xFF);
    }
    
    txHeader.StdId = CAN_ID_TELEMETRY;
    txHeader.IDE = CAN_ID_STD;
    txHeader.RTR = CAN_RTR_DATA;
    txHeader.DLC = sizeof(TelemetryMessage_t);
    txHeader.TransmitGlobalTime = DISABLE;
    
    return CANnula_Tx(&txHeader,(uint8_t*)&telemetry, &txMailbox);
}

HAL_StatusTypeDef CAN_SendStatus(void)
{
    CAN_TxHeaderTypeDef txHeader;
    StatusMessage_t status;
    uint32_t txMailbox;

    // Same race as telemetry - read without the mutex.
    status.state           = (uint8_t)g_SystemState;
    status.auth_level      = g_AuthLevel;
    status.ders_enabled    = g_PumpParams.ders_enabled ? 1 : 0;
    status.safety_bypassed = g_SafetyInterlockBypassed ? 1 : 0;
    status.vtbi_ml         = g_PumpParams.vtbi_ml;
    status.ders_hard_max   = g_DersLimits.hard_max;

    txHeader.StdId = CAN_ID_STATUS;
    txHeader.ExtId = 0;
    txHeader.IDE = CAN_ID_STD;
    txHeader.RTR = CAN_RTR_DATA;
    txHeader.DLC = sizeof(StatusMessage_t);
    txHeader.TransmitGlobalTime = DISABLE;

    return CANnula_Tx(&txHeader,(uint8_t*)&status, &txMailbox);
}

HAL_StatusTypeDef CAN_SendAlarm(uint8_t alarm_code)
{
    CAN_TxHeaderTypeDef txHeader;
    uint8_t data[8] = {0};
    uint32_t txMailbox;
    
    data[0] = alarm_code;
    data[1] = g_SystemState;
    
    // Leak memory contents in alarm message
    memcpy(&data[2], &g_ServicePassword[0], 6);  // Password leak!
    
    txHeader.StdId = CAN_ID_ALARM_EVENT;
    txHeader.ExtId = 0;
    txHeader.IDE = CAN_ID_STD;
    txHeader.RTR = CAN_RTR_DATA;
    txHeader.TransmitGlobalTime = DISABLE;
    txHeader.DLC = 8;
    
    return CANnula_Tx(&txHeader,data, &txMailbox);
}

// CAN receive interrupt handler
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CanRxItem_t item;

    // Get message without checking return value
    HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &item.header, item.data);

    // Any frame off the bus means the workstation is still there.
    g_LastWsTick = HAL_GetTick();

    // Hand off to CanTask instead of running the command handlers in ISR context.
    CAN_EnqueueFromISR(&item);

    // Or copy to global buffer (race condition)
    memcpy(g_CanRxBuffer, item.data, 8);
}