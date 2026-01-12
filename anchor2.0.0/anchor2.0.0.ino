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

// ==================== Loop ====================
void loop() {
  // 1. อ่านข้อมูลจาก GPS ตลอดเวลา
  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
  }

  // 2. ทำงานทุกๆ 1 วินาที (ส่งข้อมูล + อัพเดทจอ)
  if (millis() - lastSendTime > sendInterval) {
    lastSendTime = millis();

    // เตรียมข้อมูล
    myData.id = ANCHOR__ID;
    
    if (gps.location.isValid()) {
      myData.lat = gps.location.lat();
      myData.lng = gps.location.lng();
      myData.valid = true;
    } else {
      myData.lat = 0.0;
      myData.lng = 0.0;
      myData.valid = false;
    }

    // ส่งข้อมูลผ่าน ESP-NOW
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

    // แสดงผลบน OLED
    display.clearDisplay();
    
    // Header
    display.setTextSize(1);
    display.setCursor(0,0);
    display.print("ANCHOR "); display.print(ANCHOR__ID);
    display.setCursor(60,0);
    display.print(result == ESP_OK ? "[SENT]" : "[FAIL]");

    // GPS Status
    display.setCursor(0,12);
    if (myData.valid) {
      display.print("GPS: FIX (");
      display.print(gps.satellites.value());
      display.print(" Sats)");
      
      display.setCursor(0,25);
      display.print("Lat: "); display.print(myData.lat, 6);
      display.setCursor(0,35);
      display.print("Lng: "); display.print(myData.lng, 6);
    } else {
      display.print("GPS: SEARCHING...");
      display.setCursor(0,25);
      display.print("No Satellite Fix");
      display.setCursor(0,35);
      display.print("Check Antenna!");
    }

    // UWB Status
    display.setCursor(0,55);
    display.print("UWB: TX-Mode Active");

    display.display();
    
    // Debug Serial
    if(myData.valid) {
      Serial.printf("Anchor %d | Lat: %.6f, Lng: %.6f | Sent\n", myData.id, myData.lat, myData.lng);
    } else {
      Serial.printf("Anchor %d | Waiting for GPS...\n", myData.id);
    }
  }
}
