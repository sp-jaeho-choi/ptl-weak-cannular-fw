/**
 * CANnula Secure Firmware v0.0.2
 * Hardened version with security improvements
 * Educational example of secure coding practices
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define FIRMWARE_VERSION "0.0.2-secure"

// Security constants
#define MAX_PUMP_RATE     500   // Maximum safe rate (mL/h)
#define MIN_PUMP_RATE     1     // Minimum rate
#define MAX_VOLUME        9999  // Maximum volume (mL)
#define MAX_AUTH_ATTEMPTS 3
#define SESSION_TIMEOUT   300000 // 5 minutes in ms

// Secure authentication structure
typedef struct {
    uint8_t password_hash[32];  // SHA-256 hash
    uint32_t auth_level;
    uint32_t session_token;
    uint32_t last_activity;
    uint32_t failed_attempts;
    bool locked;
} auth_state_t;

// Secure pump parameters
typedef struct {
    uint32_t rate_mlh;
    uint32_t total_volume;
    uint32_t volume_infused;
    bool ders_enabled;
    uint32_t max_rate_limit;
    uint32_t min_rate_limit;
} pump_params_t;

// CAN message authentication
typedef struct {
    uint32_t sequence_number;
    uint32_t timestamp;
    uint8_t hmac[16];  // HMAC-SHA256 truncated
} can_auth_t;

// Global state (with secure defaults)
static auth_state_t g_auth = {
    .auth_level = 0,
    .session_token = 0,
    .failed_attempts = 0,
    .locked = false
};

static pump_params_t g_pump = {
    .rate_mlh = 0,
    .total_volume = 0,
    .volume_infused = 0,
    .ders_enabled = true,
    .max_rate_limit = MAX_PUMP_RATE,
    .min_rate_limit = MIN_PUMP_RATE
};

static can_auth_t g_can_auth = {
    .sequence_number = 0,
    .timestamp = 0
};

// Memory mapped peripherals
#define GPIOC_BASE  0x40011000
#define RCC_BASE    0x40021000
#define REG32(addr) (*(volatile uint32_t *)(addr))

// Stack canary for overflow detection
static uint32_t __stack_chk_guard = 0xDEADBEEF;

// Secure string copy with bounds checking
static int secure_strcpy(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        return -1;
    }
    
    size_t src_len = strnlen(src, dest_size);
    if (src_len >= dest_size) {
        return -1;  // Source too long
    }
    
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
    return 0;
}

// Secure authentication with rate limiting
static bool authenticate(const uint8_t *password_hash, size_t hash_len) {
    // Check if account is locked
    if (g_auth.locked) {
        return false;
    }
    
    // Rate limiting
    if (g_auth.failed_attempts >= MAX_AUTH_ATTEMPTS) {
        g_auth.locked = true;
        return false;
    }
    
    // Constant-time comparison to prevent timing attacks
    if (hash_len != 32) {
        g_auth.failed_attempts++;
        return false;
    }
    
    volatile uint8_t diff = 0;
    for (size_t i = 0; i < 32; i++) {
        diff |= password_hash[i] ^ g_auth.password_hash[i];
    }
    
    if (diff == 0) {
        // Generate new session token
        g_auth.session_token = generate_random_token();
        g_auth.auth_level = 1;
        g_auth.last_activity = get_system_time();
        g_auth.failed_attempts = 0;
        return true;
    } else {
        g_auth.failed_attempts++;
        return false;
    }
}

// Validate session
static bool validate_session(uint32_t token) {
    if (g_auth.session_token == 0 || token != g_auth.session_token) {
        return false;
    }
    
    // Check session timeout
    uint32_t current_time = get_system_time();
    if (current_time - g_auth.last_activity > SESSION_TIMEOUT) {
        g_auth.session_token = 0;
        g_auth.auth_level = 0;
        return false;
    }
    
    g_auth.last_activity = current_time;
    return true;
}

// Secure command processor with input validation
static void process_command(const char *cmd, uint32_t session_token) {
    // Validate session first
    if (!validate_session(session_token)) {
        printf("ERROR: Invalid or expired session\n");
        return;
    }
    
    // Use safe parsing
    char command[32] = {0};
    uint32_t value = 0;
    
    if (sscanf(cmd, "%31s %u", command, &value) < 1) {
        printf("ERROR: Invalid command format\n");
        return;
    }
    
    // Process commands with validation
    if (strcmp(command, "set_rate") == 0) {
        if (value < g_pump.min_rate_limit || value > g_pump.max_rate_limit) {
            printf("ERROR: Rate %u out of range [%u-%u]\n", 
                   value, g_pump.min_rate_limit, g_pump.max_rate_limit);
            return;
        }
        
        // Additional DERS check
        if (g_pump.ders_enabled && !validate_ders_limits(value)) {
            printf("ERROR: DERS limit violation\n");
            return;
        }
        
        g_pump.rate_mlh = value;
        printf("OK: Rate set to %u mL/h\n", value);
        log_audit_event("RATE_CHANGE", session_token, value);
    }
    else if (strcmp(command, "get_status") == 0) {
        printf("Status: Rate=%u, Volume=%u/%u, DERS=%s\n",
               g_pump.rate_mlh, g_pump.volume_infused, g_pump.total_volume,
               g_pump.ders_enabled ? "ON" : "OFF");
    }
    else {
        printf("ERROR: Unknown command\n");
    }
}

// Secure CAN message handler with authentication
static void handle_can_message(uint32_t id, const uint8_t *data, uint8_t len) {
    // Validate message length
    if (len < sizeof(can_auth_t)) {
        return;  // Message too short for authentication
    }
    
    // Extract and verify authentication
    can_auth_t *auth = (can_auth_t *)data;
    
    // Check sequence number (prevent replay)
    if (auth->sequence_number <= g_can_auth.sequence_number) {
        log_security_event("CAN_REPLAY_ATTEMPT", id);
        return;
    }
    
    // Verify HMAC
    if (!verify_hmac(data + sizeof(can_auth_t), len - sizeof(can_auth_t), auth->hmac)) {
        log_security_event("CAN_AUTH_FAIL", id);
        return;
    }
    
    // Update sequence number
    g_can_auth.sequence_number = auth->sequence_number;
    
    // Process authenticated message
    const uint8_t *payload = data + sizeof(can_auth_t);
    uint8_t payload_len = len - sizeof(can_auth_t);
    
    switch(id) {
        case 0x100:  // Set rate (with validation)
            if (payload_len >= 2) {
                uint16_t rate = payload[0] | (payload[1] << 8);
                
                // Validate range with saturation
                if (rate > MAX_PUMP_RATE) {
                    rate = MAX_PUMP_RATE;
                } else if (rate < MIN_PUMP_RATE && rate > 0) {
                    rate = MIN_PUMP_RATE;
                }
                
                g_pump.rate_mlh = rate;
            }
            break;
            
        default:
            // Unknown ID - ignore
            break;
    }
}

// Safe calculation with overflow protection
static uint32_t calculate_infusion_time(uint32_t volume, uint32_t rate) {
    // Input validation
    if (rate == 0) {
        return UINT32_MAX;  // Return max instead of divide by zero
    }
    
    // Check for overflow before multiplication
    if (volume > UINT32_MAX / 60) {
        return UINT32_MAX;  // Would overflow
    }
    
    uint64_t time_minutes = ((uint64_t)volume * 60) / rate;
    
    // Saturate at max value
    if (time_minutes > UINT32_MAX) {
        return UINT32_MAX;
    }
    
    return (uint32_t)time_minutes;
}

// DERS validation
static bool validate_ders_limits(uint32_t rate) {
    // Implement drug-specific limits
    // This would normally check against drug library
    return (rate >= MIN_PUMP_RATE && rate <= MAX_PUMP_RATE);
}

// Generate cryptographically secure random token
static uint32_t generate_random_token(void) {
    // In real implementation, use hardware RNG
    static uint32_t lfsr = 0x12345678;
    lfsr = (lfsr >> 1) ^ (-(lfsr & 1u) & 0xD0000001u);
    return lfsr;
}

// Get system time (milliseconds)
static uint32_t get_system_time(void) {
    // In real implementation, use SysTick or RTC
    static uint32_t time = 0;
    return ++time;
}

// HMAC verification
static bool verify_hmac(const uint8_t *data, size_t len, const uint8_t *hmac) {
    // Simplified - real implementation would use crypto library
    uint8_t calculated_hmac[16] = {0};
    
    // Calculate HMAC over data
    // ... crypto calculation ...
    
    // Constant-time comparison
    volatile uint8_t diff = 0;
    for (size_t i = 0; i < 16; i++) {
        diff |= calculated_hmac[i] ^ hmac[i];
    }
    
    return (diff == 0);
}

// Audit logging
static void log_audit_event(const char *event, uint32_t token, uint32_t value) {
    printf("[AUDIT] %s: token=%08X, value=%u, time=%u\n", 
           event, token, value, get_system_time());
}

// Security event logging
static void log_security_event(const char *event, uint32_t id) {
    printf("[SECURITY] %s: id=%08X, time=%u\n", 
           event, id, get_system_time());
}

// Stack overflow detection
void __stack_chk_fail(void) {
    // Stack corruption detected!
    printf("FATAL: Stack overflow detected!\n");
    
    // Enter safe state
    g_pump.rate_mlh = 0;  // Stop pump
    g_auth.locked = true;  // Lock authentication
    
    // Trigger watchdog reset
    while(1) {
        // System will reset via watchdog
    }
}

// LED control
static void led_set(bool state) {
    if (state) {
        REG32(GPIOC_BASE + 0x10) |= (1 << 13);
    } else {
        REG32(GPIOC_BASE + 0x10) &= ~(1 << 13);
    }
}

// Safe delay
static void delay_ms(uint32_t ms) {
    for(volatile uint32_t i = 0; i < ms * 8000; i++) {
        // Check stack canary in tight loops
        if (__stack_chk_guard != 0xDEADBEEF) {
            __stack_chk_fail();
        }
    }
}

// Main entry point
int main(void) {
    // Initialize hardware
    REG32(RCC_BASE + 0x18) |= (1 << 4);  // Enable GPIOC
    
    // Configure PC13 as output (LED)
    REG32(GPIOC_BASE + 0x04) = 0x44444444;
    REG32(GPIOC_BASE + 0x04) &= ~(0xF << 20);
    REG32(GPIOC_BASE + 0x04) |= (0x1 << 20);
    
    // Initialize security
    memset(&g_auth.password_hash, 0, sizeof(g_auth.password_hash));
    // In real implementation, load from secure storage
    
    // Print secure banner
    printf("CANnula Infusion Pump v%s\n", FIRMWARE_VERSION);
    printf("Security: ENABLED\n");
    printf("DERS: ACTIVE\n");
    
    // Main loop
    while(1) {
        // Heartbeat LED (different pattern for secure version)
        static uint32_t counter = 0;
        led_set((counter++ / 500) % 2);
        delay_ms(1);
        
        // Safe pump operation
        if (g_pump.rate_mlh > 0) {
            // Check all safety conditions
            if (g_pump.volume_infused >= g_pump.total_volume) {
                g_pump.rate_mlh = 0;  // Auto-stop at volume limit
                log_audit_event("AUTO_STOP", 0, g_pump.volume_infused);
            } else {
                // Safe increment with overflow check
                uint32_t increment = g_pump.rate_mlh / 3600;  // mL per second
                if (g_pump.volume_infused + increment < g_pump.volume_infused) {
                    // Overflow would occur
                    g_pump.rate_mlh = 0;
                    log_security_event("VOLUME_OVERFLOW", 0);
                } else {
                    g_pump.volume_infused += increment;
                }
            }
        }
        
        // Periodic security checks
        if ((counter % 10000) == 0) {
            // Check stack canary
            if (__stack_chk_guard != 0xDEADBEEF) {
                __stack_chk_fail();
            }
            
            // Check session timeouts
            if (g_auth.session_token != 0) {
                validate_session(g_auth.session_token);
            }
        }
    }
    
    return 0;
}

// Secure interrupt vector table
__attribute__((section(".isr_vector")))
const uint32_t g_pfnVectors[] = {
    0x20005000,     // Initial stack pointer
    (uint32_t)main, // Reset handler
    (uint32_t)Default_Handler,  // NMI
    (uint32_t)HardFault_Handler, // HardFault
    // ... more vectors with proper handlers
};

// Secure default handler
void Default_Handler(void) {
    // Log unexpected interrupt
    log_security_event("UNEXPECTED_IRQ", 0);
    while(1);
}

// Hard fault handler with diagnostics
void HardFault_Handler(void) {
    // Enter safe state
    g_pump.rate_mlh = 0;
    
    // Log fault
    log_security_event("HARD_FAULT", 0);
    
    // Wait for watchdog reset
    while(1);
}