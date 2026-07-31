#!/usr/bin/env python3
"""
Create a minimal valid STM32F103 binary for CANnula v0.0.1
This creates a basic firmware that will run on STM32F103C8T6
"""

import struct
import sys

# STM32F103 memory map
FLASH_BASE = 0x08000000
RAM_BASE = 0x20000000
RAM_SIZE = 0x5000  # 20KB

# Create minimal firmware
def create_firmware():
    firmware = bytearray()
    
    # Vector table (must be at start of flash)
    # First entry: initial stack pointer (top of RAM)
    firmware.extend(struct.pack('<I', RAM_BASE + RAM_SIZE))
    
    # Second entry: reset handler address (thumb mode, so LSB = 1)
    reset_handler = FLASH_BASE + 0x100 + 1  # Handler at offset 0x100
    firmware.extend(struct.pack('<I', reset_handler))
    
    # Fill other exception vectors with default handler
    default_handler = FLASH_BASE + 0x200 + 1
    for i in range(14):  # 14 more system exceptions
        firmware.extend(struct.pack('<I', default_handler))
    
    # Fill to 0x100 for reset handler
    while len(firmware) < 0x100:
        firmware.extend(b'\x00')
    
    # Reset handler code (simple infinite loop with LED blink)
    # This is thumb assembly code
    reset_handler_code = bytes([
        # Setup stack pointer
        0x4F, 0xF0, 0x00, 0x50,  # mov.w r0, #0x20005000
        0x80, 0x46,              # mov sp, r0
        
        # Enable GPIOC clock (RCC_APB2ENR |= 0x10)
        0x49, 0x48,              # ldr r0, =0x40021018
        0x01, 0x68,              # ldr r1, [r0]
        0x41, 0xF0, 0x10, 0x01,  # orr r1, #0x10
        0x01, 0x60,              # str r1, [r0]
        
        # Configure PC13 as output
        0x45, 0x48,              # ldr r0, =0x40011004
        0x44, 0x49,              # ldr r1, =0x44444444
        0x01, 0x60,              # str r1, [r0]
        
        # Main loop
        0x43, 0x48,              # ldr r0, =0x40011010  # GPIOC_ODR
        # loop:
        0x01, 0x68,              # ldr r1, [r0]
        0x81, 0xF4, 0x00, 0x51,  # eor r1, #0x2000  # Toggle PC13
        0x01, 0x60,              # str r1, [r0]
        
        # Delay loop
        0x4F, 0xF4, 0x7A, 0x72,  # mov r2, #1000000
        # delay:
        0x01, 0x3A,              # subs r2, #1
        0xFD, 0xD1,              # bne delay
        
        0xF4, 0xE7,              # b loop
        
        # Data (addresses)
        0x18, 0x10, 0x02, 0x40,  # 0x40021018 (RCC_APB2ENR)
        0x04, 0x10, 0x01, 0x40,  # 0x40011004 (GPIOC_CRH)
        0x10, 0x10, 0x01, 0x40,  # 0x40011010 (GPIOC_ODR)
        0x44, 0x44, 0x44, 0x44,  # Config value
    ])
    
    firmware.extend(reset_handler_code)
    
    # Add vulnerable signature strings
    vuln_strings = b'CANnula v0.0.1\x00admin123\x00VULNERABLE\x00'
    firmware.extend(vuln_strings)
    
    # Fill to 0x200 for default handler
    while len(firmware) < 0x200:
        firmware.extend(b'\x00')
    
    # Default handler (infinite loop)
    default_handler_code = bytes([
        0xFE, 0xE7,  # b .  (infinite loop)
    ])
    firmware.extend(default_handler_code)
    
    # Add some padding
    while len(firmware) < 0x400:
        firmware.extend(b'\xFF')
    
    return bytes(firmware)

# Create Intel HEX format
def create_hex(binary_data, base_address=0x08000000):
    hex_lines = []
    
    # Extended linear address record for upper 16 bits
    upper_addr = (base_address >> 16) & 0xFFFF
    checksum = (~(0x02 + 0x00 + 0x00 + 0x04 + (upper_addr >> 8) + (upper_addr & 0xFF)) + 1) & 0xFF
    hex_lines.append(f":02000004{upper_addr:04X}{checksum:02X}")
    
    # Data records (16 bytes per line)
    for offset in range(0, len(binary_data), 16):
        chunk = binary_data[offset:offset+16]
        record_len = len(chunk)
        addr = offset & 0xFFFF
        record_type = 0x00
        
        # Build record
        record = struct.pack('B', record_len)
        record += struct.pack('>H', addr)
        record += struct.pack('B', record_type)
        record += chunk
        
        # Calculate checksum
        checksum = (~sum(record) + 1) & 0xFF
        
        # Format as hex string
        hex_str = ':' + record.hex().upper() + f"{checksum:02X}"
        hex_lines.append(hex_str)
    
    # End of file record
    hex_lines.append(":00000001FF")
    
    return '\n'.join(hex_lines)

def main():
    print("Creating CANnula v0.0.1 vulnerable firmware...")
    
    # Create binary
    firmware = create_firmware()
    
    # Save binary file
    with open('cannula_v0.0.1.bin', 'wb') as f:
        f.write(firmware)
    print(f"Created binary: cannula_v0.0.1.bin ({len(firmware)} bytes)")
    
    # Create hex file
    hex_data = create_hex(firmware)
    with open('cannula_v0.0.1.hex', 'w') as f:
        f.write(hex_data)
    print(f"Created hex: cannula_v0.0.1.hex")
    
    # Show first few bytes
    print(f"First 32 bytes: {firmware[:32].hex()}")
    print("\nFirmware contains vulnerable signatures:")
    print("- Password: 'admin123'")
    print("- Version: 'CANnula v0.0.1'")
    print("- Status: 'VULNERABLE'")

if __name__ == '__main__':
    main()