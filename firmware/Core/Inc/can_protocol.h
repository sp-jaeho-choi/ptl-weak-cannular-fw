#ifndef __CAN_PROTOCOL_H
#define __CAN_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

// CAN Message IDs (predictable and sequential - vulnerability)
#define CAN_ID_SET_RATE         0x100
#define CAN_ID_ALARM_ACK        0x101
#define CAN_ID_PUMP_CONTROL     0x110
#define CAN_ID_AUTH_REQUEST     0x120
#define CAN_ID_DEBUG_CMD        0x130
#define CAN_ID_FW_UPDATE        0x140

#define CAN_ID_TELEMETRY        0x200
#define CAN_ID_ALARM_EVENT      0x201
#define CAN_ID_STATUS           0x202
#define CAN_ID_DEBUG_RESPONSE   0x203
#define CAN_ID_MEMORY_DUMP      0x300  // Vulnerability: Direct memory access

// Command codes
#define CMD_START       0x01
#define CMD_STOP        0x02
#define CMD_BOLUS       0x03
#define CMD_SET_PARAMS  0x04
#define CMD_GET_STATUS  0x05
#define CMD_DUMP_MEM    0x06  // Dangerous command
#define CMD_EXEC        0x07  // Command injection vulnerability

// Vulnerable message structure (no authentication/integrity)
typedef struct {
    uint16_t rate_mlh;      // mL/hour (no validation)
    uint16_t vtbi_ml;       // Volume to be infused
    uint16_t drug_id;
    uint8_t patient_weight; // kg
    uint8_t reserved;       // Padding
} __attribute__((packed)) SetRateMessage_t;

typedef struct {
    uint8_t command;
    uint8_t auth_token;     // Weak 8-bit token
    uint8_t data[6];        // Variable data
} __attribute__((packed)) ControlMessage_t;

typedef struct {
    uint32_t address;       // Memory address to dump
    uint16_t length;        // Bytes to read
    uint8_t reserved[2];
} __attribute__((packed)) MemoryDumpRequest_t;

// Vulnerable debug command (format string + buffer overflow potential)
typedef struct {
    char command[32];       // No null termination guarantee
    char args[32];          // TODO: Add input validation
} __attribute__((packed)) DebugCommand_t;

// Telemetry (sent every second)
typedef struct {
    uint16_t current_rate;
    uint16_t volume_infused;
    uint16_t volume_remaining;
    uint8_t battery_percent;
    uint8_t alarm_code;
} __attribute__((packed)) TelemetryMessage_t;

// Status (sent every second alongside telemetry)
typedef struct {
    uint8_t state;
    uint8_t auth_level;
    uint8_t ders_enabled;
    uint8_t safety_bypassed;
    uint16_t vtbi_ml;
    uint16_t ders_hard_max;
} __attribute__((packed)) StatusMessage_t;

// Alarm acknowledge / silence (no sequence number - replayable)
typedef struct {
    uint8_t alarm_code;
    uint8_t silence;
    uint8_t reserved[6];
} __attribute__((packed)) AlarmAckMessage_t;

// Firmware update chunk (no signature verification)
typedef struct {
    uint16_t chunk_id;
    uint16_t total_chunks;
    uint32_t crc32;         // Weak CRC instead of crypto signature
    uint8_t data[48];       // Firmware data
} __attribute__((packed)) FirmwareChunk_t;

// One received frame as it travels from the RX interrupt to CanTask.
typedef struct {
    CAN_RxHeaderTypeDef header;
    uint8_t data[8];
} CanRxItem_t;

// ---------------------------------------------------------------------------
//  SWD mailbox - a CAN transport that runs over the debug port.
//
//  With no USB-CAN adapter or transceiver on the bench, a host tool attached
//  through ST-LINK can still exchange frames with this firmware by reading and
//  writing these two rings in RAM. Injected frames go through exactly the same
//  handlers as frames that arrive on the real bus.
// ---------------------------------------------------------------------------
#define SWD_MB_MAGIC   0x434E4D4Cu   // "CNML" little-endian
#define SWD_MB_VERSION 1u
#define SWD_MB_SLOTS   4u            // must be a power of two

typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  rsv;
    uint8_t  data[8];
    uint8_t  pad[2];
} __attribute__((packed)) SwdFrame_t;   // 16 bytes

typedef struct {
    uint32_t magic;
    uint32_t version;
    volatile uint32_t host_head;   // host writes rx[], then increments
    volatile uint32_t fw_tail;     // firmware consumes rx[], then increments
    volatile uint32_t fw_head;     // firmware writes tx[], then increments
    volatile uint32_t host_tail;   // host consumes tx[], then increments
    SwdFrame_t rx[SWD_MB_SLOTS];   // host -> firmware
    SwdFrame_t tx[SWD_MB_SLOTS];   // firmware -> host
} SwdMailbox_t;

extern SwdMailbox_t g_SwdMailbox;

// HAL_GetTick() of the last frame received from the workstation. Zero until the
// first one arrives. PumpTask stops infusing if this goes stale.
extern volatile uint32_t g_LastWsTick;
#define WS_LINK_TIMEOUT_MS 6000u

void SwdMb_Init(void);
void SwdMb_Poll(void);   // drain host->firmware ring, dispatch as CAN frames

// Sends on the real bus and mirrors into the SWD mailbox.
HAL_StatusTypeDef CANnula_Tx(CAN_TxHeaderTypeDef *header, uint8_t *data, uint32_t *mailbox);

// Global CAN buffers (stack allocated - overflow risk)
extern uint8_t g_CanRxBuffer[256];  // Oversized buffer
extern uint8_t g_CanTxBuffer[256];

// Function prototypes
HAL_StatusTypeDef CAN_Init(void);
HAL_StatusTypeDef CAN_ProcessMessage(CAN_RxHeaderTypeDef *header, uint8_t *data);
HAL_StatusTypeDef CAN_SendTelemetry(void);
HAL_StatusTypeDef CAN_SendStatus(void);
HAL_StatusTypeDef CAN_SendAlarm(uint8_t alarm_code);
void CAN_EnqueueFromISR(const CanRxItem_t *item);  // defined in main.cpp, owns the queue
void CAN_HandleAlarmAck(uint8_t *data);
void CAN_HandleSetRate(uint8_t *data);      // No bounds checking
void CAN_HandleControl(uint8_t *data);
void CAN_HandleDebug(uint8_t *data);        // Command injection risk
void CAN_HandleMemoryDump(uint8_t *data);   // Direct memory access
void CAN_HandleFirmwareUpdate(uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __CAN_PROTOCOL_H */