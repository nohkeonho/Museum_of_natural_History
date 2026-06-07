/*
 * IoT 스마트 옷장 — Arduino UNO 펌웨어
 * 작업 영역 구분 및 뼈대
 * 
 * [작업 영역 A] 센서 + 프로파일 초기화
 * [작업 영역 B] 제어 로직 + 점진적 전환 + 안전장치
 * [작업 영역 C] NodeMCU 시리얼 통신
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

// [작업 영역 A]
void initSections();
void readSensors();

// [작업 영역 B]
void controlSection(Section &sec, int relayPelt, int relayHeat);
void updateRamp(Section &sec);
void safetyCheck(Section &sec, int relayHeat);
void setClothType(Section &sec, int type);

// [작업 영역 C]
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
// [작업 영역 A] 센서 + 프로파일 초기화
// ============================================================

void initSections() {

}

void readSensors() {

}

// ============================================================
// [작업 영역 B] 제어 로직 + 점진적 전환 + 안전장치
// ============================================================

void controlSection(Section &sec, int relayPelt, int relayHeat) {

}

void updateRamp(Section &sec) {

}

void safetyCheck(Section &sec, int relayHeat) {

}

void setClothType(Section &sec, int type) {

}

// ============================================================
// [작업 영역 C] NodeMCU 시리얼 통신 — 완성
//
// 프로토콜:
//   NodeMCU → Arduino: "SET:A:2"      (섹션A 의류타입을 2번(면)으로 변경)
//   NodeMCU → Arduino: "GET:STATUS"   (현재 상태 요청)
//   Arduino → NodeMCU: "STS:{JSON}"   (상태 응답)
//   Arduino → NodeMCU: "OK"           (설정 완료 응답)
//   Arduino → NodeMCU: "ERR:메시지"    (에러 응답)
// ============================================================

void handleSerial() {
    if (!nodeSerial.available()) return;

    String cmd = nodeSerial.readStringUntil('\n');
    cmd.trim();

    if (cmd.length() == 0) return;

    Serial.print("[RX] ");
    Serial.println(cmd);

    if (cmd == "GET:STATUS") {
        String json = buildStatusJson();
        nodeSerial.println("STS:" + json);
        Serial.print("[TX] STS:");
        Serial.println(json);
        return;
    }

    if (cmd.startsWith("SET:")) {
        int firstColon = cmd.indexOf(':', 0);
        int secondColon = cmd.indexOf(':', firstColon + 1);

        if (firstColon < 0 || secondColon < 0) {
            nodeSerial.println("ERR:INVALID_FORMAT");
            return;
        }

        String section = cmd.substring(firstColon + 1, secondColon);
        String typeStr = cmd.substring(secondColon + 1);
        int newType = typeStr.toInt();

        if (newType < 0 || newType >= PROFILE_COUNT) {
            nodeSerial.println("ERR:INVALID_TYPE");
            return;
        }

        if (section == "A") {
            setClothType(secA, newType);
            nodeSerial.println("OK");
            Serial.print("[SET] Section A -> ");
            Serial.println(profiles[newType].name);
        } else if (section == "B") {
            setClothType(secB, newType);
            nodeSerial.println("OK");
            Serial.print("[SET] Section B -> ");
            Serial.println(profiles[newType].name);
        } else {
            nodeSerial.println("ERR:INVALID_SECTION");
        }
        return;
    }

    nodeSerial.println("ERR:UNKNOWN_CMD");
}

String buildStatusJson() {
    String json = "{";

    json += "\"A\":{";
    json += "\"t\":" + String(secA.currentTemp, 1) + ",";
    json += "\"h\":" + String(secA.currentHum, 1) + ",";
    json += "\"tt\":" + String(secA.targetTemp, 1) + ",";
    json += "\"th\":" + String(secA.targetHum, 1) + ",";
    json += "\"p\":" + String(secA.peltierOn ? 1 : 0) + ",";
    json += "\"ht\":" + String(secA.heaterOn ? 1 : 0) + ",";
    json += "\"e\":" + String(secA.sensorError ? 1 : 0) + ",";
    json += "\"c\":" + String(secA.clothType) + ",";
    json += "\"r\":" + String(secA.ramping ? 1 : 0);
    json += "},";

    json += "\"B\":{";
    json += "\"t\":" + String(secB.currentTemp, 1) + ",";
    json += "\"h\":" + String(secB.currentHum, 1) + ",";
    json += "\"tt\":" + String(secB.targetTemp, 1) + ",";
    json += "\"th\":" + String(secB.targetHum, 1) + ",";
    json += "\"p\":" + String(secB.peltierOn ? 1 : 0) + ",";
    json += "\"ht\":" + String(secB.heaterOn ? 1 : 0) + ",";
    json += "\"e\":" + String(secB.sensorError ? 1 : 0) + ",";
    json += "\"c\":" + String(secB.clothType) + ",";
    json += "\"r\":" + String(secB.ramping ? 1 : 0);
    json += "}";

    json += "}";
    return json;
}
