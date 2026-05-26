/*
 * IoT 스마트 옷장 — Arduino UNO 펌웨어
 * 팀 이름: 자연사 박물관
 * * [현재 진행도] [작업 영역 A] 센서 + 프로파일 초기화 구현 완료
 * (B, C 영역은 인터페이스 약속만 정의된 상태입니다.)
 */

#include <DHT.h>
#include <SoftwareSerial.h>

// ============================================================
// 핀 배정
// ============================================================
#define DHT_PIN_A     2
#define DHT_PIN_B     3
#define RELAY_PELT_A  4
#define RELAY_PELT_B  5
#define RELAY_HEAT_A  6
#define RELAY_HEAT_B  7
#define NODE_RX       10
#define NODE_TX       11

#define DHT_TYPE DHT22

// ============================================================
// 주기 설정 (밀리초)
// ============================================================
#define SENSOR_INTERVAL   5000
#define CONTROL_INTERVAL  10000
#define RAMP_INTERVAL     60000

// ============================================================
// 안전 상수
// ============================================================
#define OVERHEAT_MARGIN   3.0

// ============================================================
// 의류 프로파일 구조체
// ============================================================
struct ClothProfile {
    const char* name;
    float targetTemp;
    float targetHum;
    float tolTemp;
    float tolHum;
    unsigned long rampTime;
};

const ClothProfile profiles[] = {
    {"wool",    20.0, 55.0, 2.0, 5.0, 7200000},
    {"leather", 16.0, 45.0, 2.0, 5.0, 10800000},
    {"cotton",  22.0, 50.0, 3.0, 5.0, 3600000},
    {"padding", 18.0, 40.0, 2.0, 5.0, 7200000},
    {"silk",    18.0, 50.0, 2.0, 5.0, 10800000},
    {"normal",  22.0, 50.0, 3.0, 8.0, 3600000}
};

#define PROFILE_COUNT 6

// ============================================================
// 섹션 상태 구조체
// ============================================================
struct Section {
    float currentTemp;
    float currentHum;
    float targetTemp;
    float targetHum;
    float rampStartTemp;
    float rampStartHum;
    float rampEndTemp;
    float rampEndHum;
    unsigned long rampStartTime;
    bool ramping;
    bool peltierOn;
    bool heaterOn;
    bool sensorError;
    int clothType;
};

// ============================================================
// 전역 변수
// ============================================================
DHT dhtA(DHT_PIN_A, DHT_TYPE);
DHT dhtB(DHT_PIN_B, DHT_TYPE);
SoftwareSerial nodeSerial(NODE_RX, NODE_TX);

Section secA;
Section secB;

unsigned long lastSensor  = 0;
unsigned long lastControl = 0;
unsigned long lastRamp    = 0;

// ============================================================
// 함수 선언
// ============================================================
void initSections();
void readSensors();
void controlSection(Section &sec, int relayPelt, int relayHeat);
void updateRamp(Section &sec);
void safetyCheck(Section &sec, int relayHeat);
void setClothType(Section &sec, int type);
void handleSerial();
String buildStatusJson();

// ============================================================
// setup
// ============================================================
void setup() {
    Serial.begin(9600);
    nodeSerial.begin(9600);

    dhtA.begin();
    dhtB.begin();

    pinMode(RELAY_PELT_A, OUTPUT);
    pinMode(RELAY_PELT_B, OUTPUT);
    pinMode(RELAY_HEAT_A, OUTPUT);
    pinMode(RELAY_HEAT_B, OUTPUT);

    digitalWrite(RELAY_PELT_A, HIGH);
    digitalWrite(RELAY_PELT_B, HIGH);
    digitalWrite(RELAY_HEAT_A, HIGH);
    digitalWrite(RELAY_HEAT_B, HIGH);

    initSections();
}

// ============================================================
// loop
// ============================================================
void loop() {
    unsigned long now = millis();

    // 5초마다 센서 읽기 실행 (작업 영역 A)
    if (now - lastSensor >= SENSOR_INTERVAL) {
        readSensors();
        lastSensor = now;
    }

    if (now - lastRamp >= RAMP_INTERVAL) {
        updateRamp(secA);
        updateRamp(secB);
        lastRamp = now;
    }

    if (now - lastControl >= CONTROL_INTERVAL) {
        safetyCheck(secA, RELAY_HEAT_A);
        safetyCheck(secB, RELAY_HEAT_B);
        controlSection(secA, RELAY_PELT_A, RELAY_HEAT_A);
        controlSection(secB, RELAY_PELT_B, RELAY_HEAT_B);
        lastControl = now;
    }

    handleSerial();
}

// ============================================================
// [작업 영역 A] 센서 + 프로파일 초기화 (구현 완료 🚀)
// ============================================================
void initSections() {
    delay(2000); // DHT22 부팅 안정화 대기

    // 임시로 하드코딩 초기화 (B영역의 setClothType이 비어있으므로 직접 대입)
    secA.clothType = 5; // normal 프로파일
    secA.targetTemp = profiles[5].targetTemp;
    secA.targetHum = profiles[5].targetHum;
    secA.peltierOn = false;
    secA.heaterOn = false;
    secA.sensorError = false;

    secB.clothType = 5; 
    secB.targetTemp = profiles[5].targetTemp;
    secB.targetHum = profiles[5].targetHum;
    secB.peltierOn = false;
    secB.heaterOn = false;
    secB.sensorError = false;

    readSensors();
    Serial.println("System A-Part Initialized. Ready.");
}

void readSensors() {
    float tA = dhtA.readTemperature();
    float hA = dhtA.readHumidity();
    float tB = dhtB.readTemperature();
    float hB = dhtB.readHumidity();

    // 섹션 A 검증
    if (isnan(tA) || isnan(hA)) {
        secA.sensorError = true;
        Serial.println("[WARN] Section A Sensor Error!");
    } else {
        secA.sensorError = false;
        secA.currentTemp = tA;
        secA.currentHum = hA;
    }

    // 섹션 B 검증
    if (isnan(tB) || isnan(hB)) {
        secB.sensorError = true;
        Serial.println("[WARN] Section B Sensor Error!");
    } else {
        secB.sensorError = false;
        secB.currentTemp = tB;
        secB.currentHum = hB;
    }

    // 모니터 출력
    Serial.print("SecA -> T: "); Serial.print(secA.currentTemp, 1);
    Serial.print("C / H: "); Serial.print(secA.currentHum, 1);
    Serial.print("% | SecB -> T: "); Serial.print(secB.currentTemp, 1);
    Serial.print("C / H: "); Serial.print(secB.currentHum, 1); Serial.println("%");
}

// ============================================================
// [작업 영역 B] 제어 로직 + 점진적 전환 + 안전장치 (인터페이스 대기)
// ============================================================
void controlSection(Section &sec, int relayPelt, int relayHeat) {
    // 뼈대 유지 (비어있음)
}

void updateRamp(Section &sec) {
    // 뼈대 유지 (비어있음)
}

void safetyCheck(Section &sec, int relayHeat) {
    // 뼈대 유지 (비어있음)
}

void setClothType(Section &sec, int type) {
    // 뼈대 유지 (비어있음)
}

// ============================================================
// [작업 영역 C] NodeMCU 시리얼 통신 (인터페이스 대기)
// ============================================================
void handleSerial() {
    // 뼈대 유지 (비어있음)
}

String buildStatusJson() {
    return ""; // 뼈대 유지 (비어있음)
}