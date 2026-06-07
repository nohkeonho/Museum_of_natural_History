# 🏠 IoT 스마트 옷장 — Smart Wardrobe

> 의류 소재별 최적 온습도를 자동으로 유지하는 2섹션 IoT 스마트 옷장

**팀명:** 자연사 박물관  
**과목:** 사물인터넷 (2026-1학기)  
**팀원:** 노건호 (조장) · 윤상혁 · 김민건

---

## 📁 프로젝트 구조

```
smart-wardrobe/
├── README.md
├── arduino_firmware/                  # Arduino UNO 펌웨어
│   └── smart_wardrobe_firmware/
│       └── smart_wardrobe_firmware.ino
├── nodemcu_firmware/                  # NodeMCU (ESP8266) 펌웨어
│   └── nodemcu_firmware/
│       ├── nodemcu_firmware.ino
│       └── data/
│           └── index.html             # SPIFFS 웹 대시보드
└── demo/
    └── web_interface.html             # 독립 실행 데모 (NodeMCU 불필요)
```

---

## 🔧 Arduino 펌웨어

`arduino_firmware/smart_wardrobe_firmware/smart_wardrobe_firmware.ino`

Arduino UNO에 업로드하는 메인 제어 펌웨어입니다.

### 작업 영역 구분

| 영역 | 함수 | 상태 | 담당 |
|------|------|------|------|
| A | `initSections()`, `readSensors()` | 미완성 | 팀원 |
| B | `controlSection()`, `updateRamp()`, `safetyCheck()`, `setClothType()` | 미완성 | 팀원 |
| C | `handleSerial()`, `buildStatusJson()` | ✅ 완성 | 김민건 |

### 작업 방법
1. 자신의 영역 함수 안에 코드를 작성
2. 전역 변수(`secA`, `secB`, `profiles[]`)와 함수 이름은 변경하지 않기
3. `setup()`과 `loop()`는 이미 완성되어 있으므로 수정 불필요
4. 릴레이는 Active-Low: `HIGH` = OFF, `LOW` = ON

### 업로드 방법
- 보드: Arduino Uno
- 포트: Arduino가 연결된 COM 포트
- 라이브러리: DHT sensor library (설치 필요)

---

## 📡 NodeMCU 펌웨어

`nodemcu_firmware/nodemcu_firmware/nodemcu_firmware.ino`

NodeMCU (ESP8266)에 업로드하는 Wi-Fi + 웹서버 펌웨어입니다.

### 기능
- Wi-Fi 접속 + IP 자동 할당
- SPIFFS에서 웹 대시보드(index.html) 서빙
- `/status` API: 현재 온습도 + 장치 상태 (JSON)
- `/set?section=A&type=2` API: 의류 종류 변경
- Arduino UNO와 SoftwareSerial 통신
- **데모 모드**: Arduino 없이 가짜 데이터로 웹 UI 시연

### 사용 전 설정
```cpp
// Wi-Fi 정보 수정 (필수)
const char* WIFI_SSID = "실제_와이파이_이름";
const char* WIFI_PASS = "실제_와이파이_비밀번호";

// 데모 모드 (Arduino 연결 후 false로 변경)
#define DEMO_MODE true
```

### 업로드 방법

**사전 준비:**
1. Arduino IDE → 환경설정 → 추가 보드 매니저 URL에 추가:
   `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
2. 도구 → 보드 매니저 → "esp8266" 검색 → 설치
3. CP2102 드라이버 설치 (https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)

**펌웨어 업로드:**
- 보드: NodeMCU 1.0 (ESP-12E Module)
- Flash Size: 4MB (FS:2MB OTA:~1019KB)
- Upload Speed: 115200
- 업로드 버튼(→) 클릭

**SPIFFS (HTML) 업로드:**
1. ESP8266 Sketch Data Upload 플러그인 설치
   (https://github.com/esp8266/arduino-esp8266fs-plugin/releases)
2. 도구 → ESP8266 Sketch Data Upload 클릭
3. `data/index.html`이 NodeMCU 내부 플래시에 업로드됨

**접속 확인:**
- 시리얼 모니터(115200bps) 열기 → IP 주소 확인
- 스마트폰 브라우저에서 해당 IP 입력 → 대시보드 표시

---

## 🎮 데모 (독립 실행)

`demo/web_interface.html`

NodeMCU 없이 브라우저에서 바로 실행 가능한 데모 버전입니다.

### 사용법
- 파일을 더블클릭하여 브라우저에서 열기
- 또는 스마트폰으로 전송하여 열기

### 동작하는 기능
- 3초마다 온습도 실시간 시뮬레이션
- 습도 우선 순차 제어 (제습 → 가온)
- 의류 종류 변경 시 점진적 전환 (30초 데모)
- 과열 방지 (+3°C 시 히터 OFF)
- 순환팬 상시 ON 표시

---

## 📋 통신 프로토콜

Arduino ↔ NodeMCU 간 SoftwareSerial (9600bps)

| 방향 | 명령 | 설명 |
|------|------|------|
| NodeMCU → Arduino | `GET:STATUS` | 현재 상태 요청 |
| NodeMCU → Arduino | `SET:A:2` | 섹션A 의류타입을 2번(면)으로 변경 |
| Arduino → NodeMCU | `STS:{JSON}` | 상태 JSON 응답 |
| Arduino → NodeMCU | `OK` | 설정 완료 |
| Arduino → NodeMCU | `ERR:메시지` | 에러 |

### 상태 JSON 형식
```json
{
  "A": {"t":17.2, "h":43.8, "tt":16.0, "th":45.0, "p":1, "ht":0, "e":0, "c":1, "r":0},
  "B": {"t":20.5, "h":56.1, "tt":20.0, "th":55.0, "p":0, "ht":0, "e":0, "c":0, "r":0}
}
```
- `t`: 현재 온도, `h`: 현재 습도
- `tt`: 목표 온도, `th`: 목표 습도
- `p`: 펠티어 ON/OFF, `ht`: 히터 ON/OFF
- `e`: 센서 에러, `c`: 의류 타입 번호, `r`: 램프 진행 중

### 의류 타입 번호
| 번호 | 소재 | 목표 온도 | 목표 습도 | 전환 시간 |
|------|------|-----------|-----------|-----------|
| 0 | 울/니트 | 20°C | 55% | 2h |
| 1 | 가죽 | 16°C | 45% | 3h |
| 2 | 면/셔츠 | 22°C | 50% | 1h |
| 3 | 패딩 | 18°C | 40% | 2h |
| 4 | 실크 | 18°C | 50% | 3h |
| 5 | 일반 | 22°C | 50% | 1h |

---

## ⚠️ 주의사항

- 12V 전원선(펠티어/히터): **18AWG 이상** 두꺼운 전선 사용 (점퍼 와이어 사용 금지)
- 12V 어댑터: **최소 5A, 권장 10A**
- Arduino UNO 업로드 시 보드를 "Arduino Uno"로, NodeMCU 업로드 시 "NodeMCU 1.0"으로 전환
- 릴레이: Active-Low (`HIGH` = OFF, `LOW` = ON)
