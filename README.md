# CANnula Vulnerable Firmware - STM32F103C8T6

교육용 취약 인퓨전 펌프 펌웨어 (보안 실습용)

⚠️ **경고**: 이 펌웨어는 의도적으로 보안 취약점을 포함하고 있습니다. 실제 제품이나 프로덕션 환경에서 절대 사용하지 마십시오.

## 하드웨어 요구사항

- STM32F103C8T6 (Blue Pill)
- ST-Link V2/V3 디버거
- CAN 트랜시버 (SN65HVD230 또는 MCP2551)
- USB-UART 변환기 (디버깅용)

## 핀 연결

| 기능 | STM32 핀 | 설명 |
|------|----------|------|
| LED | PC13 | 상태 표시 LED |
| CAN_RX | PB8 | CAN 수신 (리맵) |
| CAN_TX | PB9 | CAN 송신 (리맵) |
| UART_TX | PA9 | 디버그 콘솔 TX |
| UART_RX | PA10 | 디버그 콘솔 RX |

## 빌드 환경 설정

### Windows (MSYS2/MinGW)

1. ARM GCC 툴체인 설치:
```bash
# MSYS2에서
pacman -S mingw-w64-x86_64-arm-none-eabi-gcc
```

또는 [ARM 공식 사이트](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm)에서 다운로드

2. 환경 변수 설정:
```bash
export PATH=$PATH:/c/Program\ Files\ \(x86\)/GNU\ Arm\ Embedded\ Toolchain/10\ 2021.10/bin
```

### Linux/macOS

```bash
# Ubuntu/Debian
sudo apt-get install gcc-arm-none-eabi

# macOS
brew install arm-none-eabi-gcc
```

## 빌드 방법

```bash
cd firmware
make clean
make all
```

빌드 성공 시 `build/` 디렉토리에 다음 파일들이 생성됩니다:
- `cannula_vuln_fw.elf` - 디버깅용 ELF 파일
- `cannula_vuln_fw.hex` - Intel HEX 형식
- `cannula_vuln_fw.bin` - 바이너리 형식

## 플래싱 방법

### STM32CubeProgrammer CLI 사용 (권장)

```bash
make flash
```

또는 수동으로:

```bash
"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" -c port=SWD -w build/cannula_vuln_fw.hex -v -rst
```

### OpenOCD 사용

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/cannula_vuln_fw.elf verify reset exit"
```

## CAN 통신 프로토콜

### CAN 메시지 ID

| ID | 방향 | 설명 |
|----|------|------|
| 0x100 | PC→MCU | 주입 속도 설정 |
| 0x101 | PC→MCU | 알람 확인 |
| 0x110 | PC→MCU | 펌프 제어 (시작/정지/볼루스) |
| 0x120 | PC→MCU | 인증 요청 |
| 0x130 | PC→MCU | 디버그 명령 |
| 0x140 | PC→MCU | 펌웨어 업데이트 |
| 0x200 | MCU→PC | 텔레메트리 데이터 |
| 0x201 | MCU→PC | 알람 이벤트 |
| 0x300 | MCU→PC | 메모리 덤프 응답 |

### 디버그 콘솔 (UART 115200 8N1)

연결 후 사용 가능한 명령:
```
help                    - 명령어 목록
set <param> <value>    - 파라미터 설정
dump                   - 시크릿 덤프
reset                  - 시스템 리셋
```

## 주요 기능

1. **인퓨전 제어**
   - 주입 속도 설정 (0-9999 mL/h)
   - VTBI (총 주입량) 설정
   - 볼루스 주입

2. **약물 라이브러리**
   - 6가지 약물 프리셋
   - 농도 계산
   - DERS (용량 오류 감소 시스템)

3. **알람 시스템**
   - 폐색 감지
   - 공기 감지
   - 배터리 부족
   - 주입 완료

4. **서비스 모드**
   - 파라미터 직접 설정
   - 메모리 덤프
   - 펌웨어 업데이트

## 보안 취약점 (교육용)

이 펌웨어는 다음과 같은 의도적 취약점을 포함합니다:

- 버퍼 오버플로우
- 포맷 스트링 취약점
- 정수 오버플로우
- 인증 우회
- 명령 주입
- 메모리 직접 접근
- 하드코딩된 크레덴셜
- 레이스 컨디션
- 스택 오버플로우
- CRC만 사용한 펌웨어 검증

## 트러블슈팅

### 연결 실패
- ST-Link 드라이버 설치 확인
- BOOT0 핀이 GND에 연결되어 있는지 확인
- 리셋 버튼을 누른 상태에서 플래싱 시도

### 빌드 실패
- ARM GCC 툴체인 경로 확인
- 필요한 HAL 라이브러리 파일 확인

## 라이선스

교육 목적으로만 사용. 실제 의료 기기나 프로덕션 환경에서 사용 금지.

## 면책 조항

이 펌웨어는 보안 교육 목적으로 설계되었으며, 의도적으로 취약점을 포함하고 있습니다. 실제 환경에서 사용 시 발생하는 모든 책임은 사용자에게 있습니다.