/*
 * NODE ESP32S3 - Bidirectional IoT Gateway v2.0.0
 * 
 * โครงการ: ระบบเรืออัตโนมัติด้วย UWB Indoor Positioning
 * หน้าที่:
 * - รับข้อมูลตำแหน่ง UWB จาก Tag ผ่าน ESP-NOW
 * - อัปโหลดข้อมูลขึ้น Arduino Cloud (Real-time Monitoring)
 * - รับคำสั่งจาก Cloud Dashboard
 * - ส่งคำสั่งไปยัง Tag ผ่าน ESP-NOW
 * 
 * Hardware: MaUWB ESP32S3 Module
 * Core: ESP32 Arduino Core v3.x (IDF 5.x)
 * 
 * Created: 2026-01-12
 */

#include "thingProperties.h"  // ไฟล์ที่ Arduino Cloud สร้างให้
#include <esp_now.h>
#include <WiFi.h>

// ===== โครงสร้างข้อมูล ESP-NOW =====

// ข้อมูลจาก Tag → NODE (Uplink)
typedef struct {
  float distance_a1;
  float distance_a2;
  float distance_a3;
  float pos_x;
  float pos_y;
  char status[20];      // "IDLE", "RUNNING", "ARRIVED"
  uint32_t timestamp;
} UWB_Data_Uplink;

// คำสั่งจาก NODE → Tag (Downlink)
typedef struct {
  char command[20];     // "START", "STOP", "GOTO", "RETURN", "EMERGENCY_STOP"
  float target_x;
  float target_y;
  bool emergency_stop;
  uint32_t timestamp;
} UWB_Command_Downlink;

// ตัวแปรสำหรับเก็บข้อมูล
UWB_Data_Uplink receivedData;
UWB_Command_Downlink commandToSend;
bool newDataAvailable = false;
bool commandPending = false;

// MAC Address ของ Tag (⚠️ ใส่ MAC จริงของ Tag ของคุณ!)
// หา MAC ได้จากโค้ด Tag โดยใช้ WiFi.macAddress()
uint8_t tagMacAddress[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

// ===== ฟังก์ชัน Callback จาก Cloud (เรียกเมื่อ Dashboard เปลี่ยนค่า) =====

void onCommandChange() {
  Serial.printf("[☁️ Cloud] คำสั่งใหม่: %s\n", command.c_str());
  strncpy(commandToSend.command, command.c_str(), sizeof(commandToSend.command) - 1);
  commandToSend.command[sizeof(commandToSend.command) - 1] = '\0';
  commandPending = true;
}

void onTargetXChange() {
  Serial.printf("[☁️ Cloud] เป้าหมาย X = %.1f\n", target_x);
  commandToSend.target_x = target_x;
  commandPending = true;
}

void onTargetYChange() {
  Serial.printf("[☁️ Cloud] เป้าหมาย Y = %.1f\n", target_y);
  commandToSend.target_y = target_y;
  commandPending = true;
}

void onEmergencyStopChange() {
  if (emergency_stop) {
    Serial.println("[☁️ Cloud] ⚠️ หยุดฉุกเฉิน!");
    strcpy(commandToSend.command, "EMERGENCY_STOP");
    commandToSend.emergency_stop = true;
    commandPending = true;
    sendCommandToTag();  // ส่งทันที!
  }
}

// ===== ESP-NOW Callbacks =====

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  if (data_len == sizeof(UWB_Data_Uplink)) {
    memcpy(&receivedData, data, sizeof(receivedData));
    newDataAvailable = true;
    
    Serial.printf("[↓ ESP-NOW] A1=%.1f A2=%.1f A3=%.1f X=%.1f Y=%.1f Status=%s\n",
                  receivedData.distance_a1, receivedData.distance_a2, 
                  receivedData.distance_a3, receivedData.pos_x, 
                  receivedData.pos_y, receivedData.status);
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("[↑ ESP-NOW] ส่งคำสั่ง: %s\n", 
                status == ESP_NOW_SEND_SUCCESS ? "สำเร็จ ✓" : "ล้มเหลว ✗");
}

// ===== ฟังก์ชันส่งคำสั่งไป Tag =====
void sendCommandToTag() {
  if (!commandPending) return;
  
  commandToSend.timestamp = millis();
  
  esp_err_t result = esp_now_send(tagMacAddress, 
                                   (uint8_t *)&commandToSend, 
                                   sizeof(commandToSend));
  
  if (result == ESP_OK) {
    Serial.printf("[↑ ESP-NOW] ส่งคำสั่ง '%s' (X=%.1f Y=%.1f)\n",
                  commandToSend.command, commandToSend.target_x, commandToSend.target_y);
    commandPending = false;
  } else {
    Serial.printf("[✗ ESP-NOW] ส่งล้มเหลว! Error: %d\n", result);
  }
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n╔════════════════════════════════════════════╗");
  Serial.println("║  NODE Gateway v2.0.0 for Arduino Cloud   ║");
  Serial.println("║  UWB Boat Navigation System               ║");
  Serial.println("╚════════════════════════════════════════════╝\n");
  
  // 1. ตั้งค่า WiFi (AP+STA Mode รองรับทั้ง Cloud และ ESP-NOW)
  WiFi.mode(WIFI_AP_STA);
  Serial.println("[1/6] ตั้งค่า WiFi เป็น AP+STA Mode");
  
  // 2. เริ่มต้น ESP-NOW
  Serial.print("[2/6] กำลังเริ่มต้น ESP-NOW... ");
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ล้มเหลว! รีสตาร์ท...");
    ESP.restart();
  }
  Serial.println("✓ สำเร็จ");
  
  // 3. ลงทะเบียน Tag เป็น Peer
  Serial.print("[3/6] เพิ่ม Tag Peer... ");
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, tagMacAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("❌ ล้มเหลว!");
  } else {
    Serial.println("✓ สำเร็จ");
  }
  
  // 4. ลงทะเบียน Callbacks
  Serial.print("[4/6] ลงทะเบียน ESP-NOW Callbacks... ");
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);
  Serial.println("✓ สำเร็จ");
  
  // 5. เชื่อมต่อ Arduino Cloud
  Serial.println("[5/6] กำลังเชื่อม Arduino Cloud...");
  initProperties();  // ฟังก์ชันจาก thingProperties.h
  
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);  // 0=ไม่มี Debug, 2=แสดงทุกอย่าง
  ArduinoCloud.printDebugInfo();
  
  // 6. แสดงข้อมูลระบบ
  Serial.println("[6/6] ข้อมูลระบบ:");
  Serial.printf("     MAC Address: %s\n", WiFi.macAddress().c_str());
  Serial.printf("     Thing Name: UWB_Boat_System\n");
  Serial.printf("     Device: NODE_Gateway\n\n");
  
  Serial.println("✓ ระบบพร้อมทำงาน! กำลังรอข้อมูลจาก Tag...\n");
}

// ===== Main Loop =====
void loop() {
  // อัปเดต Cloud Connection (จำเป็น!)
  ArduinoCloud.update();
  
  // ทิศทาง 1: Tag → NODE → Cloud (Monitoring)
  if (newDataAvailable) {
    // อัปโหลดข้อมูลขึ้น Cloud
    distance_a1 = receivedData.distance_a1;
    distance_a2 = receivedData.distance_a2;
    distance_a3 = receivedData.distance_a3;
    current_x = receivedData.pos_x;
    current_y = receivedData.pos_y;
    boat_status = String(receivedData.status);
    
    Serial.println("☁️ อัปโหลดข้อมูลขึ้น Cloud สำเร็จ");
    newDataAvailable = false;
  }
  
  // ทิศทาง 2: Cloud → NODE → Tag (Control)
  if (commandPending) {
    sendCommandToTag();
  }
  
  delay(10);  // ลด CPU load
}

/*
 * ⚠️ สำคัญ: ต้องแก้ไข MAC Address ของ Tag!
 * 
 * วิธีหา MAC Address ของ Tag:
 * 1. อัปโหลดโค้ดนี้ไปยัง Tag:
 *    void setup() {
 *      Serial.begin(115200);
 *      WiFi.mode(WIFI_STA);
 *      Serial.println(WiFi.macAddress());
 *    }
 *    void loop() {}
 * 
 * 2. เปิด Serial Monitor ดู MAC Address (เช่น AA:BB:CC:DD:EE:FF)
 * 3. นำมาใส่ที่บรรทัดที่ 38 ของโค้ดนี้
 */
