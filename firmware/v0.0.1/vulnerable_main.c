/**
 * CANnula Vulnerable Firmware v0.0.1
 * Educational vulnerable infusion pump firmware
 * WARNING: Contains intentional security vulnerabilities
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define FIRMWARE_VERSION "0.0.1"

// Vulnerable global variables
char g_password[16] = "admin123";  // Hardcoded password
uint32_t g_auth_level = 0;
uint32_t g_pump_rate = 0;
uint32_t g_total_volume = 0;
char g_debug_buffer[64];  // Small buffer for overflow

// Memory mapped peripherals for STM32F103
#define GPIOC_BASE  0x40011000
#define RCC_BASE    0x40021000
#define USART1_BASE 0x40013800
#define CAN1_BASE   0x40006400

// Simple register access
#define REG32(addr) (*(volatile uint32_t *)(addr))

// LED on PC13
void led_toggle(void) {
    REG32(GPIOC_BASE + 0x10) ^= (1 << 13);
}

// Vulnerable authentication function
int authenticate(char *input) {
    // Buffer overflow vulnerability
    char buffer[8];
    strcpy(buffer, input);  // No bounds checking!
    
    // Weak comparison
    if(strcmp(buffer, g_password) == 0) {
        g_auth_level = 2;  // Admin
        return 1;
    }
    return 0;
}

// Vulnerable command processor
void process_command(char *cmd) {
    char response[32];
    
    // Format string vulnerability
    sprintf(response, cmd);  // Direct format string!
    printf(response);
    
    // Command injection
    if(strncmp(cmd, "set_rate ", 9) == 0) {
        g_pump_rate = *(uint32_t*)(cmd + 9);  // Direct memory access
    }
    else if(strncmp(cmd, "dump ", 5) == 0) {
        uint32_t addr = *(uint32_t*)(cmd + 5);
        uint32_t *ptr = (uint32_t*)addr;
        printf("Memory[0x%08X]: 0x%08X\n", addr, *ptr);
    }
    else if(strcmp(cmd, "backdoor") == 0) {
        g_auth_level = 99;  // Super admin
        printf("Backdoor activated!\n");
    }
}

// Vulnerable CAN message handler
void handle_can_message(uint32_t id, uint8_t *data, uint8_t len) {
    // No authentication check!
    
    switch(id) {
        case 0x100:  // Set rate
            // Integer overflow possible
            g_pump_rate = data[0] | (data[1] << 8);
            g_total_volume = data[2] | (data[3] << 8);
            break;
            
        case 0x666:  // Backdoor ID
            // Direct memory write
            uint32_t *addr = (uint32_t*)(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
            *addr = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
            break;
            
        case 0x130:  // Debug command
            // Buffer overflow in debug buffer
            memcpy(g_debug_buffer, data, len);  // No size check!
            process_command(g_debug_buffer);
            break;
    }
}

// Vulnerable infusion calculation
uint32_t calculate_infusion_time(uint32_t volume, uint32_t rate) {
    // Division by zero vulnerability
    if(rate == 0) return 0xFFFFFFFF;
    
    // Integer overflow in multiplication
    uint32_t time_minutes = (volume * 60) / rate;  // Can overflow
    return time_minutes;
}

// Recursive function for stack overflow demo
void recursive_check(int depth) {
    char local_buffer[128];  // Large stack allocation
    
    if(depth < 1000) {  // Weak recursion limit
        recursive_check(depth + 1);
    }
}

// Simple delay
void delay_ms(uint32_t ms) {
    for(volatile uint32_t i = 0; i < ms * 8000; i++);
}

// Main entry point
int main(void) {
    // Enable GPIOC clock
    REG32(RCC_BASE + 0x18) |= (1 << 4);
    
    // Configure PC13 as output (LED)
    REG32(GPIOC_BASE + 0x04) = 0x44444444;
    REG32(GPIOC_BASE + 0x04) &= ~(0xF << 20);
    REG32(GPIOC_BASE + 0x04) |= (0x1 << 20);
    
    // Print vulnerable banner
    printf("CANnula Infusion Pump v%s\n", FIRMWARE_VERSION);
    printf("WARNING: Vulnerable firmware for education\n");
    printf("Password: %s\n", g_password);  // Password leak!
    
    // Main loop
    while(1) {
        led_toggle();
        delay_ms(500);
        
        // Simulate pump operation
        if(g_pump_rate > 0) {
            // No safety checks!
            printf("Infusing at %d mL/h\n", g_pump_rate);
            
            // Integer overflow in volume calculation
            g_total_volume += g_pump_rate;
        }
        
        // Check for buffer overflow condition
        if(g_auth_level == 99) {
            // Backdoor mode - execute arbitrary code
            void (*func)(void) = (void (*)(void))0x20000000;
            func();  // Jump to RAM
        }
    }
    
    return 0;
}

// Minimal interrupt vector table
__attribute__((section(".isr_vector")))
const uint32_t g_pfnVectors[] = {
    0x20005000,  // Initial stack pointer (top of 20KB RAM)
    (uint32_t)main,  // Reset handler
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // Other exceptions
    // ... more vectors
};

// Weak default handler
void Default_Handler(void) {
    while(1);
}

// Stack overflow hook (intentionally disabled)
void __stack_chk_fail(void) {
    // Do nothing - vulnerability!
}