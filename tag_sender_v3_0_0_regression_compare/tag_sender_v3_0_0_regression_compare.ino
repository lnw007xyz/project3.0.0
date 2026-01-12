/*
 * =====================================================
 * TAG SENDER v3.0.1 - NO BIAS CORRECTION + 6 RANGES
 * =====================================================
 * พัฒนาจาก: v3.0.0
 * 
 * ✅ ใช้ RAW DATA โดยตรง (ไม่แก้ bias)
 * ✅ คำนวณ Regression จากข้อมูลจริงทั้งหมด (333 samples)
 * ✅ 6 ช่วงระยะ: 10-50, 50-100, 100-500, 500-1k, 1k-3k, 3k+
 * ✅ 3 แบบ Regression:
 *    - Type 1: Linear from RAW
 *    - Type 2: Polynomial from RAW
 *    - Type 3: Linear from Bias-Corrected (เพื่อเปรียบเทียบ)
 * 
 * ESP32 Arduino Core v3.x (ESP-IDF 5.x)
 * วันที่: 2025-12-24
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

// ==================== Configuration ====================
#define TAG_ID           0
#define I2C_SDA          39
#define I2C_SCL          38
#define IO_RXD2          18
#define IO_TXD2          17
#define UWB_RST          16

static const char* FW_VERSION = "v3.0.1";
uint8_t NODE_PEER_MAC[6] = { 0x30, 0xED, 0xA0, 0x1F, 0x00, 0x24 };
static const uint8_t ESPNOW_CHANNEL = 11;

// ==================== Type 1: Linear (y = a + b*x) from RAW ====================
const float LINEAR_A[6][3] = {
  {-7.920987, -11.788778, -10.975864},  // Range 0: 10-50cm
  {-45.320531, -150.276048, -72.250721},  // Range 1: 50-100cm
  {-49.867610, -40.024464, -45.699689},  // Range 2: 100-500cm
  {28.184050, -12.100354, -64.641792},  // Range 3: 500-1k
  {14.078272, -1195.577055, -713.873774},  // Range 4: 1k-3k
  {3000.000000, 3000.000000, 3000.000000},  // Range 5: 3k+
};

const float LINEAR_B[6][3] = {
  {0.560630, 0.629175, 0.614822},  // Range 0
  {1.011239, 2.015586, 1.267073},  // Range 1
  {0.996282, 1.001772, 0.963717},  // Range 2
  {0.843386, 0.829404, 0.929569},  // Range 3
  {0.907347, 1.531787, 1.262893},  // Range 4
  {0.000000, 0.000000, -0.000000},  // Range 5
};

// ==================== Type 2: Polynomial (y = p0 + p1*x + p2*x^2) from RAW ====================
const float POLY_P0[6][3] = {
  {0.506960, -22.550835, -4.121911},  // Range 0
  {-269.371748, -56.450537, -798.792456},  // Range 1
  {-26.224694, -35.543167, 63.851301},  // Range 2
  {28.184050, -12.100354, -64.641792},  // Range 3
  {1979.470102, -1937.855246, -1707.421918},  // Range 4
  {3000.000000, 3000.000000, 3000.000000},  // Range 5
};

const float POLY_P1[6][3] = {
  {0.232765, 1.015710, 0.353741},  // Range 0
  {5.345956, 0.212204, 15.067108},  // Range 1
  {0.782898, 0.959391, -0.054772},  // Range 2
  {0.843386, 0.829404, 0.929569},  // Range 3
  {-1.800532, 2.405390, 2.455907},  // Range 4
  {0.000000, 0.000000, 0.000000},  // Range 5
};

const float POLY_P2[6][3] = {
  {0.002877630, -0.003227560, 0.002288546},  // Range 0
  {-0.020781949, 0.008649596, -0.065233038},  // Range 1
  {0.000429187, 0.000088578, 0.002082892},  // Range 2
  {0.000000000, -0.000000000, 0.000000000},  // Range 3
  {0.000827063, -0.000248206, -0.000340038},  // Range 4
  {0.000000000, 0.000000000, 0.000000000},  // Range 5
};

// ==================== Type 3: Linear from Bias-Corrected (เพื่อเปรียบเทียบ) ====================
const float LINEAR_CORRECTED_A[6][3] = {
  {14.922785, 19.044369, 16.907343},  // Range 0
  {0.404563, -53.644725, 33.645219},  // Range 1
  {1.779366, 19.264210, 19.468327},  // Range 2
  {42.572294, 38.076809, -38.472405},  // Range 3
  {60.472990, -1046.932328, -595.624284},  // Range 4
  {3000.000000, 3000.000000, 3000.000000},  // Range 5
};

const float LINEAR_CORRECTED_B[6][3] = {
  {0.688664, 0.756356, 0.772887},  // Range 0
  {1.080780, 2.419191, 0.612479},  // Range 1
  {0.999623, 1.023673, 0.989268},  // Range 2
  {0.904921, 0.897624, 1.010401},  // Range 3
  {0.945515, 1.634274, 1.320791},  // Range 4
  {0.000000, 0.000000, -0.000000},  // Range 5
};

// ==================== Bias Correction (สำหรับ Type 3 เท่านั้น) ====================
float calculatePiecewiseBias(float raw_distance_cm, int anchor_id) {
  float d = raw_distance_cm;
  float bias = 0;
  
  if (anchor_id == 1) {
    if (d < 15)        bias = 21.0f  + (d - 10.0f) * 0.6f;
    else if (d < 25)   bias = 24.0f  + (d - 15.0f) * 1.095f;
    else if (d < 50)   bias = 34.95f + (d - 25.0f) * 0.388f;
    else if (d < 100)  bias = 44.65f + (d - 50.0f) * 0.084f;
    else if (d < 200)  bias = 48.85f + (d - 100.0f) * 0.0565f;
    else if (d < 500)  bias = 54.50f - (d - 200.0f) * 0.0153f;
    else if (d < 1000) bias = 49.90f + (d - 500.0f) * 0.068f;
    else if (d < 2000) bias = 83.90f + (d - 1000.0f)* 0.1046f;
    else if (d < 3000) bias = 188.50f - (d - 2000.0f)* 0.2715f;
    else               bias = -83.0f;
  } else if (anchor_id == 2) {
    float base_bias = calculatePiecewiseBias(d, 1);
    if (d < 100)       bias = base_bias + 5.0f  + d * 0.02f;
    else if (d < 500)  bias = base_bias + 7.0f  + d * 0.015f;
    else               bias = base_bias + 40.0f + d * 0.008f;
  } else {
    float base_bias = calculatePiecewiseBias(d, 1);
    if (d < 100)       bias = base_bias + 2.0f  + d * 0.03f;
    else if (d < 500)  bias = base_bias + 15.0f + d * 0.02f;
    else               bias = base_bias + 10.0f + d * 0.012f;
  }
  
  return bias;
}

// ==================== Helper Functions ====================
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
    case 0: return "10-50cm";
    case 1: return "50-100";
    case 2: return "100-500";
    case 3: return "500-1k";
    case 4: return "1k-3k";
    case 5: return "3k+";
    default: return "???";
  }
}

// ==================== Regression Functions ====================
float getRegressionValue_Type1(float raw_cm, int anchor_id) {
  int range = getDistanceRange(raw_cm);
  float a = LINEAR_A[range][anchor_id];
  float b = LINEAR_B[range][anchor_id];
  return a + b * raw_cm;
}

float getRegressionValue_Type2(float raw_cm, int anchor_id) {
  int range = getDistanceRange(raw_cm);
  float p0 = POLY_P0[range][anchor_id];
  float p1 = POLY_P1[range][anchor_id];
  float p2 = POLY_P2[range][anchor_id];
  return p0 + p1 * raw_cm + p2 * raw_cm * raw_cm;
}

float getRegressionValue_Type3(float raw_cm, int anchor_id) {
  // Type 3: ใช้ bias-corrected data (เพื่อเปรียบเทียบ)
  float bias = calculatePiecewiseBias(raw_cm, anchor_id + 1);
  float corrected_cm = raw_cm - bias;
  
  int range = getDistanceRange(raw_cm);
  float a = LINEAR_CORRECTED_A[range][anchor_id];
  float b = LINEAR_CORRECTED_B[range][anchor_id];
  return a + b * corrected_cm;
}

// ==================== Globals ====================
Adafruit_SSD1306 display(128, 64, &Wire, -1);
HardwareSerial SERIAL_AT(2);

String  rxLine;
int     range_list[8] = {0};
uint32_t last_seq     = 0;

bool     espnow_ready = false;
uint32_t tx_seq       = 0;

float prev_A1_cm = 0.0f, prev_A2_cm = 0.0f, prev_A3_cm = 0.0f;
float raw_A1_cm  = 0.0f, raw_A2_cm  = 0.0f, raw_A3_cm  = 0.0f;
float reg1_A1_cm = 0.0f, reg1_A2_cm = 0.0f, reg1_A3_cm = 0.0f;
float reg2_A1_cm = 0.0f, reg2_A2_cm = 0.0f, reg2_A3_cm = 0.0f;
float reg3_A1_cm = 0.0f, reg3_A2_cm = 0.0f, reg3_A3_cm = 0.0f;

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

static void getStaMac(uint8_t mac[6]) {
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    bool z = true;
    for (int i = 0; i < 6; i++) if (mac[i]) { z = false; break; }
    if (!z) return;
  }
  String s = WiFi.macAddress();
  if (s.length() >= 17) {
    int v[6];
    if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6) {
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
  display.printf("TAG-%d %s [v3.0.1]", TAG_ID, getRangeLabel(range));

  display.setCursor(0, 12);
  display.printf("A1 R%.0f 1:%.0f 2:%.0f 3:%.0f", rawA1, r1A1, r2A1, r3A1);
  display.setCursor(0, 22);
  display.printf("A2 R%.0f 1:%.0f 2:%.0f 3:%.0f", rawA2, r1A2, r2A2, r3A2);
  display.setCursor(0, 32);
  display.printf("A3 R%.0f 1:%.0f 2:%.0f 3:%.0f", rawA3, r1A3, r2A3, r3A3);

  display.setCursor(0, 44);
  display.print("R=RAW(no bias)");
  display.setCursor(0, 54);
  display.print("1:Lin 2:Poly 3:BiasLin");

  display.display();
}

void drawBootSplash() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("TAG SENDER v3.0.1");
  display.println("");
  display.println("NO BIAS CORRECTION");
  display.println("6 ranges, 333 samples");
  display.println("");
  display.println("1:Linear RAW");
  display.println("2:Poly RAW");
  display.println("3:Bias-corrected Lin");
  display.display();
}

void parseRange(const String &line) {
  int s1 = line.indexOf("seq:");
  if (s1 >= 0) {
    int comma = line.indexOf(",", s1);
    last_seq = (uint32_t)line.substring(s1 + 4, (comma < 0) ? line.length() : comma).toInt();
  }

  for (int i = 0; i < 8; i++) range_list[i] = 0;

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

  // ใช้ RAW โดยตรง (ไม่แก้ bias)
  raw_A1_cm = range_list[0];
  raw_A2_cm = range_list[1];
  raw_A3_cm = range_list[2];

  if (raw_A1_cm < 5.0f) raw_A1_cm = 5.0f;
  if (raw_A2_cm < 5.0f) raw_A2_cm = 5.0f;
  if (raw_A3_cm < 5.0f) raw_A3_cm = 5.0f;

  if (raw_A1_cm > 10000.0f) raw_A1_cm = prev_A1_cm;
  if (raw_A2_cm > 10000.0f) raw_A2_cm = prev_A2_cm;
  if (raw_A3_cm > 10000.0f) raw_A3_cm = prev_A3_cm;

  prev_A1_cm = raw_A1_cm;
  prev_A2_cm = raw_A2_cm;
  prev_A3_cm = raw_A3_cm;

  // คำนวณ Regression 3 แบบ
  reg1_A1_cm = getRegressionValue_Type1(raw_A1_cm, 0);
  reg1_A2_cm = getRegressionValue_Type1(raw_A2_cm, 1);
  reg1_A3_cm = getRegressionValue_Type1(raw_A3_cm, 2);

  reg2_A1_cm = getRegressionValue_Type2(raw_A1_cm, 0);
  reg2_A2_cm = getRegressionValue_Type2(raw_A2_cm, 1);
  reg2_A3_cm = getRegressionValue_Type2(raw_A3_cm, 2);

  reg3_A1_cm = getRegressionValue_Type3(raw_A1_cm, 0);
  reg3_A2_cm = getRegressionValue_Type3(raw_A2_cm, 1);
  reg3_A3_cm = getRegressionValue_Type3(raw_A3_cm, 2);

  Serial.printf("[DATA] Raw: %.0f,%.0f,%.0f | T1: %.0f,%.0f,%.0f | T2: %.0f,%.0f,%.0f | T3: %.0f,%.0f,%.0f\n",
                raw_A1_cm, raw_A2_cm, raw_A3_cm,
                reg1_A1_cm, reg1_A2_cm, reg1_A3_cm,
                reg2_A1_cm, reg2_A2_cm, reg2_A3_cm,
                reg3_A1_cm, reg3_A2_cm, reg3_A3_cm);

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

  p.A1_raw  = (int16_t)raw_A1_cm;
  p.A2_raw  = (int16_t)raw_A2_cm;
  p.A3_raw  = (int16_t)raw_A3_cm;

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
  if (err != ESP_OK) Serial.printf("[TAG] esp_now_send err=%d\n", (int)err);
}

void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < 40 && !Serial; i++) delay(50);
  delay(300);

  Serial.println();
  Serial.println("========================================");
  Serial.printf("[BOOT] TAG v3.0.1 - NO BIAS\n");
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
  delay(2500);

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
      for (int i = 0; i < 6; i++) Serial.printf("%02X%s", NODE_PEER_MAC[i], (i < 5) ? ":" : "\n");
      Serial.printf("[SETUP] Peer Channel: %u\n", (unsigned)ESPNOW_CHANNEL);
    } else {
      Serial.println("[ERROR] Failed to add Node as peer");
    }
  }

  Serial.println("========================================");
  Serial.printf("[READY] TAG v3.0.1\n");
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
      if (rxLine.startsWith("AT+RANGE")) parseRange(rxLine);
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
    Serial.printf("[HEARTBEAT] v3.0.1 | Uptime: %lu ms\n", (unsigned long)millis());
    t_heartbeat = millis();
  }

  while (Serial.available()) {
    SERIAL_AT.write(Serial.read());
  }
}
