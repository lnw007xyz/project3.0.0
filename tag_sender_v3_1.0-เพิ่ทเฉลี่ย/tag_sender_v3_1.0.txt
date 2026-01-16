/*
 * =====================================================
 * TAG SENDER v3.1.0 - REGRESSION WITH SOFT HANDOFF
 * =====================================================
 * พัฒนาต่อจาก: v3.0.0-ALL1
 * 
 * ✅ ไม่มี UKF (ใช้ raw distance หลังแก้ bias)
 * ✅ คำนวณ Regression ทั้ง 3 แบบพร้อมกัน (Linear, Poly, ErrorLin)
 * ✅ NEW: 5 ช่วงระยะ (0-100, 100-300, 300-700, 700-1500, 1500-3000 cm)
 * ✅ NEW: Soft Handoff - เกลี่ยค่ารอยต่อลดการกระโดด (±20cm transition)
 * ✅ NEW: Coefficients คำนวณจากข้อมูลจริง 100 ตัวอย่าง (2 การทดลอง)
 * ✅ แยก Bias Correction และ Regression ชัดเจน
 * ✅ แสดงผล 3 แบบบน OLED + ส่ง ESP-NOW
 * 
 * ผู้พัฒนา: โครงการ II (Enhanced by Perplexity AI)
 * วันที่: 2025-12-23
 * Model Training: 100 samples, MAE < 10cm (Type2)
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

// ==================== Configuration ====================
#define TAG_ID           0
#define I2C_SDA          39
#define I2C_SCL          38
#define IO_RXD2          18
#define IO_TXD2          17
#define UWB_RST          16

static const char* FW_VERSION = "v3.1.0-Final";
uint8_t NODE_PEER_MAC[6] = { 0x30, 0xED, 0xA0, 0x1F, 0x00, 0x24 };
static const uint8_t ESPNOW_CHANNEL = 11;

// ==================== Soft Handoff Configuration ====================
#define TRANSITION_ZONE_WIDTH 40.0f  // ระยะเกลี่ย ±20cm แต่ละด้าน

// ==================== Regression Coefficients (REAL DATA) ====================
// คำนวณจากข้อมูล 100 ตัวอย่าง, 2 การทดลอง
// ช่วงระยะ: 0=0-100cm, 1=100-300cm, 2=300-700cm, 3=700-1500cm, 4=1500-3000cm

// Type 1: Linear (y = a + b*x) - MAE: 10-80cm
const float LINEAR_A[5][3] = {
  {4.812, -2.487, -2.420},         // Range 0
  {-5.249, -9.000, -9.309},        // Range 1
  {-45.824, -166.892, -18.355},    // Range 2
  {-126.070, 436.551, 452.662},    // Range 3
  {-467.792, -508.685, -478.580},  // Range 4
};

const float LINEAR_B[5][3] = {
  {0.492398, 0.554863, 0.605513},   // Range 0
  {0.787069, 0.878049, 0.796547},   // Range 1
  {0.975901, 1.188660, 0.888069},   // Range 2
  {1.085421, 0.425537, 0.429114},   // Range 3
  {1.174605, 1.223053, 1.175479},   // Range 4
};

// Type 2: Polynomial (y = p0 + p1*x + p2*x^2) - MAE: 3-73cm ⭐ BEST
const float POLY_P0[5][3] = {
  {19.356, 16.166, 14.071},
  {148.092, 177.734, -310.062},
  {-727.356, 850.151, -16.936},
  {-3593.864, -2289.829, 94.038},
  {6194.083, -3831.287, 7017.731},
};

const float POLY_P1[5][3] = {
  {-0.045718, -0.083892, -0.037109},
  {-0.967808, -1.449064, 4.053875},
  {4.111236, -3.226126, 0.881686},
  {8.844719, 5.714326, 1.227544},
  {-4.283093, 3.991656, -4.961547},
};

const float POLY_P2[5][3] = {
  {0.004194306, 0.004834203, 0.005341267},
  {0.004505005, 0.006541998, -0.008017152},
  {-0.003426222, 0.004643859, 0.000006775},
  {-0.004248459, -0.002388052, -0.000404346},
  {0.001086848, -0.000561288, 0.001221835},
};

// Type 3: Error Linear (error = c + d*raw) - MAE: 10-80cm
const float ERROR_C[5][3] = {
  {4.812, -2.487, -2.420},
  {-5.249, -9.000, -9.309},
  {-45.824, -166.892, -18.355},
  {-126.070, 436.551, 452.662},
  {-467.792, -508.685, -478.580},
};

const float ERROR_D[5][3] = {
  {-0.507602, -0.445137, -0.394487},
  {-0.212931, -0.121951, -0.203453},
  {-0.024099, 0.188660, -0.111931},
  {0.085421, -0.574463, -0.570886},
  {0.174605, 0.223053, 0.175479},
};

// ==================== Helper Functions ====================

// ฟังก์ชันกำหนดช่วงระยะ (5 ranges)
int getDistanceRange(float raw_cm) {
  if (raw_cm < 100.0f) return 0;
  else if (raw_cm < 300.0f) return 1;
  else if (raw_cm < 700.0f) return 2;
  else if (raw_cm < 1500.0f) return 3;
  else return 4;
}

// ฟังก์ชันหาขอบบนของช่วง
float getRangeUpperBound(int range) {
  switch(range) {
    case 0: return 100.0f;
    case 1: return 300.0f;
    case 2: return 700.0f;
    case 3: return 1500.0f;
    default: return 9999.0f;
  }
}

// ฟังก์ชันแสดงชื่อช่วงระยะ
const char* getRangeLabel(int range) {
  switch (range) {
    case 0: return "0-100cm";
    case 1: return "100-300cm";
    case 2: return "300-700cm";
    case 3: return "700-1.5k";
    case 4: return "1.5k-3k";
    default: return "Out";
  }
}

// ==================== Soft Handoff Calculation ====================

float calculateSingleReg_Type1(float raw_cm, int anchor_id, int range) {
  float a = LINEAR_A[range][anchor_id];
  float b = LINEAR_B[range][anchor_id];
  return a + b * raw_cm;
}

float calculateSingleReg_Type2(float raw_cm, int anchor_id, int range) {
  float p0 = POLY_P0[range][anchor_id];
  float p1 = POLY_P1[range][anchor_id];
  float p2 = POLY_P2[range][anchor_id];
  return p0 + p1 * raw_cm + p2 * raw_cm * raw_cm;
}

float calculateSingleReg_Type3(float raw_cm, int anchor_id, int range) {
  float c = ERROR_C[range][anchor_id];
  float d = ERROR_D[range][anchor_id];
  float error = c + d * raw_cm;
  return raw_cm + error;
}

// Soft Handoff Engine
float applySoftHandoff(float raw_cm, int anchor_id, int reg_type,
                       float (*calcFunc)(float, int, int)) {
  int current_range = getDistanceRange(raw_cm);
  float val_main = calcFunc(raw_cm, anchor_id, current_range);

  // ตรวจสอบ Transition Zone
  if (current_range < 4) {
    float upper_bound = getRangeUpperBound(current_range);
    
    // ถ้าอยู่ในโซนเกลี่ย (ก่อนถึงขอบ 20cm)
    if (raw_cm > (upper_bound - TRANSITION_ZONE_WIDTH / 2.0f)) {
      int next_range = current_range + 1;
      float val_next = calcFunc(raw_cm, anchor_id, next_range);

      // คำนวณ weight (0.0 -> 1.0)
      float dist_into_zone = raw_cm - (upper_bound - TRANSITION_ZONE_WIDTH / 2.0f);
      float weight = dist_into_zone / (TRANSITION_ZONE_WIDTH / 2.0f);
      
      if (weight < 0.0f) weight = 0.0f;
      if (weight > 1.0f) weight = 1.0f;

      // Linear Interpolation
      return (val_main * (1.0f - weight)) + (val_next * weight);
    }
  }
  
  return val_main;
}

// Regression Functions (With Soft Handoff)
float getRegressionValue_Type1(float raw_cm, int anchor_id) {
  return applySoftHandoff(raw_cm, anchor_id, 1, calculateSingleReg_Type1);
}

float getRegressionValue_Type2(float raw_cm, int anchor_id) {
  return applySoftHandoff(raw_cm, anchor_id, 2, calculateSingleReg_Type2);
}

float getRegressionValue_Type3(float raw_cm, int anchor_id) {
  return applySoftHandoff(raw_cm, anchor_id, 3, calculateSingleReg_Type3);
}

float getRegressionValue(float raw_cm, int anchor_id, int reg_type) {
  switch (reg_type) {
    case 1: return getRegressionValue_Type1(raw_cm, anchor_id);
    case 2: return getRegressionValue_Type2(raw_cm, anchor_id);
    case 3: return getRegressionValue_Type3(raw_cm, anchor_id);
    default: return raw_cm;
  }
}

// ==================== Bias Correction (ORIGINAL) ====================
float calculatePiecewiseBias(float raw_distance_cm, int anchor_id) {
  float d = raw_distance_cm;
  float bias = 0;

  if (anchor_id == 1) {
    if (d < 15)       bias = 21.0f  + (d - 10.0f) * 0.6f;
    else if (d < 25)  bias = 24.0f  + (d - 15.0f) * 1.095f;
    else if (d < 50)  bias = 34.95f + (d - 25.0f) * 0.388f;
    else if (d < 100) bias = 44.65f + (d - 50.0f) * 0.084f;
    else if (d < 200) bias = 48.85f + (d - 100.0f) * 0.0565f;
    else if (d < 500) bias = 54.50f - (d - 200.0f) * 0.0153f;
    else if (d < 1000)bias = 49.90f + (d - 500.0f) * 0.068f;
    else if (d < 2000)bias = 83.90f + (d - 1000.0f)* 0.1046f;
    else if (d < 3000)bias = 188.50f - (d - 2000.0f)* 0.2715f;
    else              bias = -83.0f;
  } else if (anchor_id == 2) {
    float base_bias = calculatePiecewiseBias(d, 1);
    if (d < 100)      bias = base_bias + 5.0f  + d * 0.02f;
    else if (d < 500) bias = base_bias + 7.0f  + d * 0.015f;
    else              bias = base_bias + 40.0f + d * 0.008f;
  } else {
    float base_bias = calculatePiecewiseBias(d, 1);
    if (d < 100)      bias = base_bias + 2.0f  + d * 0.03f;
    else if (d < 500) bias = base_bias + 15.0f + d * 0.02f;
    else              bias = base_bias + 10.0f + d * 0.012f;
  }

  return bias;
}

// ==================== Global Variables ====================
Adafruit_SSD1306 display(128, 64, &Wire, -1);
HardwareSerial SERIAL_AT(2);

String  rxLine;
int     range_list[8] = {0};
float   rssi_list[8]  = {0};
uint32_t last_seq     = 0;

bool     espnow_ready     = false;
uint32_t tx_seq           = 0;

float prev_A1_cm = 0.0f;
float prev_A2_cm = 0.0f;
float prev_A3_cm = 0.0f;

float raw_A1_cm = 0.0f, raw_A2_cm = 0.0f, raw_A3_cm = 0.0f;
float reg1_A1_cm = 0.0f, reg1_A2_cm = 0.0f, reg1_A3_cm = 0.0f;
float reg2_A1_cm = 0.0f, reg2_A2_cm = 0.0f, reg2_A3_cm = 0.0f;
float reg3_A1_cm = 0.0f, reg3_A2_cm = 0.0f, reg3_A3_cm = 0.0f;

// ==================== ESP-NOW Packet ====================
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
  int16_t  A1_reg3, A2_reg3, A3_reg3;
} espnow_pkt_t;

// ==================== Functions ====================
static void getStaMac(uint8_t mac[6]) {
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    bool z = true;
    for (int i = 0; i < 6; i++) if (mac[i]) { z = false; break; }
    if (!z) return;
  }
  String s = WiFi.macAddress();
  if (s.length() >= 17) {
    int v[6];
    if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x",
               &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6) {
      for (int i = 0; i < 6; i++) mac[i] = (uint8_t)v[i];
      return;
    }
  }
  memset(mac, 0, 6);
}

static uint8_t getPrimaryChannel() {
  uint8_t primary = 0;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&primary, &second);
  return primary;
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

void drawScreen(float rawA1, float rawA2, float rawA3,
                float r1A1, float r1A2, float r1A3,
                float r2A1, float r2A2, float r2A3,
                float r3A1, float r3A2, float r3A3) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  int range = getDistanceRange(rawA1);
  display.printf("TAG-%d %s", TAG_ID, getRangeLabel(range));

  display.setCursor(0, 10);
  display.printf("A1 R%.0f 1:%.0f 2:%.0f 3:%.0f", rawA1, r1A1, r2A1, r3A1);

  display.setCursor(0, 20);
  display.printf("A2 R%.0f 1:%.0f 2:%.0f 3:%.0f", rawA2, r1A2, r2A2, r3A2);

  display.setCursor(0, 30);
  display.printf("A3 R%.0f 1:%.0f 2:%.0f 3:%.0f", rawA3, r1A3, r2A3, r3A3);

  display.setCursor(0, 42);
  display.print("R=Raw 1=Lin 2=Poly");

  display.setCursor(0, 52);
  display.print("3=Err [v3.1 FINAL]");

  display.display();
}

void drawBootSplash() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("TAG SENDER");
  display.println(FW_VERSION);
  display.println("");
  display.println("5 Ranges + Smooth");
  display.println("Real Coefficients");
  display.println("");
  display.println("100 Samples Trained");
  display.display();
}

void parseRange(const String &line) {
  int s1 = line.indexOf("seq:");
  if (s1 >= 0) {
    int comma = line.indexOf(",", s1);
    last_seq = (uint32_t)line.substring(s1 + 4, (comma < 0) ? line.length() : comma).toInt();
  }

  for (int i = 0; i < 8; i++) {
    range_list[i] = 0;
    rssi_list[i]  = 0;
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

  // แก้ bias
  float bias_A1 = calculatePiecewiseBias(range_list[0], 1);
  float bias_A2 = calculatePiecewiseBias(range_list[1], 2);
  float bias_A3 = calculatePiecewiseBias(range_list[2], 3);

  raw_A1_cm = range_list[0] - bias_A1;
  raw_A2_cm = range_list[1] - bias_A2;
  raw_A3_cm = range_list[2] - bias_A3;

  if (raw_A1_cm < 5.0f) raw_A1_cm = 5.0f;
  if (raw_A2_cm < 5.0f) raw_A2_cm = 5.0f;
  if (raw_A3_cm < 5.0f) raw_A3_cm = 5.0f;

  if (raw_A1_cm > 10000.0f) raw_A1_cm = prev_A1_cm;
  if (raw_A2_cm > 10000.0f) raw_A2_cm = prev_A2_cm;
  if (raw_A3_cm > 10000.0f) raw_A3_cm = prev_A3_cm;

  prev_A1_cm = raw_A1_cm;
  prev_A2_cm = raw_A2_cm;
  prev_A3_cm = raw_A3_cm;

  // คำนวณ Regression ทั้ง 3 แบบ (WITH SOFT HANDOFF)
  reg1_A1_cm = getRegressionValue(raw_A1_cm, 0, 1);
  reg1_A2_cm = getRegressionValue(raw_A2_cm, 1, 1);
  reg1_A3_cm = getRegressionValue(raw_A3_cm, 2, 1);

  reg2_A1_cm = getRegressionValue(raw_A1_cm, 0, 2);
  reg2_A2_cm = getRegressionValue(raw_A2_cm, 1, 2);
  reg2_A3_cm = getRegressionValue(raw_A3_cm, 2, 2);

  reg3_A1_cm = getRegressionValue(raw_A1_cm, 0, 3);
  reg3_A2_cm = getRegressionValue(raw_A2_cm, 1, 3);
  reg3_A3_cm = getRegressionValue(raw_A3_cm, 2, 3);

  Serial.printf(
    "[DATA] Raw: %.0f,%.0f,%.0f | T1: %.0f,%.0f,%.0f | T2: %.0f,%.0f,%.0f | T3: %.0f,%.0f,%.0f\n",
    raw_A1_cm, raw_A2_cm, raw_A3_cm,
    reg1_A1_cm, reg1_A2_cm, reg1_A3_cm,
    reg2_A1_cm, reg2_A2_cm, reg2_A3_cm,
    reg3_A1_cm, reg3_A2_cm, reg3_A3_cm
  );

  drawScreen(raw_A1_cm, raw_A2_cm, raw_A3_cm,
             reg1_A1_cm, reg1_A2_cm, reg1_A3_cm,
             reg2_A1_cm, reg2_A2_cm, reg2_A3_cm,
             reg3_A1_cm, reg3_A2_cm, reg3_A3_cm);
}

void onEspNowSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("[TAG->NODE] ");
  if (mac_addr) {
    for (int i = 0; i < 6; i++) Serial.printf("%02X%s", mac_addr[i], (i < 5) ? ":" : "");
  } else {
    Serial.print("NULL");
  }
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? " OK" : " FAIL");
}

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

  p.A1_reg3 = (int16_t)reg3_A1_cm;
  p.A2_reg3 = (int16_t)reg3_A2_cm;
  p.A3_reg3 = (int16_t)reg3_A3_cm;

  esp_err_t err = esp_now_send(NODE_PEER_MAC, (uint8_t*)&p, sizeof(p));
  if (err != ESP_OK) {
    Serial.printf("[TAG] esp_now_send err=%d\n", (int)err);
  }
}

void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < 40 && !Serial; i++) delay(50);
  delay(300);
  Serial.println();
  Serial.println("========================================");
  Serial.printf("[BOOT] TAG %s - FINAL VERSION\n", FW_VERSION);
  Serial.println("========================================");

  pinMode(UWB_RST, OUTPUT);
  digitalWrite(UWB_RST, HIGH);

  SERIAL_AT.begin(115200, SERIAL_8N1, IO_RXD2, IO_TXD2);
  Serial.println("[SETUP] UART2 initialized");

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[ERROR] SSD1306 OLED init failed!");
    while (1) delay(1000);
  }
  Serial.println("[SETUP] OLED initialized");
  display.clearDisplay();
  display.display();
  drawBootSplash();
  delay(2000);

  Serial.println("[SETUP] Configuring UWB module...");
  sendAT("AT?");
  sendAT("AT+RESTORE", 3000);

  char cfg[40];
  sprintf(cfg, "AT+SETCFG=%d,%d,1,1", TAG_ID, 0);
  sendAT(cfg, 1500);
  Serial.printf("[SETUP] UWB Config: TAG_ID=%d, ROLE=0\n", TAG_ID);

  sendAT("AT+SETCAP=4,10,1", 1000);
  sendAT("AT+SETRPT=1", 800);
  sendAT("AT+SAVE", 500);
  sendAT("AT+RESTART", 2000);
  Serial.println("[SETUP] UWB configured and restarted");

  Serial.println("[SETUP] Initializing ESP-NOW...");
  WiFi.mode(WIFI_STA);

  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  delay(50);

  uint8_t mac[6];
  getStaMac(mac);
  Serial.printf("[INFO] TAG MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("[INFO] Wi-Fi Channel: %u\n", (unsigned)getPrimaryChannel());

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] ESP-NOW init failed!");
  } else {
    esp_now_register_send_cb(onEspNowSent);

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, NODE_PEER_MAC, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) == ESP_OK) {
      espnow_ready = true;
      Serial.print("[SETUP] ESP-NOW ready. NODE MAC: ");
      for (int i = 0; i < 6; i++)
        Serial.printf("%02X%s", NODE_PEER_MAC[i], (i < 5) ? ":" : "\n");
      Serial.printf("[SETUP] Peer Channel: %u\n", (unsigned)ESPNOW_CHANNEL);
    } else {
      Serial.println("[ERROR] Failed to add Node as peer");
    }
  }

  Serial.println("========================================");
  Serial.printf("[READY] TAG %s - REAL COEFFICIENTS\n", FW_VERSION);
  Serial.println("[READY] 100 Samples, 5 Ranges, Smoothed\n");
  Serial.println("========================================");
}

void loop() {
  static uint32_t t_uwb = 0;
  if (millis() - t_uwb > 200) {
    sendAT("AT+RDATA", 0);
    t_uwb = millis();
  }

  while (SERIAL_AT.available()) {
    char c = SERIAL_AT.read();
    if (c == '\n') {
      if (rxLine.startsWith("AT+RANGE")) {
        parseRange(rxLine);
      }
      rxLine = "";
    } else if (c != '\r') {
      rxLine += c;
    }
  }

  static uint32_t t_espnow = 0;
  if (espnow_ready && millis() - t_espnow > 200) {
    sendNow_AllData();
    t_espnow = millis();
  }

  static uint32_t t_heartbeat = 0;
  if (millis() - t_heartbeat > 5000) {
    Serial.printf("[HEARTBEAT] Uptime: %lu ms | v3.1.0 FINAL ACTIVE\n",
                  (unsigned long)millis());
    t_heartbeat = millis();
  }

  while (Serial.available()) {
    SERIAL_AT.write(Serial.read());
  }
}
