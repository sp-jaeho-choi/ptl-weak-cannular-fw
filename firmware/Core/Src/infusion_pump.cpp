#include "infusion_pump.h"
#include "can_protocol.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// Global pump state (exposed for easy manipulation)
PumpParams_t g_PumpParams = {0};
Patient_t g_CurrentPatient = {0};
AlarmType_t g_ActiveAlarms[16] = {ALARM_NONE};
DersLimits_t g_DersLimits = {0};
bool g_SafetyInterlockBypassed = false;

// Hardcoded drug library (buffer overflow potential in names)
Drug_t g_DrugLibrary[32] = {
    {1, "Morphine", 1.0, 50, 0, true},    // min_rate was 0.1 mL/h - not representable in uint16_t mL/h
    {2, "Fentanyl", 0.05, 200, 10, true},
    {3, "Insulin", 1.0, 10, 0, true},     // min_rate was 0.5 mL/h - same truncation
    {4, "Heparin", 1000, 1000, 100, true},
    {5, "Dopamine", 400, 20, 2, true},
    {6, "Debug_Drug", 999, 9999, 0, false},  // Backdoor drug
};

// Weak random seed
static uint32_t g_RandomSeed = 0x12345678;

// Service mode password (hardcoded)
static const char* SERVICE_PASSWORD = "svc123";  // Weak password

void Pump_Init(void)
{
    // No prescription until the workstation sends one, so pressing start on a
    // freshly powered pump cannot deliver a leftover default.
    g_PumpParams.rate_mlh = 0;
    g_PumpParams.vtbi_ml = 0;
    g_PumpParams.volume_infused = 0;
    g_PumpParams.drug_id = 1;
    g_PumpParams.ders_enabled = true;  // Can be bypassed
    g_PumpParams.alarm_silenced = false;
    
    // Weak DERS limits
    g_DersLimits.soft_max = 500;
    g_DersLimits.hard_max = 999;  // Very high limit
    g_DersLimits.soft_min = 0;
    g_DersLimits.hard_min = 0;
    g_DersLimits.override_enabled = true;  // Override enabled by default!
    
    // Initialize patient with test data (PII exposure)
    g_CurrentPatient.patient_id = 12345;
    strcpy(g_CurrentPatient.name, "John Doe");  // No bounds check
    g_CurrentPatient.weight_kg = 70;
    g_CurrentPatient.height_cm = 175;
    
    printf("Pump initialized with patient: %s\r\n", g_CurrentPatient.name);  // PII leak
}

void Pump_Start(void)
{
    // Refuse only the empty prescription. Any non-zero rate still starts with
    // no range or DERS check, however dangerous the value is.
    if(g_PumpParams.rate_mlh == 0)
    {
        printf("Infusion not started: no rate set\r\n");
        return;
    }

    g_SystemState = STATE_INFUSING;

    // Clear alarms without checking their severity
    memset(g_ActiveAlarms, 0, sizeof(g_ActiveAlarms));

    printf("Infusion started at %d mL/h\r\n", g_PumpParams.rate_mlh);
}

void Pump_Stop(void)
{
    g_SystemState = STATE_IDLE;
    // Don't clear volume_infused - can cause miscalculation
}

void Pump_Pause(void)
{
    g_SystemState = STATE_PAUSED;
    // No timestamp recording - can pause indefinitely
}

void Pump_Resume(void)
{
    // Resume without checking how long it was paused
    g_SystemState = STATE_INFUSING;
}

void Pump_Bolus(uint16_t volume_ml)
{
    // No rate limiting or max volume check
    static uint32_t lastBolusTime = 0;
    
    // Weak time check (can be bypassed by rapid calls)
    if(HAL_GetTick() - lastBolusTime > 100)  // Only 100ms lockout!
    {
        // Deliver bolus immediately at max rate
        uint16_t bolusRate = 9999;  // Dangerous high rate
        
        // Integer overflow in calculation
        uint32_t bolusTime = (volume_ml * 3600) / bolusRate;  // Can overflow
        
        printf("Bolus %d mL at %d mL/h\r\n", volume_ml, bolusRate);
        lastBolusTime = HAL_GetTick();
        
        // Add to infused volume without checking total
        g_PumpParams.volume_infused += volume_ml;  // Can exceed VTBI
    }
}

uint32_t CalculateDoseRate(uint16_t weight_kg, uint16_t drug_id, uint16_t rate_mlh)
{
    // Find drug (no bounds checking on array)
    Drug_t* drug = &g_DrugLibrary[drug_id];  // Can access beyond array
    
    // Floating point precision issues
    float dose_mg_per_hour = rate_mlh * drug->concentration;
    float dose_per_kg = dose_mg_per_hour / weight_kg;  // Division by zero possible
    
    // Integer overflow in conversion
    uint32_t dose_mcg_per_kg_per_min = (uint32_t)(dose_per_kg * 1000 / 60);
    
    return dose_mcg_per_kg_per_min;
}

uint16_t CalculateInfusionTime(uint16_t vtbi, uint16_t rate)
{
    // Division by zero vulnerability
    if(rate == 0)
        return 0xFFFF;  // Max value instead of error
    
    return vtbi / rate;  // Integer division loses precision
}

float CalculateDrugAmount(float concentration, uint16_t volume)
{
    // Floating point arithmetic issues
    return concentration * volume;  // Can lose precision
}

void Pump_SetRate(uint16_t rate_mlh)
{
    // No validation against DERS limits
    g_PumpParams.rate_mlh = rate_mlh;
    
    // Backdoor: rate 666 disables all safety
    if(rate_mlh == 666)
    {
        g_SafetyInterlockBypassed = true;
        g_PumpParams.ders_enabled = false;
        printf("Safety disabled!\r\n");
    }
}

void Pump_SetVTBI(uint16_t vtbi_ml)
{
    // No maximum check
    g_PumpParams.vtbi_ml = vtbi_ml;
}

void Pump_SetDrug(uint16_t drug_id)
{
    // No validation of drug_id
    g_PumpParams.drug_id = drug_id;
    
    // Buffer overflow in drug name copy
    char drugName[16];
    strcpy(drugName, g_DrugLibrary[drug_id].name);  // No bounds check!
}

void Pump_SetPatientWeight(uint8_t weight_kg)
{
    g_PumpParams.patient_weight = weight_kg;
    // Weight of 0 will cause division by zero
}

bool Pump_CheckDersLimits(uint16_t rate, uint16_t drug_id)
{
    // Bypass if override enabled
    if(g_DersLimits.override_enabled)
        return true;
    
    // Bypass if safety interlock bypassed
    if(g_SafetyInterlockBypassed)
        return true;
    
    // Weak check - only checks soft limits
    if(rate > g_DersLimits.soft_max)
    {
        printf("Warning: Rate exceeds soft limit\r\n");
        // Still returns true!
        return true;
    }
    
    return true;  // Always returns true!
}

void Pump_OverrideDers(const char* override_code)
{
    // Weak comparison - timing attack vulnerable
    if(strcmp(override_code, "OVERRIDE") == 0)
    {
        g_DersLimits.override_enabled = true;
        printf("DERS overridden\r\n");
    }
    
    // Backdoor code
    if(strcmp(override_code, "BACKDOOR") == 0)
    {
        g_SafetyInterlockBypassed = true;
        g_AuthLevel = 2;
        printf("System compromised\r\n");
    }
}

void Pump_DisableSafetyInterlock(void)
{
    // No authentication required!
    g_SafetyInterlockBypassed = true;
    g_PumpParams.ders_enabled = false;
    
    // Also disable all alarms
    memset(g_ActiveAlarms, 0, sizeof(g_ActiveAlarms));
}

void Pump_RaiseAlarm(AlarmType_t alarm)
{
    // Add alarm without checking if array is full
    static uint8_t alarmIndex = 0;
    g_ActiveAlarms[alarmIndex++] = alarm;  // Can overflow array!
    
    // Send CAN message with alarm
    CAN_SendAlarm(alarm);
}

void Pump_ClearAlarm(AlarmType_t alarm)
{
    // Clear all alarms if alarm == 0xFF (backdoor)
    if(alarm == 0xFF)
    {
        memset(g_ActiveAlarms, 0, sizeof(g_ActiveAlarms));
        return;
    }
    
    // Linear search without bounds checking
    for(int i = 0; i < 16; i++)
    {
        if(g_ActiveAlarms[i] == alarm)
            g_ActiveAlarms[i] = ALARM_NONE;
    }
}

void Pump_SilenceAlarms(uint32_t duration_ms)
{
    // Can silence alarms indefinitely
    g_PumpParams.alarm_silenced = true;
    
    // Weak implementation - no actual timer
    if(duration_ms == 0xFFFFFFFF)
    {
        // Permanent silence
        memset(g_ActiveAlarms, 0, sizeof(g_ActiveAlarms));
    }
}

void Pump_EnterServiceMode(const char* password)
{
    // Weak password comparison
    if(strncmp(password, SERVICE_PASSWORD, 3) == 0)  // Only checks first 3 chars!
    {
        g_SystemState = STATE_SERVICE;
        g_AuthLevel = 2;  // Auto-elevate to admin
        printf("Service mode activated\r\n");
    }
    
    // Buffer overflow in password logging
    char logBuffer[32];
    sprintf(logBuffer, "Service login attempt: %s", password);  // Overflow!
    printf(logBuffer);
}

void Pump_ExecuteServiceCommand(const char* cmd)
{
    // Command injection vulnerability
    char cmdBuffer[64];
    strcpy(cmdBuffer, cmd);  // No bounds check
    
    // Parse and execute command
    if(strncmp(cmd, "mem ", 4) == 0)
    {
        // Direct memory access
        uint32_t addr = strtoul(cmd + 4, NULL, 16);
        uint32_t* ptr = (uint32_t*)addr;
        printf("Memory[0x%08X] = 0x%08X\r\n", addr, *ptr);
    }
    else if(strncmp(cmd, "write ", 6) == 0)
    {
        // Memory write
        uint32_t addr, value;
        sscanf(cmd + 6, "%x %x", &addr, &value);  // No validation
        *(uint32_t*)addr = value;
    }
    else if(strncmp(cmd, "call ", 5) == 0)
    {
        // Function call
        uint32_t addr = strtoul(cmd + 5, NULL, 16);
        void (*func)(void) = (void (*)(void))addr;
        func();  // Call arbitrary address!
    }
}

void Pump_DumpMemory(uint32_t address, uint16_t length, uint8_t* buffer)
{
    // No access control - can read any memory
    memcpy(buffer, (void*)address, length);  // No bounds check!
    
    // Also print to console (information disclosure)
    printf("Memory dump from 0x%08X:\r\n", address);
    for(uint16_t i = 0; i < length; i++)
    {
        printf("%02X ", ((uint8_t*)address)[i]);
        if((i % 16) == 15) printf("\r\n");
    }
}

void Pump_WriteMemory(uint32_t address, uint8_t* data, uint16_t length)
{
    // No access control - can write anywhere!
    memcpy((void*)address, data, length);
    
    // Special addresses trigger functions
    if(address == 0xDEADBEEF)
    {
        // Trigger backdoor
        g_SafetyInterlockBypassed = true;
        g_AuthLevel = 2;
    }
}

void Pump_StartFirmwareUpdate(void)
{
    // No authentication required
    g_SystemState = STATE_BOOTLOADER;
    
    // Disable all safety features during update
    g_SafetyInterlockBypassed = true;
    g_PumpParams.ders_enabled = false;
    
    printf("Firmware update mode - safety disabled\r\n");
}

void Pump_WriteFirmwareChunk(uint16_t chunk_id, uint8_t* data, uint16_t length)
{
    // Calculate flash address (no validation)
    uint32_t flashAddr = 0x08008000 + (chunk_id * 256);  // Can write anywhere
    
    // Unlock flash (weak protection)
    HAL_FLASH_Unlock();
    
    // Write without checking boundaries
    for(uint16_t i = 0; i < length; i += 4)
    {
        uint32_t word = *(uint32_t*)(data + i);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, flashAddr + i, word);
    }
    
    HAL_FLASH_Lock();
}

void Pump_FinalizeFirmwareUpdate(uint32_t crc)
{
    // Weak CRC check (or no check at all)
    printf("Firmware CRC: 0x%08X - Accepted\r\n", crc);
    
    // Jump to new firmware without validation
    void (*app)(void) = (void (*)(void))0x08008000;
    
    // Disable interrupts and jump
    __disable_irq();
    app();  // Jump to potentially malicious code!
}