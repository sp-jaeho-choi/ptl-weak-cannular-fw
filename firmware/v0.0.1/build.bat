@echo off
echo Building CANnula v0.0.1 (Vulnerable Version)...
echo.

REM Check if ARM toolchain is installed
where arm-none-eabi-gcc >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: ARM toolchain not found!
    echo Please install arm-none-eabi-gcc
    echo.
    echo Creating demo binary for flashing...
    
    REM Create a demo hex file with vulnerable firmware signature
    echo :020000040800F2 > cannula_v0.0.1_vulnerable.hex
    echo :10000000005000200901000000000000000000006B >> cannula_v0.0.1_vulnerable.hex
    echo :1000100000000000000000000000000000000000E0 >> cannula_v0.0.1_vulnerable.hex
    echo :100020004341416E756C612076302E302E31005F >> cannula_v0.0.1_vulnerable.hex
    echo :1000300061646D696E313233000000000000000093 >> cannula_v0.0.1_vulnerable.hex
    echo :00000001FF >> cannula_v0.0.1_vulnerable.hex
    
    echo Demo HEX file created: cannula_v0.0.1_vulnerable.hex
    echo This is a minimal demo binary for testing flash procedure.
    exit /b 0
)

REM Actual build with toolchain
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O0 -fno-stack-protector -c vulnerable_main.c -o vulnerable_main.o
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Tlink.ld -nostartfiles -o cannula_v0.0.1_vulnerable.elf vulnerable_main.o
arm-none-eabi-objcopy -O ihex cannula_v0.0.1_vulnerable.elf cannula_v0.0.1_vulnerable.hex
arm-none-eabi-objcopy -O binary cannula_v0.0.1_vulnerable.elf cannula_v0.0.1_vulnerable.bin
arm-none-eabi-size cannula_v0.0.1_vulnerable.elf

echo.
echo Build complete!
echo Output files:
echo   - cannula_v0.0.1_vulnerable.hex (for STM32CubeProgrammer)
echo   - cannula_v0.0.1_vulnerable.bin (binary format)
echo   - cannula_v0.0.1_vulnerable.elf (with debug symbols)