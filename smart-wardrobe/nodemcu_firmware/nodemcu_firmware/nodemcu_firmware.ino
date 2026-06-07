/*
 * IoT 스마트 옷장 — NodeMCU (ESP8266) 펌웨어
 * 
 * 역할:
 *   1. Wi-Fi 접속 + IP 할당
 *   2. 웹서버 운영 (SPIFFS에서 HTML 서빙)
 *   3. API 엔드포인트 (/status, /set)
 *   4. Arduino UNO와 SoftwareSerial 통신 (명령 중계)
 *   5. 데모 모드: Arduino 미연결 시 가짜 데이터로 웹 UI 시연
 *
 * 보드 설정 (Arduino IDE):
 *   - 보드: NodeMCU 1.0 (ESP-12E Module)
 *   - Flash Size: 4MB (FS:2MB OTA:~1019KB)
 *   - Upload Speed: 115200
 *
 * 사용법:
 *   - DEMO_MODE = true  → Arduino 없이 가짜 데이터로 웹 UI 시연
 *   - DEMO_MODE = false → Arduino와 연동하여 실제 센서 데이터 사용
 *   - Arduino 연결 후 false로 바꾸고 다시 업로드하면 됩니다.
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <SoftwareSerial.h>

// ============================================================
// 데모 모드 설정 — Arduino 연결 후 false로 변경
// ============================================================
#define DEMO_MODE true

// ============================================================
// Wi-Fi 설정 — 사용 환경에 맞게 수정
// ============================================================
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// ============================================================
// 핀 배정 (NodeMCU 기준)
// ============================================================
#define ARD_RX D2   // Arduino TX(D11) → NodeMCU RX
#define ARD_TX D3   // Arduino RX(D10) ← NodeMCU TX

// ============================================================
// 전역 객체
// ============================================================
ESP8266WebServer server(80);
SoftwareSerial ardSerial(ARD_RX, ARD_TX);

// 최근 상태 캐시 (Arduino에서 받은 JSON)
String lastStatus = "{}";
unsigned long lastStatusTime = 0;
#define STATUS_CACHE_MS 3000  // 3초 캐시

// 데모 모드 상태
int demoClothA = 1;   // 기본: 가죽
int demoClothB = 0;   // 기본: 울/니트

// ============================================================
// 데모 모드 — Arduino 미연결 시 가짜 데이터 생성
// ============================================================
String buildDemoJson() {
    // 시간에 따라 미세하게 변하는 가짜 센서값
    float wave = sin(millis() / 10000.0);
    float wave2 = cos(millis() / 8000.0);

    // 의류 프로파일 목표값 (Arduino 쪽 profiles[]와 동일)
    float targetTemps[] = {20.0, 16.0, 22.0, 18.0, 18.0, 22.0};
    float targetHums[]  = {55.0, 45.0, 50.0, 40.0, 50.0, 50.0};

    float ttA = targetTemps[demoClothA];
    float thA = targetHums[demoClothA];
    float ttB = targetTemps[demoClothB];
    float thB = targetHums[demoClothB];

    // 현재 센서값 (목표 근처에서 미세 변동)
    float tA = ttA + wave * 1.5 + 0.3;
    float hA = thA + wave2 * 3.0 + 1.5;
    float tB = ttB + wave2 * 1.2 - 0.2;
    float hB = thB + wave * 2.5 + 2.0;

    // 습도 우선 순차 제어 시뮬레이션
    int pA = (hA > thA + 5.0) ? 1 : 0;
    int htA = (!pA && tA < ttA - 2.0) ? 1 : 0;
    int pB = (hB > thB + 5.0) ? 1 : 0;
    int htB = (!pB && tB < ttB - 2.0) ? 1 : 0;

    // 과열 방지
    if (tA > ttA + 3.0) htA = 0;
    if (tB > ttB + 3.0) htB = 0;

    String json = "{";
    json += "\"A\":{";
    json += "\"t\":" + String(tA, 1) + ",";
    json += "\"h\":" + String(hA, 1) + ",";
    json += "\"tt\":" + String(ttA, 1) + ",";
    json += "\"th\":" + String(thA, 1) + ",";
    json += "\"p\":" + String(pA) + ",";
    json += "\"ht\":" + String(htA) + ",";
    json += "\"e\":0,";
    json += "\"c\":" + String(demoClothA) + ",";
    json += "\"r\":0";
    json += "},";
    json += "\"B\":{";
    json += "\"t\":" + String(tB, 1) + ",";
    json += "\"h\":" + String(hB, 1) + ",";
    json += "\"tt\":" + String(ttB, 1) + ",";
    json += "\"th\":" + String(thB, 1) + ",";
    json += "\"p\":" + String(pB) + ",";
    json += "\"ht\":" + String(htB) + ",";
    json += "\"e\":0,";
    json += "\"c\":" + String(demoClothB) + ",";
    json += "\"r\":0";
    json += "}";
    json += "}";
    return json;
}

// ============================================================
// Arduino에 명령 전송 + 응답 대기
// ============================================================
String sendToArduino(String cmd, unsigned long timeout = 2000) {
    // 수신 버퍼 비우기
    while (ardSerial.available()) {
        ardSerial.read();
    }

    // 명령 전송
    ardSerial.println(cmd);
    Serial.print("[->ARD] ");
    Serial.println(cmd);

    // 응답 대기
    unsigned long start = millis();
    String response = "";

    while (millis() - start < timeout) {
        if (ardSerial.available()) {
            response = ardSerial.readStringUntil('\n');
            response.trim();
            if (response.length() > 0) {
                Serial.print("[<-ARD] ");
                Serial.println(response);
                return response;
            }
        }
        yield();  // ESP8266 워치독 리셋 방지
    }

    Serial.println("[<-ARD] TIMEOUT");
    return "ERR:TIMEOUT";
}

// ============================================================
// API: GET /status — 현재 온습도 + 장치 상태 반환
// ============================================================
void handleStatus() {
    // 데모 모드: Arduino 없이 가짜 데이터 반환
    if (DEMO_MODE) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", buildDemoJson());
        return;
    }

    unsigned long now = millis();

    // 캐시가 유효하면 캐시된 값 반환 (Arduino 부담 줄이기)
    if (now - lastStatusTime < STATUS_CACHE_MS && lastStatus != "{}") {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", lastStatus);
        return;
    }

    // Arduino에 상태 요청
    String response = sendToArduino("GET:STATUS");

    if (response.startsWith("STS:")) {
        // "STS:" 접두어 제거하고 JSON만 추출
        lastStatus = response.substring(4);
        lastStatusTime = now;
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", lastStatus);
    } else {
        server.send(500, "application/json", "{\"error\":\"arduino_timeout\"}");
    }
}

// ============================================================
// API: GET /set?section=A&type=2 — 의류 종류 변경
// ============================================================
void handleSet() {
    // 파라미터 읽기
    if (!server.hasArg("section") || !server.hasArg("type")) {
        server.send(400, "application/json", "{\"error\":\"missing_params\"}");
        return;
    }

    String section = server.arg("section");
    String type = server.arg("type");
    int typeIdx = type.toInt();

    // 데모 모드: 로컬 상태만 변경
    if (DEMO_MODE) {
        if (section == "A") demoClothA = typeIdx;
        else if (section == "B") demoClothB = typeIdx;
        Serial.print("[DEMO SET] ");
        Serial.print(section);
        Serial.print(" -> type ");
        Serial.println(typeIdx);
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", "{\"result\":\"ok\"}");
        return;
    }

    // 실제 모드: Arduino에 명령 전송
    String cmd = "SET:" + section + ":" + type;
    String response = sendToArduino(cmd);

    // 캐시 무효화 (상태가 바뀌었으므로)
    lastStatusTime = 0;

    if (response == "OK") {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", "{\"result\":\"ok\"}");
    } else {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"error\":\"" + response + "\"}");
    }
}

// ============================================================
// 웹페이지: SPIFFS에서 index.html 서빙
// ============================================================
void handleRoot() {
    if (SPIFFS.exists("/index.html")) {
        File file = SPIFFS.open("/index.html", "r");
        server.streamFile(file, "text/html");
        file.close();
    } else {
        // SPIFFS에 파일이 없으면 기본 안내 페이지
        String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
        html += "<title>Smart Wardrobe</title></head><body style='font-family:sans-serif;padding:20px'>";
        html += "<h2>IoT Smart Wardrobe</h2>";
        html += "<p>SPIFFS에 index.html을 업로드해 주세요.</p>";
        html += "<p><a href='/status'>/status</a> - 현재 상태 (JSON)</p>";
        html += "<p>/set?section=A&type=0 - 의류 변경</p>";
        if (DEMO_MODE) html += "<p style='color:purple'>데모 모드 활성화됨</p>";
        html += "</body></html>";
        server.send(200, "text/html", html);
    }
}

// ============================================================
// 404 처리 — SPIFFS에서 정적 파일 찾기
// ============================================================
void handleNotFound() {
    String path = server.uri();

    // SPIFFS에서 해당 경로의 파일 찾기
    if (SPIFFS.exists(path)) {
        String contentType = "text/plain";
        if (path.endsWith(".html")) contentType = "text/html";
        else if (path.endsWith(".css")) contentType = "text/css";
        else if (path.endsWith(".js")) contentType = "application/javascript";
        else if (path.endsWith(".json")) contentType = "application/json";
        else if (path.endsWith(".png")) contentType = "image/png";
        else if (path.endsWith(".svg")) contentType = "image/svg+xml";

        File file = SPIFFS.open(path, "r");
        server.streamFile(file, contentType);
        file.close();
        return;
    }

    server.send(404, "text/plain", "404 Not Found");
}

// ============================================================
// Wi-Fi 접속
// ============================================================
void connectWiFi() {
    Serial.print("Wi-Fi 접속 중: ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.print("접속 성공! IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println();
        Serial.println("Wi-Fi 접속 실패. SSID/비밀번호를 확인하세요.");
    }
}

// ============================================================
// setup
// ============================================================
void setup() {
    Serial.begin(115200);    // 디버깅용 시리얼 모니터
    ardSerial.begin(9600);   // Arduino와 통신

    Serial.println();
    Serial.println("=== IoT Smart Wardrobe - NodeMCU ===");

    if (DEMO_MODE) {
        Serial.println("*** 데모 모드 활성화 ***");
        Serial.println("*** Arduino 연결 후 DEMO_MODE를 false로 변경하세요 ***");
    }

    // SPIFFS 초기화
    if (SPIFFS.begin()) {
        Serial.println("SPIFFS 초기화 성공");
    } else {
        Serial.println("SPIFFS 초기화 실패!");
    }

    // Wi-Fi 접속
    connectWiFi();

    // 웹서버 라우팅 설정
    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/set", HTTP_GET, handleSet);
    server.onNotFound(handleNotFound);

    // 서버 시작
    server.begin();
    Serial.println("웹서버 시작됨 (포트 80)");
}

// ============================================================
// loop
// ============================================================
void loop() {
    // 웹서버 클라이언트 요청 처리
    server.handleClient();

    // Wi-Fi 끊김 시 재접속 시도
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi 연결 끊김. 재접속 시도...");
        connectWiFi();
    }
}
