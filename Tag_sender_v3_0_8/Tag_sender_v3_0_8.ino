/*
 * =====================================================
 * TAG SENDER v3.0.8 - ปรับปรุงสมการใหม่
 * =====================================================
 * BASE: v3.0.7 (GPS Trilateration + Bidirectional)
 * NEW FEATURES:
 * - อัพเดทสมการ Regression ใหม่ จากไฟล์ regression_compare_old_vs_new_v3_0_7.xlsx
 * - ใช้สมการแบบ 'new' ที่มีความแม่นยำสูงขึ้น (RMSE และ MAE ต่ำกว่า)
 * - คงฟีเจอร์อื่นๆ เดิมไว้ทั้งหมด (GPS Trilateration, Bidirectional)
 * - แก้ไข ESP-NOW callback signatures สำหรับ IDF 5.5+ compatibility
 * 
 * Platform: ESP32 Arduino Core v3.x (IDF 5.x)
 * Date: 2026-01-16
 * =====================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <math.h>

// ==================== CONFIGURATION ====================
#define TAG_ID           7
#define I2C_SDA          39
#define I2C_SCL          38
#define IO_RXD2          18
#define IO_TXD2          17
#define UWB_RST          16

static const char* FW_VERSION = "v3.0.8-NewEq";

// ⚠️ ใส่ MAC Address ของ NODE Gateway ที่นี่ (ตัวอย่าง: AA:BB:CC:DD:EE:FF)
uint8_t NODE_PEER_MAC[6] = { 0x30, 0xED, 0xA0, 0x1F, 0x00, 0x24 }; 

static const uint8_t ESPNOW_CHANNEL = 11;

// ==================== NEW REGRESSION COEFFICIENTS (v3.0.8) ====================
// Based on regression_compare_old_vs_new_v3_0_7.xlsx - 'new' equations
// สมการใหม่มีความแม่นยำสูงกว่า (RMSE และ MAE ต่ำกว่า)

const float LINEAR_A_NEW[6][3] = {
  {-8.444002, -19.539761, -9.925643},
  {-17.328058, -9.381345, -14.598817},
  {-41.465105, -22.955749, -40.430076},
  {-9.863671, -18.451526, 239.121862},
  {-222.590712, -1050.403936, -756.825359},
  {3000.000000, 3000.000000, 3000.000000},
};

const float LINEAR_B_NEW[6][3] = {
  {0.596695226438, 0.810139165010, 0.596864951768},
  {0.698216027643, 0.596750954644, 0.664883029405},
  {0.959160504358, 0.865906859305, 0.953018190500},
  {0.928290529636, 0.847455072475, 0.567954261162},
  {1.076949637108, 1.445004233258, 1.288055580525},
  {0.000000000000, 0.000000000000, 0.000000000000},
};

const float POLY_P0_NEW[6][3] = {
  {-35.528340, -10.404021, 20.320618},
  {3.097516, -8.117261, -15.613270},
  {-25.341996, -78.984475, 27.474012},
  {-1713.639273, 1594.617826, 1110.883535},
  {623.121797, -1528.162938, -649.214437},
  {3000.000000, 3000.000000, 3000.000000},
};

const float POLY_P1_NEW[6][3] = {
  {2.155286571925, 0.371610552507, -0.956862212445},
  {0.120940136949, 0.560813653495, 0.692768034790},
  {0.793531606424, 1.460499206127, 0.211756803372},
  {6.252989621387, -3.821157545724, -2.075813099053},
  {0.070081991541, 1.927636472982, 1.177509699218},
  {0.000000000000, 0.000000000000, 0.000000000000},
};

const float POLY_P2_NEW[6][3] = {
  {-0.022126982649569301, 0.005201227377292300, 0.019517983128906999},
  {0.003929703573457400, 0.000246289901249000, -0.000185185166636600},
  {0.000354349435176600, -0.001227830807574900, 0.001650409095546900},
  {-0.004048981891561800, 0.003318121619088500, 0.001939293782087000},
  {0.000255336760929700, -0.000113309369010500, 0.000025890862117756},
  {0.000000000000000000, 0.000000000000000000, 0.000000000000000000},
};

// ==================== HELPER FUNCTIONS ====================
int getDistanceRange(float raw_cm) {
  if (raw_cm < 50.0f) return 0;
  else if (raw_cm < 100.0f) return 1;
  else if (raw_cm < 500.0f) return 2;
  else if (raw_cm < 1000.0f) return 3;
  else if (raw_cm < 3000.0f) return 4;
  else return 5;
}

const char* getRangeLabel(int range) {
  switch (range) {
    case 0: return "10-50";
    case 1: return "50-100";
    case 2: return "100-500";
    case 3: return "500-1k";
    case 4: return "1k-3k";
    case 5: return "3k+";
    default: return "UNK";
  }
}

float getRegressionValue_Type1(float raw_cm, int anchor_idx) {
  int range = getDistanceRange(raw_cm);
  return LINEAR_A_NEW[range][anchor_idx] + LINEAR_B_NEW[range][anchor_idx] * raw_cm;
}

float getRegressionValue_Type2(float raw_cm, int anchor_idx) {
  int range = getDistanceRange(raw_cm);
  float p0 = POLY_P0_NEW[range][anchor_idx];
  float p1 = POLY_P1_NEW[range][anchor_idx];
  float p2 = POLY_P2_NEW[range][anchor_idx];
  return p0 + p1 * raw_cm + p2 * raw_cm * raw_cm;
}

// ==================== DATA STRUCTURES ====================

// โครงสร้างข้อมูลส่งไป Node (Uplink)
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
  
  int32_t  tag_lat_e6;  // Lat * 1e6
  int32_t  tag_lng_e6;  // Lng * 1e6
} espnow_pkt_t;

// ⭐ โครงสร้างรับคำสั่งจาก Node (Downlink)
typedef struct __attribute__((packed)) {
  char command[20];     // "START", "STOP", "GOTO", "RETURN", "EMERGENCY_STOP"
  float target_x;       // เป้าหมาย X
  float target_y;       // เป้าหมาย Y
  bool emergency_stop;
  uint32_t timestamp;
} UWB_Command_Downlink;

// โครงสร้างรับข้อมูลจาก Anchor (GPS)
typedef struct __attribute__((packed)) {
  int id;           // Anchor ID (1, 2, 3)
  float lat;        // Latitude
  float lng;        // Longitude
  bool valid;       // GPS Fix Status
} anchor_gps_t;

// ==================== GLOBAL VARIABLES ====================
Adafruit_SSD1306 display(128, 64, &Wire, -1);
HardwareSerial SERIAL_AT(2);

String  rxLine;
int     range_list[8] = {0};
float   rssi_list[8]  = {0};
uint32_t last_seq     = 0;

bool     espnow_ready     = false;
uint32_t tx_seq           = 0;

float prev_A1_cm = 0.0f, prev_A2_cm = 0.0f, prev_A3_cm = 0.0f;
float raw_A1_cm = 0.0f, raw_A2_cm = 0.0f, raw_A3_cm = 0.0f;
float reg1_A1_cm = 0.0f, reg1_A2_cm = 0.0f, reg1_A3_cm = 0.0f;
float reg2_A1_cm = 0.0f, reg2_A2_cm = 0.0f, reg2_A3_cm = 0.0f;

uint32_t last_uwb_packet_time = 0;

// ตัวแปร GPS & Position
anchor_gps_t anchor_positions[3]; 
bool anchor_gps_ready[3] = {false, false, false};
float tag_latitude = 0.0;
float tag_longitude = 0.0;
bool tag_position_valid = false;

// ตัวแปรคำสั่ง
UWB_Command_Downlink receivedCommand;

// ==================== TRILATERATION LOGIC (v3.0.6) ====================

// แปลง Degree เป็น Radian
float toRad(float degree) { return degree * PI / 180.0; }
// แปลง Radian เป็น Degree
float toDeg(float rad) { return rad * 180.0 / PI; }

// คำนวณระยะทางระหว่างพิกัด GPS 2 จุด (Haversine) - return เป็นเมตร
float calculateGPSDistance(float lat1, float lng1, float lat2, float lng2) {
  float R = 6371000.0; // รัศมีโลก (เมตร)
  float dLat = toRad(lat2 - lat1);
  float dLon = toRad(lng2 - lng1);
  float a = sin(dLat / 2) * sin(dLat / 2) +
            cos(toRad(lat1)) * cos(toRad(lat2)) *
            sin(dLon / 2) * sin(dLon / 2);
  float c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c;
}

// ฟังก์ชันคำนวณตำแหน่ง Tag จากระยะทาง 3 วงกลม (Trilateration แบบง่ายบนระนาบ)
void updateTrilateration() {
  // ตรวจสอบว่ามีข้อมูลครบหรือไม่
  if (!anchor_gps_ready[0] || !anchor_gps_ready[1] || !anchor_gps_ready[2]) return;
  if (raw_A1_cm <= 0 || raw_A2_cm <= 0 || raw_A3_cm <= 0) return;

  // ใช้ระยะทางที่ผ่าน Regression Type 2 (แม่นยำที่สุด) แปลงเป็นเมตร
  float d1 = reg2_A1_cm / 100.0;
  float d2 = reg2_A2_cm / 100.0;
  float d3 = reg2_A3_cm / 100.0;

  // แปลงพิกัด GPS เป็นพิกัด X,Y ในหน่วยเมตร (โดยให้ A1 เป็นจุดอ้างอิง 0,0)
  // หมายเหตุ: วิธีนี้เป็นการประมาณบนระนาบเล็กๆ (Local Tangent Plane)
  float lat0 = anchor_positions[0].lat;
  float lng0 = anchor_positions[0].lng;

  // พิกัด A1 (0,0)
  float x1 = 0; 
  float y1 = 0;

  // พิกัด A2 เทียบ A1
  float x2 = calculateGPSDistance(lat0, lng0, lat0, anchor_positions[1].lng);
  if (anchor_positions[1].lng < lng0) x2 = -x2;
  float y2 = calculateGPSDistance(lat0, lng0, anchor_positions[1].lat, lng0);
  if (anchor_positions[1].lat < lat0) y2 = -y2;

  // พิกัด A3 เทียบ A1
  float x3 = calculateGPSDistance(lat0, lng0, lat0, anchor_positions[2].lng);
  if (anchor_positions[2].lng < lng0) x3 = -x3;
  float y3 = calculateGPSDistance(lat0, lng0, anchor_positions[2].lat, lng0);
  if (anchor_positions[2].lat < lat0) y3 = -y3;

  // สูตร Trilateration 
  // A = 2*x2, B = 2*y2
  // C = 2*x3, D = 2*y3
  // E = d1^2 - d2^2 - x1^2 + x2^2 - y1^2 + y2^2
  // F = d1^2 - d3^2 - x1^2 + x3^2 - y1^2 + y3^2
  
  float A = 2 * x2;
  float B = 2 * y2;
  float C = 2 * x3;
  float D = 2 * y3;
  float E = (d1*d1) - (d2*d2) + (x2*x2) + (y2*y2); 
  float F = (d1*d1) - (d3*d3) + (x3*x3) + (y3*y3);

  // แก้สมการหา x, y ของ Tag
  float tag_x = (E*D - B*F) / (A*D - B*C);
  float tag_y = (A*F - E*C) / (A*D - B*C);

  // แปลงกลับเป็น GPS (lat, lng) จาก A1 (lat0, lng0)
  // 1 degree lat approx 111320 meters
  // 1 degree lng approx 111320 * cos(lat) meters
  float meters_per_deg_lat = 111320.0;
  float meters_per_deg_lng = 111320.0 * cos(toRad(lat0));

  tag_latitude  = lat0 + (tag_y / meters_per_deg_lat);
  tag_longitude = lng0 + (tag_x / meters_per_deg_lng);
  tag_position_valid = true;
}

// ==================== OLED FUNCTIONS ====================
void drawScreen(float rawA1, float rawA2, float rawA3,
                float r1A1, float r1A2, float r1A3,
                float r2A1, float r2A2, float r2A3) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Line 1: ID & Status
  display.setCursor(0, 0);
  display.printf("TAG-%d %s [v3.0.8]", TAG_ID, getRangeLabel(getDistanceRange(rawA1)));
  if (tag_position_valid) display.print(" GPS");

  // Line 2: A1
  display.setCursor(0, 10);
  display.printf("A1 R%.0f T2:%.0f", rawA1, r2A1);

  // Line 3: A2
  display.setCursor(0, 20);
  display.printf("A2 R%.0f T2:%.0f", rawA2, r2A2);

  // Line 4: A3
  display.setCursor(0, 30);
  display.printf("A3 R%.0f T2:%.0f", rawA3, r2A3);

  // Line 5: Lat/Lng (ถ้ามี)
  display.setCursor(0, 42);
  if (tag_position_valid) {
    display.printf("%.5f, %.5f", tag_latitude, tag_longitude);
  } else {
    display.print("Waiting for GPS...");
  }
  
  // Line 6: Last Command
  display.setCursor(0, 52);
  display.printf("CMD: %s", receivedCommand.command[0] ? receivedCommand.command : "NONE");

  display.display();
}

// ==================== ESP-NOW CALLBACKS ====================

// Callback เมื่อได้รับข้อมูล (รองรับทั้ง Anchor GPS และ Node Command)
// Updated for ESP32 Arduino Core v3.x (IDF 5.5+)
void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *data, int data_len) {
  // กรณี 1: รับ GPS จาก Anchor (ขนาดข้อมูล = sizeof(anchor_gps_t))
  if (data_len == sizeof(anchor_gps_t)) {
    anchor_gps_t recv_anchor;
    memcpy(&recv_anchor, data, sizeof(recv_anchor));
    
    if (recv_anchor.id >= 1 && recv_anchor.id <= 3) {
      anchor_positions[recv_anchor.id - 1] = recv_anchor;
      anchor_gps_ready[recv_anchor.id - 1] = true;
      // Serial.printf("[ESP-NOW] Updated Anchor %d GPS\n", recv_anchor.id);
    }
  }
  // กรณี 2: รับคำสั่งจาก Node (ขนาดข้อมูล = sizeof(UWB_Command_Downlink))
  else if (data_len == sizeof(UWB_Command_Downlink)) {
    memcpy(&receivedCommand, data, sizeof(receivedCommand));
    Serial.printf("[CMD] %s X:%.1f Y:%.1f STOP:%d\n", 
                  receivedCommand.command, receivedCommand.target_x, 
                  receivedCommand.target_y, receivedCommand.emergency_stop);
    
    // ดำเนินการคำสั่ง (Execute)
    if (receivedCommand.emergency_stop) {
      Serial.println("!!! EMERGENCY STOP !!!");
      // TODO: หยุดมอเตอร์ทันที
    }
  }
}

// Updated for ESP32 Arduino Core v3.x (IDF 5.5+)
void onEspNowSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  // Serial.print(status == ESP_NOW_SEND_SUCCESS ? "." : "x"); // ลดการปริ้นท์
}

// ==================== UWB PARSING & SENDING ====================

void sendNow_AllData() {
  if (!espnow_ready) return;

  espnow_pkt_t p{};
  p.role    = 0;
  p.tag_id  = TAG_ID;
  p.a_count = 3;
  p.seq     = ++tx_seq;
  p.msec    = millis();

  p.A1_raw = (int16_t)raw_A1_cm;
  p.A2_raw = (int16_t)raw_A2_cm;
  p.A3_raw = (int16_t)raw_A3_cm;

  p.A1_reg1 = (int16_t)reg1_A1_cm;
  p.A2_reg1 = (int16_t)reg1_A2_cm;
  p.A3_reg1 = (int16_t)reg1_A3_cm;

  p.A1_reg2 = (int16_t)reg2_A1_cm;
  p.A2_reg2 = (int16_t)reg2_A2_cm;
  p.A3_reg2 = (int16_t)reg2_A3_cm;

  // ส่งพิกัดที่คำนวณได้ไปด้วย (คูณ 1 ล้านเพื่อส่งเป็น int)
  p.tag_lat_e6 = (int32_t)(tag_latitude * 1000000);
  p.tag_lng_e6 = (int32_t)(tag_longitude * 1000000);

  esp_err_t err = esp_now_send(NODE_PEER_MAC, (uint8_t*)&p, sizeof(p));
}

String sendAT(const String &cmd, uint32_t timeout_ms = 800, bool echo = false) {
  String resp;
  if (echo) Serial.println(cmd);
  SERIAL_AT.println(cmd);
  uint32_t t0 = millis();
  while (millis() - t0 < timeout_ms) {
    while (SERIAL_AT.available()) resp += (char)SERIAL_AT.read();
  }
  return resp;
}

void parseRange(const String &line) {
  // (Logic เดิม)
  int s1 = line.indexOf("seq:");
  if (s1 >= 0) {
    int comma = line.indexOf(",", s1);
    last_seq = (uint32_t)line.substring(s1 + 4, (comma < 0) ? line.length() : comma).toInt();
  }

  for (int i = 0; i < 8; i++) {
    range_list[i] = 0;
  }

  int idx1 = line.indexOf("range:(");
  int idx2 = (idx1 >= 0) ? line.indexOf(")", idx1) : -1;
  if (idx1 >= 0 && idx2 > idx1) {
    String sr = line.substring(idx1 + 7, idx2);
    int last = 0;
    for (int i = 0; i < 8 && last < (int)sr.length(); i++) {
      int c = sr.indexOf(',', last);
      range_list[i] = sr.substring(last, (c < 0) ? sr.length() : c).toInt();
      last = (c < 0) ? sr.length() : c + 1;
    }
  }

  raw_A1_cm = (float)range_list[0];
  raw_A2_cm = (float)range_list[1];
  raw_A3_cm = (float)range_list[2];

  // Validation
  if (raw_A1_cm < 5.0f) raw_A1_cm = 5.0f;
  if (raw_A2_cm < 5.0f) raw_A2_cm = 5.0f;
  if (raw_A3_cm < 5.0f) raw_A3_cm = 5.0f;
  
  if (raw_A1_cm > 10000.0f) raw_A1_cm = prev_A1_cm;
  if (raw_A2_cm > 10000.0f) raw_A2_cm = prev_A2_cm;
  if (raw_A3_cm > 10000.0f) raw_A3_cm = prev_A3_cm;

  prev_A1_cm = raw_A1_cm;
  prev_A2_cm = raw_A2_cm;
  prev_A3_cm = raw_A3_cm;
  
  last_uwb_packet_time = millis();

  // Regression Calculation (ใช้สมการใหม่)
  reg1_A1_cm = getRegressionValue_Type1(raw_A1_cm, 0);
  reg1_A2_cm = getRegressionValue_Type1(raw_A2_cm, 1);
  reg1_A3_cm = getRegressionValue_Type1(raw_A3_cm, 2);

  reg2_A1_cm = getRegressionValue_Type2(raw_A1_cm, 0);
  reg2_A2_cm = getRegressionValue_Type2(raw_A2_cm, 1);
  reg2_A3_cm = getRegressionValue_Type2(raw_A3_cm, 2);

  // คำนวณตำแหน่ง (Trilateration)
  updateTrilateration();

  // Debug Output
  Serial.printf(
    "[DATA] R:%.0f,%.0f,%.0f | Pos: %.6f, %.6f (%s)\n",
    raw_A1_cm, raw_A2_cm, raw_A3_cm, 
    tag_latitude, tag_longitude, 
    tag_position_valid ? "VALID" : "NO FIX"
  );

  // Update Display
  drawScreen(raw_A1_cm, raw_A2_cm, raw_A3_cm,
             reg1_A1_cm, reg1_A2_cm, reg1_A3_cm,
             reg2_A1_cm, reg2_A2_cm, reg2_A3_cm);
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== TAG SENDER v3.0.8 (ปรับปรุงสมการใหม่) ===");

  // 1. ⚠️ แก้ปัญหา WiFi Connecting Error
  WiFi.persistent(false);   // ปิดการบันทึกค่า WiFi ลง Flash
  WiFi.disconnect(true);    // ตัดการเชื่อมต่อเก่าที่ค้างอยู่
  WiFi.mode(WIFI_STA);      // ตั้งโหมดเป็น Station (แต่ไม่ Connect AP)
  
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  
  // แสดง MAC Address (เพื่อเช็คว่าถูกต้องหรือไม่)
  Serial.print("[INFO] My MAC: ");
  Serial.println(WiFi.macAddress());

  // 2. Hardware Init
  pinMode(UWB_RST, OUTPUT);
  digitalWrite(UWB_RST, HIGH);

  SERIAL_AT.begin(115200, SERIAL_8N1, IO_RXD2, IO_TXD2);
  
  Wire.begin(I2C_SDA, I2C_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
  }
  display.clearDisplay();
  display.display();

  // 3. UWB Config
  sendAT("AT?");
  sendAT("AT+RESTORE", 2000);
  char cfg[40];
  sprintf(cfg, "AT+SETCFG=%d,0,1,1", TAG_ID); // Role 0 = Tag
  sendAT(cfg, 1000);
  sendAT("AT+SETCAP=4,10,1", 1000); // 4 Anchors max
  sendAT("AT+SETRPT=1", 800);
  sendAT("AT+SAVE", 500);
  sendAT("AT+RESTART", 2000);

  // 4. ESP-NOW Init
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  esp_now_register_send_cb(onEspNowSent);
  esp_now_register_recv_cb(OnDataRecv); // ⭐ ลงทะเบียนรับข้อมูล (GPS + CMD)

  // 5. Add Peer (Node)
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, NODE_PEER_MAC, 6);
  peerInfo.channel = ESPNOW_CHANNEL;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
  } else {
    Serial.println("Node Peer Added");
    espnow_ready = true;
  }
  
  Serial.println("✓ Initialization Complete - Using NEW Regression Equations");
}

// ==================== MAIN LOOP ====================
void loop() {
  // Smart Polling Logic
  static uint32_t t_last_poll = 0;
  if (millis() - last_uwb_packet_time > 500) {
    if (millis() - t_last_poll > 500) {
      sendAT("AT+RDATA", 0);
      t_last_poll = millis();
    }
  }

  // อ่านข้อมูลจาก UWB
  while (SERIAL_AT.available()) {
    char c = SERIAL_AT.read();
    if (c == '\n') {
      if (rxLine.startsWith("AT+RANGE")) {
        parseRange(rxLine);
      }
      rxLine = "";
    } else if (c != '\r') {
      if (rxLine.length() < 200) rxLine += c;
      else rxLine = "";
    }
  }

  // ส่งข้อมูลไป Node ทุก 200ms
  static uint32_t t_espnow = 0;
  if (millis() - t_espnow > 200) {
    sendNow_AllData();
    t_espnow = millis();
  }

  // UWB Watchdog
  if (millis() - last_uwb_packet_time > 3000 && last_uwb_packet_time > 0) {
    // Reset UWB if silent
    digitalWrite(UWB_RST, LOW);
    delay(50);
    digitalWrite(UWB_RST, HIGH);
    last_uwb_packet_time = millis();
  }
}
