#ifndef __INFUSION_PUMP_H
#define __INFUSION_PUMP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>

// Drug library (hardcoded - security risk)
typedef struct {
    uint16_t drug_id;
    char name[16];          // Buffer overflow risk
    float concentration;    // mg/mL
    uint16_t max_rate;      // mL/h
    uint16_t min_rate;      // mL/h
    bool high_alert;
} Drug_t;

// Patient data (stored in plain text)
typedef struct {
    uint32_t patient_id;
    char name[32];          // PII in plaintext
    uint8_t weight_kg;
    uint8_t height_cm;
    uint16_t allergies[8];  // Drug IDs
} Patient_t;

// Pump parameters (vulnerable to integer overflow)
typedef struct {
    uint16_t rate_mlh;          // Current infusion rate
    uint16_t vtbi_ml;           // Total volume to infuse
    uint16_t volume_infused;    // Volume already infused
    uint32_t start_time;        // ms since boot
    uint16_t drug_id;
    float drug_concentration;   // Floating point arithmetic risks
    uint8_t patient_weight;
    uint8_t occlusion_pressure; // PSI
    bool ders_enabled;          // Dose error reduction (can be bypassed)
    bool alarm_silenced;
} PumpParams_t;

// Alarm types
typedef enum {
    ALARM_NONE = 0,
    ALARM_OCCLUSION,
    ALARM_AIR_IN_LINE,
    ALARM_LOW_BATTERY,
    ALARM_EMPTY_RESERVOIR,
    ALARM_DOSE_LIMIT_HIGH,
    ALARM_DOSE_LIMIT_LOW,
    ALARM_SYSTEM_FAULT,
    ALARM_DOOR_OPEN,
    ALARM_FLOW_ERROR
} AlarmType_t;

// DERS limits (can be bypassed via debug commands)
typedef struct {
    uint16_t soft_max;      // Warning limit
    uint16_t hard_max;      // Absolute max (bypassable)
    uint16_t soft_min;
    uint16_t hard_min;
    bool override_enabled;  // Service mode bypass
} DersLimits_t;

// Global pump state (exposed globally - bad practice)
extern PumpParams_t g_PumpParams;
extern Patient_t g_CurrentPatient;
extern Drug_t g_DrugLibrary[32];  // Fixed size array
extern AlarmType_t g_ActiveAlarms[16];
extern DersLimits_t g_DersLimits;
extern bool g_SafetyInterlockBypassed;  // Dangerous flag

// Vulnerable calculation functions (integer overflow risks)
uint32_t CalculateDoseRate(uint16_t weight_kg, uint16_t drug_id, uint16_t rate_mlh);
uint16_t CalculateInfusionTime(uint16_t vtbi, uint16_t rate);  // Division by zero risk
float CalculateDrugAmount(float concentration, uint16_t volume);  // Float precision issues

// State machine functions
void Pump_Init(void);
void Pump_Start(void);
void Pump_Stop(void);
void Pump_Pause(void);
void Pump_Resume(void);
void Pump_Bolus(uint16_t volume_ml);  // No rate limiting

// Parameter setters (no validation)
void Pump_SetRate(uint16_t rate_mlh);
void Pump_SetVTBI(uint16_t vtbi_ml);
void Pump_SetDrug(uint16_t drug_id);
void Pump_SetPatientWeight(uint8_t weight_kg);

// DERS functions (bypassable)
bool Pump_CheckDersLimits(uint16_t rate, uint16_t drug_id);
void Pump_OverrideDers(const char* override_code);  // Weak authentication
void Pump_DisableSafetyInterlock(void);  // Dangerous function

// Alarm functions
void Pump_RaiseAlarm(AlarmType_t alarm);
void Pump_ClearAlarm(AlarmType_t alarm);
void Pump_SilenceAlarms(uint32_t duration_ms);
void Pump_TestAlarms(void);

// Service mode (backdoor)
void Pump_EnterServiceMode(const char* password);  // Hardcoded password
void Pump_ExecuteServiceCommand(const char* cmd);  // Command injection

// Memory operations (dangerous)
void Pump_DumpMemory(uint32_t address, uint16_t length, uint8_t* buffer);
void Pump_WriteMemory(uint32_t address, uint8_t* data, uint16_t length);

// Update functions (no signature verification)
void Pump_StartFirmwareUpdate(void);
void Pump_WriteFirmwareChunk(uint16_t chunk_id, uint8_t* data, uint16_t length);
void Pump_FinalizeFirmwareUpdate(uint32_t crc);  // Weak CRC check only

#ifdef __cplusplus
}
#endif

#endif /* __INFUSION_PUMP_H */