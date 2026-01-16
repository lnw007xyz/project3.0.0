/*
 * =====================================================
 * NODE GATE ESP32S3 - WiFi Master Mode
 * =====================================================
 * 1. เชื่อมต่อ WiFi ให้สำเร็จก่อน
 * 2. อ่านค่า Channel ของ WiFi
 * 3. ตั้งค่า ESP-NOW ให้ตรงกับ WiFi Channel
 * =====================================================
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "thingProperties.h" 

// ==================== CONFIGURATION ====================
static const char* FW_VERSION = "v2.2.0-WiFiMaster";

// เราจะไม่อ้างอิง ESPNOW_CHANNEL คงที่แล้ว แต่จะใช้ตาม WiFi
// static const uint8_t ESPNOW_CHANNEL = 11; 

uint8_t targetTagMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ==================== DATA STRUCTURES ====================

typedef struct __attribute__((packed)) {
  uint8_t  role;
  uint8_t  tag_id;
  uint8_t  reserved;
  uint8_t  a_count;
  uint32_t seq;
  uint32_t msec;
  
  int16_t  A1_raw, A2_raw, A3_raw;
  int16_t  A1_reg1, A2_reg1, A3_reg1;
  int16_t  A1_reg2, A2_reg2, A3_reg2; 
  
  int32_t  tag_lat_e6;  
  int32_t  tag_lng_e6;  
} espnow_pkt_t;

typedef struct __attribute__((packed)) {
  char command[20];
  float target_x;
  float target_y;
  bool emergency_stop;
  uint32_t timestamp;
} UWB_Command_Downlink;

espnow_pkt_t receivedData;
bool newData = false;
int currentChannel = 1; // จะอัปเดทตาม WiFi

// ==================== ESP-NOW CALLBACKS ====================

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  if (data_len == sizeof(espnow_pkt_t)) {
    memcpy(&receivedData, data, sizeof(receivedData));
    memcpy(targetTagMAC, mac_addr, 6);
    
    if (!esp_now_is_peer_exist(targetTagMAC)) {
      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, targetTagMAC, 6);
      peerInfo.channel = currentChannel; // ใช้ Channel เดียวกับ WiFi
      peerInfo.encrypt = false;
      esp_now_add_peer(&peerInfo);
    }
    newData = true;
  }
}

// ==================== COMMAND & CLOUD CALLBACKS ====================

void sendToTag(String cmdStr, float tx, float ty, bool emerg) {
  UWB_Command_Downlink pkg;
  strncpy(pkg.command, cmdStr.c_str(), 19);
  pkg.target_x = tx; 
  pkg.target_y = ty; 
  pkg.emergency_stop = emerg;
  pkg.timestamp = millis();
  
  esp_now_send(targetTagMAC, (uint8_t*)&pkg, sizeof(pkg));
  Serial.printf("[Command] Sent: %s\n", cmdStr.c_str());
}

void onCommandChange() {
  if (command == "START") sendToTag("START", target_x, target_y, false);
  else if (command == "STOP") sendToTag("STOP", 0, 0, false);
  else if (command == "RETURN") sendToTag("RETURN", 0, 0, false);
  else if (command == "GOTO") sendToTag("GOTO", target_x, target_y, false);
}

void onEmergencyStopChange() {
  if (emergency_stop) {
    sendToTag("EMERGENCY_STOP", 0, 0, true);
    command = "EMERGENCY_STOP"; 
  }
}

void onTargetXChange() {}
void onTargetYChange() {}
void onCurrentXChange() {}
void onCurrentYChange() {}
void onDistanceA1Change() {}
void onDistanceA2Change() {}
void onDistanceA3Change() {}

// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("\n=== Node Gate: WiFi First Mode ===");
  Serial.println("1. Connecting to WiFi 'AB'...");

  // 1. เริ่มต้น Arduino Cloud (มันจะจัดการเชื่อม WiFi ให้เอง)
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  // 2. รอจนกว่าจะต่อ WiFi ติด (Timeout 20 วินาที)
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000) {
    Serial.print(".");
    delay(500);
    ArduinoCloud.update();
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    currentChannel = WiFi.channel();
    Serial.printf("\n✅ WiFi Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("👉 WiFi Channel is: %d\n", currentChannel);
    Serial.println("⚠️ IMPORTANT: PLEASE UPDATE YOUR TAG_SENDER CODE TO USE THIS CHANNEL!");
  } else {
    Serial.println("\n❌ WiFi Failed. Defaulting to Channel 1.");
    currentChannel = 1;
  }

  // 3. เริ่มต้น ESP-NOW บน Channel ของ WiFi
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    ESP.restart();
  }
  esp_now_register_recv_cb(OnDataRecv);
  
  // *** สำคัญ: ต้องตั้งค่า Channel ของ ESP-NOW ให้ตรงกับ WiFi ***
  // (ESP32 ใช้ Channel เดียวกันอัตโนมัติเมื่อ WiFi connect แล้ว แต่เราปริ้นบอก user เฉยๆ)
  
  Serial.println("System Ready. Waiting for data...");
}

// ==================== LOOP ====================

void loop() {
  ArduinoCloud.update();
  
  if (newData) {
    distance_a1 = receivedData.A1_reg2;
    distance_a2 = receivedData.A2_reg2;
    distance_a3 = receivedData.A3_reg2;
    
    current_y = receivedData.tag_lat_e6 / 1000000.0; 
    current_x = receivedData.tag_lng_e6 / 1000000.0; 
    
    if (emergency_stop) boat_status = "EMERGENCY";
    else if (current_x != 0) boat_status = "ACTIVE";
    else boat_status = "IDLE";
    
    Serial.printf("Up: A1=%.0f cm, Pos=%.6f, %.6f\n", distance_a1, current_y, current_x);
    newData = false;
  }
}
