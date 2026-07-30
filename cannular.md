# CANnula — 취약점 내장 인퓨전 펌프 (Embedded ↔ Desktop CAN 침투테스트 실습 프로젝트)

> **이름**: **CANnula** — "**CAN**" 버스 + 의료용 주입관 "**cannula**"의 말장난 (의료 + CAN 주제에 부합)
> **대안 이름**: DVIP (Damn Vulnerable Infusion Pump), DripSec, VulnfusePump
> **성격**: vulnerable-by-design 실습 타깃 (DVWA / OWASP Juice Shop의 임베디드·의료 버전)

## 1. 목표
Embedded ↔ Desktop을 **CAN 버스**로 연결한 의료기기 시스템에서 침투테스트를 실습한다.
인퓨전 펌프는 안전-크리티컬(용량 조작 = 환자 위해로 임팩트 명확)하고 실제 CVE·FDA
사이버보안 사례가 풍부해 교육 타깃으로 최적이다.

## 2. 시스템 구성
```
[데스크톱: 임상 워크스테이션]   Python/Qt + SocketCAN
        │  USB-CAN 어댑터 (CANable/candleLight)
     ── CAN 버스 (양 끝 120Ω 종단) ──
        │  트랜시버 SN65HVD230  (※ NUCLEO-C092RC면 온보드 내장)
[임베디드: 펌프 컨트롤러]   STM32 F446RE(or ESP32) + FreeRTOS
```
- **임베디드 = 펌프**: FreeRTOS 태스크로 모터/주입 제어, 센서, 알람, CAN 통신
- **데스크톱 = 워크스테이션**: 모니터링·파라미터 설정·알람 로그·펌웨어 업데이트

## 3. 하드웨어 / 기술 스택
| 구성 | 선택지 | 비고 |
|---|---|---|
| 펌프 MCU | **STM32 F446RE** (Cortex-M4, classic bxCAN) | RDP/부트로더/SWD 등 하드웨어 공격면 실습 풍부. 외부 트랜시버(SN65HVD230) 필요 |
| 〃 (간편) | **NUCLEO-C092RC** | **CAN FD 트랜시버 온보드 내장** → MCU측 트랜시버 배선 불필요 (단 Cortex-M0+ 저사양) |
| 〃 (무선/eFuse) | **ESP32 (WROOM-32E)** | TWAI(CAN) + WiFi/BLE + **eFuse 기반 시큐어부트/플래시암호화** 실습. JTAG은 ESP-PROG/J-Link 필요(ST-Link 불가) |
| MCU측 트랜시버 | SN65HVD230 (3.3V) | MCP2551은 5V라 레벨 주의 |
| PC측 | **USB-CAN 어댑터** (CANable/candleLight, PCAN) | PC엔 CAN 포트 없음 → 필수. SocketCAN + can-utils |
| 하드웨어 디버그 | **ST-Link V2/V3** (STM32, SWD, CN2 제거→CN4) / ESP-PROG (ESP32, JTAG) | RAM/플래시 덤프·RDP 실험용 |

> 참고: "MCU측 트랜시버 불필요"는 NUCLEO-C092RC 한정이며, **PC측 USB-CAN 어댑터는 어느 경우든 필요**.

## 4. 기능 목록 (현실적인 의료기기처럼)
### 펌프 (임베디드)
- 주입 속도(mL/h) · 총 용량(VTBI) · 환자 체중 기반 용량 계산
- **약물 라이브러리 + DERS** (Dose Error Reduction System = 용량 상·하한 가드레일)
- 알람: 폐색(occlusion), 공기 감지(air-in-line), 저용량/완료, 배터리
- 상태 텔레메트리 주기 송신 (현재 rate, 잔량, 알람 상태)
- **디버그 UART CLI** (서비스 모드: `get`/`set` 명령)
- 부트로더 / OTA 펌웨어 업데이트 수신

### 워크스테이션 (데스크톱)
- 실시간 대시보드 (속도·잔량·알람 시각화)
- 파라미터 원격 설정 (약물 선택, rate, VTBI, 체중)
- 알람 확인(ack) · 이벤트 로그 저장 (파일/DB)
- 사용자 로그인 (간호사/기사 권한)
- 펌웨어 업데이트 전송

## 5. CAN 프로토콜 예시 메시지셋 (일부러 허술하게)
| CAN ID | 방향 | 내용 |
|---|---|---|
| `0x100` | WS → 펌프 | SET_RATE (rate, VTBI, drug_id) |
| `0x101` | WS → 펌프 | ALARM_ACK / SILENCE |
| `0x110` | WS → 펌프 | START / STOP / BOLUS |
| `0x200` | 펌프 → WS | TELEMETRY (rate, volume_left, status) |
| `0x201` | 펌프 → WS | ALARM_EVENT |
| `0x300` | WS → 펌프 | FW_UPDATE_CHUNK |

## 6. 의도적 취약점 (실습 포인트)
### CAN 버스
- **인증·무결성·암호화 없음** → `0x100` 스푸핑으로 **rate 999 mL/h 주입 명령 주입** (용량 조작)
- **리플레이** → ALARM_ACK 프레임 재전송으로 **알람 영구 무음화**
- **퍼징** → 파서 크래시 유발, **버스 플러딩 / 에러프레임 injection = DoS**
- 예측 가능한 ID, 시퀀스/논스 없음

### 펌웨어 (임베디드)
- CAN 메시지 파서 **버퍼 오버플로**
- 용량 계산 **정수 오버플로 / 단위 혼동 (mg ↔ mcg)**
- **DERS 우회** (가드레일 검증 순서 결함)
- **UART CLI 명령 주입** / 인증 없는 서비스 모드
- **RDP 미설정 → 플래시 덤프** (stm32flash / esptool), **flash 내 하드코딩 키·서명키**
- **OTA 서명 검증 없음** → 악성 펌웨어 주입

### 데스크톱
- 텔레메트리 파싱 **메모리 손상 / 안전하지 않은 역직렬화**
- 로그 저장 **경로 순회(path traversal)**
- **하드코딩 크레덴셜**, 권한 분리 부재

## 7. 실습 매핑 & 도구
| 영역 | 도구 |
|---|---|
| CAN | `can-utils` (`candump`, `cansend`), **SavvyCAN**, `scapy`(CAN), **boofuzz/AFL**(퍼징) |
| 임베디드 | **Ghidra**(펌웨어 RE), **OpenOCD + SWD**(RAM/플래시 덤프·RDP 실험), esptool / stm32flash |
| 데스크톱 | SAST (cppcheck / CodeQL), 바이너리 분석 |
| SBOM/CVE | CycloneDX(cdxgen) → Grype / Trivy / osv-scanner / OWASP Dependency-Track |

## 8. ⚠️ 안전 / 윤리
- **격리된 랩(더미 부하)에서만.** 실제 의료기기·실제 환자·병원 네트워크 절대 금지.
- 모터/펌프 대신 **LED·시뮬레이션**으로 "주입"을 표현하면 물리 위험 0으로 학습 가능.
- 목적은 방어·교육(authorized self-education). 취약점은 학습용으로 의도적으로 심는 것.

## 9. 다음 단계 (택1로 코드 시작)
1. **CAN 메시지셋 상세 스펙** (바이트 레이아웃, 스케일링, 상태 비트)
2. **펌프 펌웨어 FreeRTOS 골격** (태스크 구조: CAN수신 / 제어 / 알람 / 텔레메트리 + 큐)
3. **워크스테이션 최소 UI** (SocketCAN 송수신 + 대시보드)
