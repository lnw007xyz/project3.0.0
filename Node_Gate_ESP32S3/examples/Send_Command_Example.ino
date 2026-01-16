/*
 * =====================================================
 * EXAMPLE: Send Command to Tag (ESP-NOW Downlink)
 * =====================================================
 * ตัวอย่างนี้แสดงวิธีการส่งคำสั่งจาก Node_Gate ไปยัง Tag_sender
 * ผ่าน ESP-NOW (Downlink Communication)
 * 
 * คำสั่งนี้สามารถนำไปใช้ใน Node_Gate_ESP32S3.ino
 * เพื่อให้สามารถส่งคำสั่งจาก Arduino IoT Cloud ลงมาควบคุม Tag
 * =====================================================
 */

#include <esp_now.h>
#include <WiFi.h>

// โครงสร้างคำสั่งที่ส่งไปยัง Tag (ต้องตรงกับ Tag_sender)
typedef struct __attribute__((packed)) {
  char command[20];     // "START", "STOP", "GOTO", "RETURN", "EMERGENCY_STOP"
  float target_x;       // เป้าหมาย X (หรือ Longitude)
  float target_y;       // เป้าหมาย Y (หรือ Latitude)
  bool emergency_stop;
  uint32_t timestamp;
} UWB_Command_Downlink;

// MAC Address ของ Tag ที่ต้องการส่งคำสั่งไป
uint8_t TAG_MAC[6] = { 0x30, 0xED, 0xA0, 0x1F, 0x00, 0x24 }; // เปลี่ยนเป็น MAC ของ Tag จริง

// ==================== HELPER FUNCTIONS ====================

// ส่งคำสั่ง START
void sendStartCommand() {
  UWB_Command_Downlink cmd;
  strcpy(cmd.command, "START");
  cmd.target_x = 0.0f;
  cmd.target_y = 0.0f;
  cmd.emergency_stop = false;
  cmd.timestamp = millis();
  
  esp_err_t result = esp_now_send(TAG_MAC, (uint8_t*)&cmd, sizeof(cmd));
  
  if (result == ESP_OK) {
    Serial.println("[✓] START command sent successfully");
  } else {
    Serial.println("[✗] Error sending START command");
  }
}

// ส่งคำสั่ง STOP
void sendStopCommand() {
  UWB_Command_Downlink cmd;
  strcpy(cmd.command, "STOP");
  cmd.target_x = 0.0f;
  cmd.target_y = 0.0f;
  cmd.emergency_stop = false;
  cmd.timestamp = millis();
  
  esp_err_t result = esp_now_send(TAG_MAC, (uint8_t*)&cmd, sizeof(cmd));
  
  if (result == ESP_OK) {
    Serial.println("[✓] STOP command sent successfully");
  } else {
    Serial.println("[✗] Error sending STOP command");
  }
}

// ส่งคำสั่ง GOTO (ไปยังตำแหน่งเป้าหมาย)
void sendGotoCommand(float lat, float lng) {
  UWB_Command_Downlink cmd;
  strcpy(cmd.command, "GOTO");
  cmd.target_x = lng;  // X = Longitude
  cmd.target_y = lat;  // Y = Latitude
  cmd.emergency_stop = false;
  cmd.timestamp = millis();
  
  esp_err_t result = esp_now_send(TAG_MAC, (uint8_t*)&cmd, sizeof(cmd));
  
  if (result == ESP_OK) {
    Serial.printf("[✓] GOTO command sent: Lat=%.6f, Lng=%.6f\n", lat, lng);
  } else {
    Serial.println("[✗] Error sending GOTO command");
  }
}

// ส่งคำสั่ง RETURN (กลับจุดเริ่มต้น)
void sendReturnCommand() {
  UWB_Command_Downlink cmd;
  strcpy(cmd.command, "RETURN");
  cmd.target_x = 0.0f;
  cmd.target_y = 0.0f;
  cmd.emergency_stop = false;
  cmd.timestamp = millis();
  
  esp_err_t result = esp_now_send(TAG_MAC, (uint8_t*)&cmd, sizeof(cmd));
  
  if (result == ESP_OK) {
    Serial.println("[✓] RETURN command sent successfully");
  } else {
    Serial.println("[✗] Error sending RETURN command");
  }
}

// ส่งคำสั่ง EMERGENCY_STOP (หยุดฉุกเฉิน)
void sendEmergencyStopCommand() {
  UWB_Command_Downlink cmd;
  strcpy(cmd.command, "EMERGENCY_STOP");
  cmd.target_x = 0.0f;
  cmd.target_y = 0.0f;
  cmd.emergency_stop = true;
  cmd.timestamp = millis();
  
  esp_err_t result = esp_now_send(TAG_MAC, (uint8_t*)&cmd, sizeof(cmd));
  
  if (result == ESP_OK) {
    Serial.println("[✓] 🚨 EMERGENCY STOP command sent!");
  } else {
    Serial.println("[✗] Error sending EMERGENCY STOP command");
  }
}

// ==================== INTEGRATION WITH CLOUD CALLBACKS ====================

/*
 * ใน Node_Gate_ESP32S3.ino ใช้ฟังก์ชันเหล่านี้ใน Callbacks:
 */

// Callback เมื่อ command ใน Cloud เปลี่ยน
void onCommandChange() {
  Serial.printf("[Cloud] Command changed to: %s\n", command.c_str());
  
  if (command == "START") {
    sendStartCommand();
  } 
  else if (command == "STOP") {
    sendStopCommand();
  } 
  else if (command == "GOTO") {
    // ใช้ค่า targetLat และ targetLng จาก Cloud
    sendGotoCommand(targetLat, targetLng);
  } 
  else if (command == "RETURN") {
    sendReturnCommand();
  } 
  else if (command == "EMERGENCY_STOP") {
    sendEmergencyStopCommand();
  }
  else {
    Serial.printf("[Warning] Unknown command: %s\n", command.c_str());
  }
}

// Callback เมื่อ emergencyStop ใน Cloud เปลี่ยน
void onEmergencyStopChange() {
  if (emergencyStop == true) {
    Serial.println("[Cloud] Emergency Stop activated!");
    sendEmergencyStopCommand();
  }
}

// ==================== USAGE EXAMPLE ====================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== ESP-NOW Command Sender Example ===");
  
  // Initialize WiFi & ESP-NOW (same as main code)
  WiFi.mode(WIFI_STA);
  esp_now_init();
  
  // Add peer (Tag)
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, TAG_MAC, 6);
  peerInfo.channel = 11;  // ต้องตรงกับ Tag
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
  
  Serial.println("Ready to send commands!");
}

void loop() {
  // ตัวอย่างการส่งคำสั่งต่างๆ
  
  delay(10000);  // รอ 10 วินาที
  sendStartCommand();
  
  delay(5000);   // รอ 5 วินาที
  sendGotoCommand(13.756331, 100.501762);  // ส่งพิกัดเป้าหมาย
  
  delay(10000);  // รอ 10 วินาที
  sendReturnCommand();
  
  delay(5000);   // รอ 5 วินาที
  sendStopCommand();
  
  delay(10000);  // รอ 10 วินาที
}

// ==================== NOTES ====================

/*
 * วิธีการนำไปใช้ใน Node_Gate_ESP32S3.ino:
 * 
 * 1. Copy ฟังก์ชัน sendXXXCommand() ไปวางใน Node_Gate_ESP32S3.ino
 * 
 * 2. เพิ่มโค้ดใน onCommandChange() callback:
 *    void onCommandChange() {
 *      if (command == "START") sendStartCommand();
 *      else if (command == "STOP") sendStopCommand();
 *      // ... etc
 *    }
 * 
 * 3. ลงทะเบียน Callback ใน thingProperties.h:
 *    ArduinoCloud.addProperty(command, READWRITE, ON_CHANGE, onCommandChange);
 * 
 * 4. ใน Dashboard บน Arduino IoT Cloud:
 *    - เพิ่ม Messenger Widget ผูกกับ "command"
 *    - พิมพ์คำสั่ง: START, STOP, GOTO, RETURN, EMERGENCY_STOP
 * 
 * 5. Tag_sender จะได้รับคำสั่งและดำเนินการตาม OnDataRecv() callback
 */
