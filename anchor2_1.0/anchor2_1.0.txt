/********************************************************************
  ANCHOR v2.1.0 - GPS Enabled & ESP-NOW Broadcasting
  
  หน้าที่:
  1. ทำหน้าที่เป็น UWB Anchor (TX-Only) เหมือนเดิม
  2. อ่านค่าพิกัดจากโมดูล GPS (GT-U7)
  3. ส่งค่าพิกัด (Lat/Lon) และ ID ของตัวเองผ่าน ESP-NOW (Broadcast)
     เพื่อให้ Tag นำไปคำนวณตำแหน่งจริงบนโลก
  
  Hardware: 
  - ESP32-S3 + MaUWB (DW3000)
  - OLED (I2C)
  - GPS GT-U7 (UART1)
  
  ผู้พัฒนา: สำหรับโครงการเรือไร้คนขับ
  วันที่: 2026-01-12
********************************************************************/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HardwareSerial.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <esp_now.h>

// ==================== การตั้งค่าผู้ใช้ (User Config) ====================
// *** เลือก ID สำหรับ Anchor แต่ละตัว (1, 2, หรือ 3) ***
#define ANCHOR__ID   1        // <--- เปลี่ยนเป็น 1, 2, หรือ 3 ตามตัวเครื่อง

// ==================== Pin Definitions ====================
// OLED
#define I2C_SDA     39
#define I2C_SCL     38

// UWB (UART2)
#define UWB_RXD2    18  // ต่อกับ TX ของโมดูล UWB
#define UWB_TXD2    17  // ต่อกับ RX ของโมดูล UWB
#define UWB_RST     16

// GPS (UART1) - ใหม่
#define GPS_RX_PIN  4   // ต่อกับ TX ของ GT-U7
#define GPS_TX_PIN  5   // ต่อกับ RX ของ GT-U7

// ==================== Global Objects ====================
Adafruit_SSD1306 display(128, 64, &Wire, -1);
HardwareSerial SERIAL_AT(2);      // UWB
HardwareSerial GPS_Serial(1);     // GPS
TinyGPSPlus gps;

// ==================== เพิ่มตัวแปร Global (ใส่หลัง TinyGPSPlus gps;) ====================
unsigned long gps_fix_time = 0;        // เวลาที่ได้ Fix ครั้งแรก (millis)
bool gps_ever_fixed = false;           // เคยได้ Fix หรือยัง
uint32_t gps_data_age_ms = 0;          // อายุของข้อมูล GPS (ms)

// โครงสร้างข้อมูลที่จะส่งผ่าน ESP-NOW
typedef struct struct_message {
  int id;           // Anchor ID (1, 2, 3)
  float lat;        // Latitude
  float lng;        // Longitude
  bool valid;       // GPS Fix status
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// Broadcast Address (ส่งหาทุกคน)
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ตัวแปรจับเวลา
unsigned long lastSendTime = 0;
const int sendInterval = 1000; // ส่งข้อมูลทุก 1 วินาที

// ==================== Helper: UWB AT Command ====================
String sendAT(const String &cmd, uint32_t timeout_ms = 800, bool echo = false) {
  String resp;
  if (echo) Serial.println(cmd);
  SERIAL_AT.println(cmd);
  uint32_t t0 = millis();
  while (millis() - t0 < timeout_ms) {
    while (SERIAL_AT.available()) {
      resp += (char)SERIAL_AT.read();
    }
  }
  return resp;
}

// ==================== Helper: ESP-NOW Callback ====================
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // ไม่ต้องทำอะไรมาก แค่ debug
  // Serial.print("\r\nLast Packet Send Status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// ==================== Setup ====================
void setup() {
  // 1. Init Serial & Pins
  Serial.begin(115200);
  pinMode(UWB_RST, OUTPUT);
  digitalWrite(UWB_RST, HIGH);

  // 2. Init OLED
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Booting Anchor " + String(ANCHOR__ID) + "...");
  display.display();

  // 3. Init GPS Serial
  GPS_Serial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println(F("GPS Serial Started"));

  // 4. Init UWB (เหมือน v2.0.0)
  SERIAL_AT.begin(115200, SERIAL_8N1, UWB_RXD2, UWB_TXD2);
  Serial.println(F("Configuring UWB..."));
  sendAT("AT+RESTORE", 2000);
  char cfg[40];
  // Anchor ID logic: (USER_ID - 1) -> 0, 1, 2
  sprintf(cfg, "AT+SETCFG=%d,1,1,1", ANCHOR__ID - 1); 
  sendAT(cfg, 1000);
  sendAT("AT+SETCAP=4,10,1", 1000);
  sendAT("AT+SETRPT=0", 500); // TX-Only ไม่ต้องรายงานระยะกลับมาที่ ESP
  sendAT("AT+SAVE", 500);
  sendAT("AT+RESTART", 2000);

  // 5. Init ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(OnDataSent);

  // Register Peer (Broadcast)
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  Serial.println("Anchor v2.1.0 Ready!");
}

// ==================== Loop (แทนที่ทั้งหมด) ====================
void loop() {
  // 1. อ่านข้อมูลจาก GPS ตลอดเวลา
  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
  }

  // 2. ตรวจสอบอายุข้อมูล GPS (ถ้าเงียบนานเกิน 2 วินาที = GPS หลุด)
  if (gps.location.isValid()) {
    gps_data_age_ms = gps.location.age(); // TinyGPS++ มี .age() บอกอายุข้อมูล
  } else {
    gps_data_age_ms = 9999; // ค่า Error สูงถ้าไม่มีข้อมูล
  }

  // 3. ส่งข้อมูลและอัปเดตจอทุกๆ 1 วินาที
  if (millis() - lastSendTime > sendInterval) {
    lastSendTime = millis();

    // เตรียมข้อมูลส่ง ESP-NOW
    myData.id = ANCHOR__ID;
    
    if (gps.location.isValid()) {
      myData.lat = gps.location.lat();
      myData.lng = gps.location.lng();
      myData.valid = true;
      
      // บันทึกเวลาที่ได้ Fix ครั้งแรก
      if (!gps_ever_fixed) {
        gps_fix_time = millis();
        gps_ever_fixed = true;
      }
    } else {
      myData.lat = 0.0;
      myData.lng = 0.0;
      myData.valid = false;
    }

    // ส่งข้อมูลผ่าน ESP-NOW
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

    // ==================== อัปเดตหน้าจอ OLED (ปรับปรุงใหม่) ====================
    display.clearDisplay();
    
    // บรรทัดที่ 1 (y=0): Header + ESP-NOW Status
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("ANCHOR ");
    display.print(ANCHOR__ID);
    display.setCursor(80, 0);
    display.print(result == ESP_OK ? "[OK]" : "[ERR]");

    // บรรทัดที่ 2 (y=12): สถานะ GPS หลัก
    display.setCursor(0, 12);
    if (myData.valid) {
      // แสดง FIX พร้อมจำนวนดาวเทียม + HDOP
      uint32_t sats = gps.satellites.value();
      uint32_t hdop_val = gps.hdop.value(); // HDOP * 100 (เช่น 150 = 1.50)
      
      display.printf("FIX %uSat H%.1f", sats, hdop_val / 100.0);
      
      // บรรทัดที่ 3-4 (y=22, 32): พิกัด (ย่อให้สั้น)
      display.setCursor(0, 22);
      display.printf("N:%.6f", myData.lat);  // 13 ตัว
      display.setCursor(0, 32);
      display.printf("E:%.6f", myData.lng);  // 13-14 ตัว
      
      // บรรทัดที่ 5 (y=42): อายุข้อมูล GPS + เวลาที่ได้ Fix
      display.setCursor(0, 42);
      display.printf("Age:%lums", gps_data_age_ms);
      
      // บรรทัดที่ 6 (y=52): เวลาตั้งแต่ได้ Fix (วินาที)
      display.setCursor(0, 52);
      uint32_t fix_duration_sec = (millis() - gps_fix_time) / 1000;
      display.printf("FixFor:%lus", fix_duration_sec);
      
    } else {
      // กรณียังไม่ได้ Fix
      uint32_t sats_visible = gps.satellites.value();
      
      display.print("SEARCHING...");
      
      display.setCursor(0, 22);
      if (sats_visible > 0) {
        display.printf("Tracking:%u Sats", sats_visible);
      } else {
        display.print("No Satellites");
      }
      
      display.setCursor(0, 32);
      display.print("Check Sky View!");
      
      display.setCursor(0, 42);
      // แสดง Uptime (เวลาตั้งแต่ Boot)
      uint32_t uptime_sec = millis() / 1000;
      display.printf("Boot:%lus", uptime_sec);
      
      display.setCursor(0, 52);
      if (gps_ever_fixed) {
        display.print("Lost Fix!");
      } else {
        display.print("Wait 1st Fix...");
      }
    }

    display.display();
    
    // ==================== Debug Serial ====================
    if (myData.valid) {
      Serial.printf("[A%d] FIX | Lat:%.6f Lng:%.6f | Sats:%u HDOP:%.1f Age:%lums\n",
                    myData.id, myData.lat, myData.lng,
                    gps.satellites.value(), gps.hdop.value() / 100.0, gps_data_age_ms);
    } else {
      Serial.printf("[A%d] SEARCH | Visible:%u Sats | Uptime:%lus\n",
                    myData.id, gps.satellites.value(), millis() / 1000);
    }
  }
}