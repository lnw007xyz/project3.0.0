/*
 * =====================================================
 * TAG SENDER v3.0.1 - OPTIMIZED & FULLY COMMENTED (THAI)
 * =====================================================
 * ระบบ UWB Tag Sender สำหรับการระบุตำแหน่งภายในอาคาร (Indoor Positioning)
 * 
 * คุณสมบัติหลักของระบบ:
 * 1. รับข้อมูลระยะทางดิบ (Raw Distance) จากโมดูล UWB ผ่านคำสั่ง AT Command
 * 2. นำระยะทางที่ได้มาคำนวณ Regression 3 รูปแบบ เพื่อเปรียบเทียบความแม่นยำ
 *    - Type 1: Linear Regression (เชิงเส้น)
 *    - Type 2: Polynomial Regression (พหุนามกำลังสอง)
 *    - Type 3: Bias-corrected Linear (เชิงเส้นแบบแก้ค่าเบี่ยงเบน)
 * 3. แสดงผลข้อมูลทั้งหมดบนหน้าจอ OLED Display ขนาด 128x64
 * 4. ส่งข้อมูลทั้งหมดไปยัง Node ตัวแม่ข่ายผ่านโปรโตคอล ESP-NOW
 * 
 * แพลตฟอร์ม: ESP32 Arduino Core v3.x (อิงตาม ESP-IDF 5.x)
 * =====================================================
 */

// ==================== SECTION 1: LIBRARIES IMPORT (การนำเข้าไลบรารี) ====================
#include <Arduino.h>          // ไลบรารีหลักของ Arduino Framework สำหรับการเขียนโปรแกรมบน ESP32
#include <Wire.h>             // ไลบรารีสำหรับการสื่อสารแบบ I2C (ใช้สำหรับหน้าจอ OLED)
#include <Adafruit_GFX.h>     // ไลบรารีกราฟิกพื้นฐานของ Adafruit สำหรับวาดข้อความและรูปทรง
#include <Adafruit_SSD1306.h> // ไลบรารีไดรเวอร์สำหรับควบคุมหน้าจอ OLED ชนิด SSD1306
#include <esp_now.h>          // ไลบรารีสำหรับโปรโตคอลการสื่อสารไร้สาย ESP-NOW (สื่อสารระหว่าง ESP32 โดยตรง)
#include <WiFi.h>             // ไลบรารีหลักสำหรับการจัดการ Wi-Fi บน ESP32
#include <esp_wifi.h>         // ไลบรารีระดับล่าง (Low-level) ของ ESP-IDF สำหรับควบคุมการตั้งค่า Wi-Fi
#include <esp_mac.h>          // ไลบรารีสำหรับอ่านและจัดการ MAC Address ของอุปกรณ์
#include <math.h>             // ไลบรารีคณิตศาสตร์มาตรฐาน (สำหรับฟังก์ชันเช่น pow, sqrt ฯลฯ)

// ==================== SECTION 2: CONFIGURATION (การตั้งค่าระบบ) ====================
#define TAG_ID           0    // กำหนดหมายเลขประจำตัวของ Tag นี้ (ค่าระหว่าง 0-255) เพื่อแยกแยะอุปกรณ์
#define I2C_SDA          39   // กำหนดขาพิน GPIO 39 สำหรับสัญญาณ Data ของ I2C (SDA) เชื่อมต่อหน้าจอ OLED
#define I2C_SCL          38   // กำหนดขาพิน GPIO 38 สำหรับสัญญาณ Clock ของ I2C (SCL) เชื่อมต่อหน้าจอ OLED
#define IO_RXD2          18   // กำหนดขาพิน GPIO 18 เป็น RX ของ UART2 (รับข้อมูลจากโมดูล UWB)
#define IO_TXD2          17   // กำหนดขาพิน GPIO 17 เป็น TX ของ UART2 (ส่งข้อมูลตหาโมดูล UWB)
#define UWB_RST          16   // กำหนดขาพิน GPIO 16 สำหรับสั่ง Reset โมดูล UWB (Active High/Low แล้วแต่โมดูล)

static const char* FW_VERSION = "v3.0.5";  // ตัวแปรเก็บเวอร์ชันของ Firmware สำหรับแสดงผล
// MAC Address ของอุปกรณ์ปลายทาง (Node) ที่จะรับข้อมูลผ่าน ESP-NOW
uint8_t NODE_PEER_MAC[6] = { 0x30, 0xED, 0xA0, 0x1F, 0x00, 0x24 };  
static const uint8_t ESPNOW_CHANNEL = 11;  // ช่องสัญญาณ Wi-Fi ที่จะไช้ในการสื่อสาร (ต้องตรงกันทั้งตัวรับและตัวส่ง)

// ==================== SECTION 3: REGRESSION COEFFICIENTS (ค่าสัมประสิทธิ์การคำนวณ) ====================
// คำอธิบายการแบ่งช่วงระยะทาง (Distance Range Index):
// 0 = 10-50 cm, 1 = 50-100 cm, 2 = 100-500 cm, 
// 3 = 500-1000 cm, 4 = 1000-3000 cm, 5 = มากกว่า 3000 cm
// 
// คำอธิบาย Index ของ Anchor:
// 0 = Anchor 1 (A1), 1 = Anchor 2 (A2), 2 = Anchor 3 (A3)

// --- Type 1: Linear Regression (สมการเส้นตรง y = a + b*x) ---
// ค่าสัมประสิทธิ์ 'a' (จุดตัดแกน y) สำหรับ Linear Regression แยกตามช่วงระยะและ Anchor
const float LINEAR_A_RAW[6][3] = {
  {-7.920987, -11.788778, -10.975864},       // Range 0: 10-50cm (A1, A2, A3)
  {-45.320531, -150.276048, -72.250721},     // Range 1: 50-100cm
  {-46.127010, -6.897944, -48.541441},       // Range 2: 100-500cm
  {-9.863671, -18.451526, 238.863974},       // Range 3: 500-1000cm
  {16.207090, -1189.281891, -722.593423},    // Range 4: 1000-3000cm
  {3000.000000, 3000.000000, 3000.000000},   // Range 5: >3000cm (ค่า Default)
};
// ค่าสัมประสิทธิ์ 'b' (ความชัน) สำหรับ Linear Regression
const float LINEAR_B_RAW[6][3] = {
  {0.560629874, 0.629175295, 0.614821903},   // Range 0
  {1.011238899, 2.015586367, 1.267072780},   // Range 1
  {0.974567451, 0.815575627, 0.979988595},   // Range 2
  {0.928290530, 0.847455072, 0.568337768},   // Range 3
  {0.906326793, 1.528761112, 1.266922277},   // Range 4
  {0.000000000, 0.000000000, -0.000000000},  // Range 5
};

// --- Type 2: Polynomial Regression (สมการพหุนาม y = p0 + p1*x + p2*x^2) ---
// ค่าสัมประสิทธิ์ 'p0' (พจน์ค่าคงที่)
const float POLY_P0_RAW[6][3] = {
  {0.506960, -22.550835, -4.121911},         // Range 0
  {-269.371748, -56.450537, -798.792456},    // Range 1
  {-30.914491, -84.559781, 79.446354},       // Range 2
  {-1713.639273, 1594.617826, 1095.019806},  // Range 3
  {1971.562486, -1591.815677, -1610.998913}, // Range 4
  {3000.000000, 3000.000000, 3000.000000},   // Range 5
};
// ค่าสัมประสิทธิ์ 'p1' (พจน์กำลังหนึ่ง)
const float POLY_P1_RAW[6][3] = {
  {0.232764752, 1.015710318, 0.353741242},   // Range 0
  {5.345956243, 0.212203772, 15.067108369},  // Range 1
  {0.838229897, 1.500325769, -0.219436726},  // Range 2
  {6.252989621, -3.821157546, -2.027975940}, // Range 3
  {-1.789529706, 2.002782068, 2.332959723},  // Range 4
  {0.000000000, 0.000000000, 0.000000000},   // Range 5
};
// ค่าสัมประสิทธิ์ 'p2' (พจน์กำลังสอง)
const float POLY_P2_RAW[6][3] = {
  {0.002877630304, -0.003227560187, 0.002288546413},   // Range 0
  {-0.020781949043, 0.008649595507, -0.065233037975},  // Range 1
  {0.000271495075, -0.001293928520, 0.002477209871},   // Range 2
  {-0.004048981892, 0.003318121619, 0.001904415423},   // Range 3
  {0.000823687047, -0.000134730616, -0.000303717763},  // Range 4
  {0.000000000000, 0.000000000000, 0.000000000000},    // Range 5
};



// [MOVING_AVERAGE REMOVED] ลบ struct และฟังก์ชัน Moving Average ออกตามคำขอ
// กลับไปใช้ค่า Raw ในการคำนวณ Range โดยตรง

// ==================== [MOVING_AVERAGE_END - REMOVED] ====================

// ==================== SECTION 4: HELPER FUNCTIONS (ฟังก์ชันช่วยคำนวณ) ====================

// ฟังก์ชันสำหรับแปลงค่าระยะทาง (cm) เป็นดัชนีช่วง (Range Index) 0-5
int getDistanceRange(float raw_cm) {
  if (raw_cm < 50.0f) return 0;           // ถ้าระยะ < 50cm คืนค่า 0
  else if (raw_cm < 100.0f) return 1;     // ถ้าระยะ < 100cm คืนค่า 1
  else if (raw_cm < 500.0f) return 2;     // ถ้าระยะ < 500cm คืนค่า 2
  else if (raw_cm < 1000.0f) return 3;    // ถ้าระยะ < 1000cm คืนค่า 3
  else if (raw_cm < 3000.0f) return 4;    // ถ้าระยะ < 3000cm คืนค่า 4
  else return 5;                          // ถ้าระยะ >= 3000cm คืนค่า 5
}

// ฟังก์ชันแปลงดัชนีช่วง (Risk Index) เป็นข้อความ (String Label) เพื่อแสดงผล
const char* getRangeLabel(int range) {
  switch (range) {
    case 0: return "10-50";     // ข้อความสำหรับช่วงที่ 0
    case 1: return "50-100";    // ข้อความสำหรับช่วงที่ 1
    case 2: return "100-500";   // ข้อความสำหรับช่วงที่ 2
    case 3: return "500-1k";    // ข้อความสำหรับช่วงที่ 3
    case 4: return "1k-3k";     // ข้อความสำหรับช่วงที่ 4
    case 5: return "3k+";       // ข้อความสำหรับช่วงที่ 5
    default: return "UNK";      // กรณีไม่ทราบค่า (Unknown)
  }
}

// ==================== [MOVING_AVERAGE REMOVED] ====================

// ฟังก์ชันคำนวณ Type 1: Linear Regression
// รับค่าระยะดิบ (raw_cm) และดัชนีของ Anchor (0, 1, 2)
float getRegressionValue_Type1(float raw_cm, int anchor_idx) {
  int range = getDistanceRange(raw_cm); // ใช้ raw_cm หาช่วงระยะโดยตรง
  // สูตร: y = a + b * x (ยังคงใช้ raw_cm ในการคำนวณ)
  return LINEAR_A_RAW[range][anchor_idx] + LINEAR_B_RAW[range][anchor_idx] * raw_cm;
}

// ฟังก์ชันคำนวณ Type 2: Polynomial Regression
// รับค่าระยะดิบ (raw_cm) และดัชนีของ Anchor (0, 1, 2)
float getRegressionValue_Type2(float raw_cm, int anchor_idx) {
  int range = getDistanceRange(raw_cm); // ใช้ raw_cm หาช่วงระยะโดยตรง
  float p0 = POLY_P0_RAW[range][anchor_idx]; // ดึงสัมประสิทธิ์ p0
  float p1 = POLY_P1_RAW[range][anchor_idx]; // ดึงสัมประสิทธิ์ p1
  float p2 = POLY_P2_RAW[range][anchor_idx]; // ดึงสัมประสิทธิ์ p2
  // สูตร: y = p0 + p1*x + p2*x^2 (ยังคงใช้ raw_cm ในการคำนวณ)
  return p0 + p1 * raw_cm + p2 * raw_cm * raw_cm;
}



// ==================== SECTION 6: GLOBAL VARIABLES (ตัวแปรระดับโกลบอล) ====================
Adafruit_SSD1306 display(128, 64, &Wire, -1);  // ประกาศ Object ควบคุมจอ OLED ขนาด 128x64 (-1 คือไม่มีพิน Reset)
HardwareSerial SERIAL_AT(2);                    // ประกาศ Object UART2 สำหรับคุยกับโมดูล UWB

String  rxLine;               // ตัวแปร String สำหรับเก็บข้อมูลที่รับมาจาก UART ทีละบรรทัด
int     range_list[8] = {0};  // อาร์เรย์เก็บระยะทางดิบจากทุก Anchor (รองรับสูงสุด 8 ตัว)
float   rssi_list[8]  = {0};  // อาร์เรย์เก็บค่าความแรงสัญญาณ RSSI (ยังไม่ได้ถูกใช้งานหลักในโค้ดนี้)
uint32_t last_seq     = 0;    // ตัวแปรเก็บหมายเลขลำดับ (Sequence) ล่าสุดที่รับจาก UWB

bool     espnow_ready     = false;  // แฟล็กบอกสถานะว่า ESP-NOW พร้อมใช้งานหรือไม่
uint32_t tx_seq           = 0;      // ตัวนับลำดับแพ็กเก็ตที่ส่งออกไปทาง ESP-NOW

// ตัวแปรเก็บค่าระยะทางก่อนหน้า (Previous Value) เพื่อใช้ทำ Validation/Fallback
float prev_A1_cm = 0.0f;  
float prev_A2_cm = 0.0f;  
float prev_A3_cm = 0.0f;  

// ตัวแปรเก็บค่าระยะปัจจุบัน (Current Raw Value)
float raw_A1_cm = 0.0f, raw_A2_cm = 0.0f, raw_A3_cm = 0.0f;
// ตัวแปรเก็บค่าผลลัพธ์จากการคำนวณ Regression Type 1
float reg1_A1_cm = 0.0f, reg1_A2_cm = 0.0f, reg1_A3_cm = 0.0f;
// ตัวแปรเก็บค่าผลลัพธ์จากการคำนวณ Regression Type 2
float reg2_A1_cm = 0.0f, reg2_A2_cm = 0.0f, reg2_A3_cm = 0.0f;

// [WATCHDOG] ตัวแปรจับเวลาข้อมูลล่าสุด
uint32_t last_uwb_packet_time = 0;

// ==================== SECTION 7: ESP-NOW PACKET STRUCTURE (โครงสร้างข้อมูลแพ็กเก็ต) ====================
// กำหนดโครงสร้างข้อมูลที่จะส่งผ่าน ESP-NOW (ต้องตรงกันกับฝั่งรับ)
// __attribute__((packed)) บอกคอมไพเลอร์ว่าไม่ต้องทำ padding ข้อมูล (ให้เรียงไบต์ติดกันเลย)
typedef struct __attribute__((packed)) {
  uint8_t  role;      // บทบาทของอุปกรณ์ (0 = Tag)
  uint8_t  tag_id;    // หมายเลข ID ของ Tag
  uint8_t  reserved;  // ช่องว่างสำรอง (เพื่อให้ครบ Byte หรือเผื่ออนาคต)
  uint8_t  a_count;   // จำนวน Anchor ที่ส่งข้อมูลไป (ในที่นี้คือ 3)
  uint32_t seq;       // ลำดับแพ็กเก็ต (Sequence Number)
  uint32_t msec;      // เวลาของระบบ (Timestamp หน่วยมิลลิวินาที)
  
  // ข้อมูลระยะทางของแต่ละ Anchor (คูณ 1 เก็บเป็น int16 ได้ ถ้าต้องการทศนิยมอาจต้องคูณ 10 หรือ 100)
  // แต่ในที่นี้เก็บเป็น Integer เซนติเมตร
  int16_t  A1_raw, A2_raw, A3_raw;       // ระยะดิบ
  int16_t  A1_reg1, A2_reg1, A3_reg1;    // ระยะคำนวณ Type 1
  int16_t  A1_reg2, A2_reg2, A3_reg2;    // ระยะคำนวณ Type 2
} espnow_pkt_t;

// ==================== SECTION 8: UTILITY FUNCTIONS (ฟังก์ชันเบ็ดเตล็ด) ====================

// ฟังก์ชันอ่านค่า MAC Address ของตัวเครื่อง (Station Interface)
static void getStaMac(uint8_t mac[6]) {
  // ลองอ่านจาก API ของ ESP โดยตรง
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    bool z = true;
    for (int i = 0; i < 6; i++) if (mac[i]) { z = false; break; } // เช็คว่าเป็น 00:00:00:00:00:00 หรือไม่
    if (!z) return; // ถ้าไม่ใช่ 0 ล้วน ถือว่าอ่านสำเร็จ
  }
  // ถ้าอ่านไม่สำเร็จ ให้ลองใช้ WiFi.macAddress() แล้วแปลง String เป็น Array
  String s = WiFi.macAddress();
  if (s.length() >= 17) {
    int v[6];
    if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x",
               &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6) {
      for (int i = 0; i < 6; i++) mac[i] = (uint8_t)v[i];
      return;
    }
  }
  // ถ้าหาไม่ได้เลย ให้เซตเป็น 0
  memset(mac, 0, 6);
}

// ฟังก์ชันอ่านค่าช่องสัญญาณ Wi-Fi หลักที่ใช้งานอยู่
static uint8_t getPrimaryChannel() {
  uint8_t primary = 0;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&primary, &second); // เรียก API ระดับล่างเพื่อดึง Channel
  return primary;
}

// ฟังก์ชันส่ง AT Command ไปยังโมดูล UWB และรอรับค่าตอบกลับ
// input: cmd = คำสั่ง, timeout_ms = เวลารอสูงสุด, echo = แสดง debug หรือไม่
String sendAT(const String &cmd, uint32_t timeout_ms = 800, bool echo = false) {
  String resp;
  if (echo) Serial.println(cmd);  // ปริ้นท์คำสั่งออก Serial Monitor ถ้าต้องการดู
  SERIAL_AT.println(cmd);         // ส่งคำสั่งไป UART2 (UWB)
  uint32_t t0 = millis();
  // วนรอรับข้อมูลจนกว่าจะหมดเวลา Timeout
  while (millis() - t0 < timeout_ms) {
    while (SERIAL_AT.available()) resp += (char)SERIAL_AT.read(); // อ่านข้อมูลที่ตอบกลับมาเก็บใส่ resp
  }
  return resp; // คืนค่าข้อความที่ได้รับ
}

// ==================== SECTION 9: OLED FUNCTIONS (ฟังก์ชันแสดงผลหน้าจอ) ====================

// ฟังก์ชันแสดงข้อมูลระยะทางทั้งหมดบนหน้าจอ OLED
// รับพารามิเตอร์ 12 ตัว คือระยะของ A1, A2, A3 ในทั้ง 4 แบบ (Raw, Reg1, Reg2, Reg3)
void drawScreen(float rawA1, float rawA2, float rawA3,
                float r1A1, float r1A2, float r1A3,
                float r2A1, float r2A2, float r2A3) {
  display.clearDisplay();           // ล้างหน้าจอ
  display.setTextColor(SSD1306_WHITE); // ตั้งสีตัวอักษรเป็นสีขาว
  display.setTextSize(1);           // ขนาดตัวอักษรปกติ (เล็ก)

  // บรรทัดที่ 1: แสดง ID และ Range ของ A1
  display.setCursor(0, 0);
  int range = getDistanceRange(rawA1);
  display.printf("TAG-%d %s", TAG_ID, getRangeLabel(range));

  // บรรทัดที่ 2: ข้อมูล A1 (Raw, Reg1, Reg2, Reg3)
  display.setCursor(0, 10);
  // %.0f คือแสดงทศนิยม 0 ตำแหน่ง (จำนวนเต็ม) เพื่อประหยัดพื้นที่จอ
  display.printf("A1 R%.0f 1:%.0f 2:%.0f ", rawA1, r1A1, r2A1);

  // บรรทัดที่ 3: ข้อมูล A2
  display.setCursor(0, 20);
  display.printf("A2 R%.0f 1:%.0f 2:%.0f ", rawA2, r1A2, r2A2);

  // บรรทัดที่ 4: ข้อมูล A3
  display.setCursor(0, 30);
  display.printf("A3 R%.0f 1:%.0f 2:%.0f ", rawA3, r1A3, r2A3);

  // บรรทัดที่ 5: คำอธิบายย่อ (Legend) - บรรทัดบน
  display.setCursor(0, 42);
  display.print("R=Raw 1=Lin 2=Poly");

  // บรรทัดที่ 6: คำอธิบายย่อ (Legend) - บรรทัดล่าง   // ยกเลิกใช้
  //display.setCursor(0, 52);
  //display.print("3=BiasLin");

  display.display(); // สั่งให้จอแสดงผลตามบัฟเฟอร์ที่วาดไว้
}

// ฟังก์ชันแสดงหน้าจอเริ่มต้น (Boot Logo / Splash Screen)
void drawBootSplash() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("TAG SENDER");
  display.println(FW_VERSION);
  display.println("");
  display.println("Regression Compare");
  display.println("1=Lin 2=Poly");
  display.display();
}

// ==================== SECTION 10: PARSE FUNCTION (ฟังก์ชันแปลงข้อมูล) ====================

// ฟังก์ชันแยกแยะข้อมูลสตริงที่รับจาก UWB (Parsing)
// ตัวอย่างข้อมูล: "seq:123,range:(150,230,400)"
void parseRange(const String &line) {
  // 1. ดึงค่า Sequence Number
  int s1 = line.indexOf("seq:"); // หาตำแหน่งคำว่า "seq:"
  if (s1 >= 0) {
    int comma = line.indexOf(",", s1); // หาคอมม่าถัดไป
    // ตัดคำช่วงตัวเลขออกมาแปลงเป็น int
    last_seq = (uint32_t)line.substring(s1 + 4, (comma < 0) ? line.length() : comma).toInt();
  }

  // 2. เคลียร์อาร์เรย์เก็บค่าให้เป็น 0
  for (int i = 0; i < 8; i++) {
    range_list[i] = 0;
    rssi_list[i]  = 0;
  }

  // 3. ดึงค่า Range จากในวงเล็บ range:(...)
  int idx1 = line.indexOf("range:(");
  int idx2 = (idx1 >= 0) ? line.indexOf(")", idx1) : -1;
  if (idx1 >= 0 && idx2 > idx1) { // ถ้าเจอวงเล็บเปิดและปิดถูกต้อง
    String sr = line.substring(idx1 + 7, idx2); // ตัดเอาเฉพาะไส้ในวงเล็บ
    int last = 0;
    // วนลูปตัดด้วยคอมม่า เพื่อแยกค่าระยะแต่ละตัว
    for (int i = 0; i < 8 && last < (int)sr.length(); i++) {
      int c = sr.indexOf(',', last); // หาคอมม่าถัดไป
      range_list[i] = sr.substring(last, (c < 0) ? sr.length() : c).toInt(); // แปลงเป็น int เก็บลง array
      last = (c < 0) ? sr.length() : c + 1; // ขยับตำแหน่งค้นหา
    }
  }

  // 4. แปลงค่าลงตัวแปร float ของแต่ละ Anchor
  raw_A1_cm = (float)range_list[0];
  raw_A2_cm = (float)range_list[1];
  raw_A3_cm = (float)range_list[2];

  // 5. กรองค่าต่ำสุด (Minimum Clamping) เพื่อป้องกันค่าผิดพลาดที่เป็น 0 หรือลบ
  if (raw_A1_cm < 5.0f) raw_A1_cm = 5.0f;
  if (raw_A2_cm < 5.0f) raw_A2_cm = 5.0f;
  if (raw_A3_cm < 5.0f) raw_A3_cm = 5.0f;

  // 6. กรองค่าสูงสุด (Maximum Validation) ถ้ากระโดดผิดปกติให้ใช้ค่าเดิม (Fallback)
  if (raw_A1_cm > 10000.0f) raw_A1_cm = prev_A1_cm;
  if (raw_A2_cm > 10000.0f) raw_A2_cm = prev_A2_cm;
  if (raw_A3_cm > 10000.0f) raw_A3_cm = prev_A3_cm;

  // 7. บันทึกค่าปัจจุบันไว้เป็นค่าก่อนหน้า (สำหรับรอบถัดไป)
  prev_A1_cm = raw_A1_cm;
  prev_A2_cm = raw_A2_cm;
  prev_A3_cm = raw_A3_cm;
  
  // [WATCHDOG] อัปเดตเวลาล่าสุดที่ได้รับข้อมูลถูกต้อง
  last_uwb_packet_time = millis();

  // 8. คำนวณ Regression ทั้ง 2 แบบ (กลับไปใช้ raw_cm คำนวณ Range โดยตรง)
  
  // แบบที่ 1: Linear
  reg1_A1_cm = getRegressionValue_Type1(raw_A1_cm, 0);
  reg1_A2_cm = getRegressionValue_Type1(raw_A2_cm, 1);
  reg1_A3_cm = getRegressionValue_Type1(raw_A3_cm, 2);

  // แบบที่ 2: Polynomial
  reg2_A1_cm = getRegressionValue_Type2(raw_A1_cm, 0);
  reg2_A2_cm = getRegressionValue_Type2(raw_A2_cm, 1);
  reg2_A3_cm = getRegressionValue_Type2(raw_A3_cm, 2);

  // 9. แสดงผลออก Serial Monitor สำหรับ Debug
  Serial.printf(
    "[DATA] Raw: %.0f,%.0f,%.0f | T1: %.0f,%.0f,%.0f | T2: %.0f,%.0f,%.0f\n",
    raw_A1_cm, raw_A2_cm, raw_A3_cm,
    reg1_A1_cm, reg1_A2_cm, reg1_A3_cm,
    reg2_A1_cm, reg2_A2_cm, reg2_A3_cm
  );

  // 10. อัปเดตหน้าจอ OLED
  drawScreen(raw_A1_cm, raw_A2_cm, raw_A3_cm,
             reg1_A1_cm, reg1_A2_cm, reg1_A3_cm,
             reg2_A1_cm, reg2_A2_cm, reg2_A3_cm);
}

// ==================== SECTION 11: ESP-NOW CALLBACKS (จัดการอีเวนต์ ESP-NOW) ====================

// ฟังก์ชัน Callback ที่จะถูกเรียกเมื่อส่งข้อมูล ESP-NOW เสร็จสิ้น
void onEspNowSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("[TAG->NODE] ");
  if (mac_addr) {
    // แสดง MAC ของปลายทางที่ส่งไป
    for (int i = 0; i < 6; i++) Serial.printf("%02X%s", mac_addr[i], (i < 5) ? ":" : "");
  } else {
    Serial.print("NULL");
  }
  // แสดงสถานะว่าส่งผ่าน (OK) หรือล้มเหลว (FAIL)
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? " OK" : " FAIL");
}

// ฟังก์ชันสำหรับแพ็คข้อมูลและสั่งส่งผ่าน ESP-NOW
void sendNow_AllData() {
  if (!espnow_ready) return; // ถ้า ESP-NOW ยังไม่พร้อม ให้ยกเลิกการส่ง

  espnow_pkt_t p{}; // สร้างตัวแปร struct และเคลียร์ค่าเป็น 0
  p.role    = 0;        // ระบุว่าเป็น Tag
  p.tag_id  = TAG_ID;   // ใส่ ID
  p.a_count = 3;        // ใส่จำนวน Anchor
  p.seq     = ++tx_seq; // เพิ่มลำดับและใส่ค่า
  p.msec    = millis(); // ใส่เวลาปัจจุบัน

  // ใส่ข้อมูลระยะทางให้ครบทุกช่อง
  p.A1_raw = (int16_t)raw_A1_cm;
  p.A2_raw = (int16_t)raw_A2_cm;
  p.A3_raw = (int16_t)raw_A3_cm;

  p.A1_reg1 = (int16_t)reg1_A1_cm;
  p.A2_reg1 = (int16_t)reg1_A2_cm;
  p.A3_reg1 = (int16_t)reg1_A3_cm;

  p.A1_reg2 = (int16_t)reg2_A1_cm;
  p.A2_reg2 = (int16_t)reg2_A2_cm;
  p.A3_reg2 = (int16_t)reg2_A3_cm;

  // เรียกคำสั่งส่งข้อมูล
  esp_err_t err = esp_now_send(NODE_PEER_MAC, (uint8_t*)&p, sizeof(p));
  if (err != ESP_OK) { // ตรวจสอบว่าคำสั่งถูกเรียกสำเร็จหรือไม่ (ไม่ใช่ผลการส่งปลายทาง)
    Serial.printf("[TAG] esp_now_send err=%d\n", (int)err);
  }
}

// ==================== SECTION 12: SETUP (การเริ่มต้นระบบ) ====================
void setup() {
  // 1. เริ่มต้น Serial สำหรับ Debug
  Serial.begin(115200);
  // รอให้ Serial พร้อม (ไม่เกิน 2 วินาที)
  for (uint8_t i = 0; i < 40 && !Serial; i++) delay(50);
  delay(300);

  Serial.println();
  Serial.println("========================================");
  Serial.printf("[BOOT] TAG %s - REGRESSION COMPARE\n", FW_VERSION);
  Serial.println("========================================");

  // 2. ตั้งค่าขารีเซ็ต UWB และดึงสถานะเป็น HIGH (ทำงานปกติ)
  pinMode(UWB_RST, OUTPUT);
  digitalWrite(UWB_RST, HIGH);

  // 3. เริ่มต้น UART2 เชื่อมต่อกับ UWB
  SERIAL_AT.begin(115200, SERIAL_8N1, IO_RXD2, IO_TXD2);
  Serial.println("[SETUP] UART2 initialized");

  // 4. เริ่มต้น I2C และหน้าจอ OLED
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C มาตรฐาน OLED
    Serial.println("[ERROR] SSD1306 OLED init failed!");
    while (1) delay(1000); // ถ้าจอเสีย ให้หยุดทำงานตรงนี้
  }
  Serial.println("[SETUP] OLED initialized");
  display.clearDisplay();
  display.display();
  drawBootSplash(); // แสดงโลโก้
  delay(1500);      // หน่วงเวลาให้เห็นโลโก้

  // 5. ตั้งค่าโมดูล UWB ผ่าน AT Command
  Serial.println("[SETUP] Configuring UWB module...");
  sendAT("AT?");                // เช็คการเชื่อมต่อ
  sendAT("AT+RESTORE", 3000);   // รีเซ็ตค่าโรงงานเพื่อให้มั่นใจว่า Config สะอาด

  // สร้างคำสั่งตั้ง ID (AT+SETCFG)
  char cfg[40];
  sprintf(cfg, "AT+SETCFG=%d,%d,1,1", TAG_ID, 0); // Param: ID, Role(0=Tag), ...
  sendAT(cfg, 1500);
  Serial.printf("[SETUP] UWB Config: TAG_ID=%d, ROLE=0\n", TAG_ID);

  sendAT("AT+SETCAP=4,10,1", 1000); // ตั้งค่า Capacity (Max Anchor, TimeSlot, ...)
  sendAT("AT+SETRPT=1", 800);       // ตั้งค่า Reporting Rate
  sendAT("AT+SAVE", 500);           // บันทึกการตั้งค่าลงหน่วยความจำ
  sendAT("AT+RESTART", 2000);       // รีสตาร์ทโมดูลเพื่อให้ค่าใหม่ทำงาน
  Serial.println("[SETUP] UWB configured and restarted");

  // 6. เริ่มต้นระบบ Wi-Fi และ ESP-NOW
  Serial.println("[SETUP] Initializing ESP-NOW...");
  WiFi.mode(WIFI_STA); // ตั้งโหมดเป็น Station (Client) แต่ไม่เกาะ AP

  esp_wifi_set_ps(WIFI_PS_NONE); // ปิด Power Save เพื่อความไวสูงสุด
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE); // ล็อก Channel
  delay(50);

  // แสดง MAC Address ของตัวเอง
  uint8_t mac[6];
  getStaMac(mac);
  Serial.printf("[INFO] TAG MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("[INFO] Wi-Fi Channel: %u\n", (unsigned)getPrimaryChannel());

  // เริ่มต้น ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] ESP-NOW init failed!");
  } else {
    // ลงทะเบียน Callback ส่งข้อมูลเสร็จ
    esp_now_register_send_cb(onEspNowSent);

    // เพิ่ม Node ปลายทางเป็น Peer
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, NODE_PEER_MAC, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false; // ไม่เข้ารหัสข้อมูล

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
  Serial.printf("[READY] TAG %s\n", FW_VERSION);
  Serial.println("[READY] 1=Lin, 2=Poly");
  Serial.println("========================================");
}

// ==================== SECTION 13: MAIN LOOP (ลูปการทำงานหลัก) ====================
void loop() {
  // 1. [SMART POLLING] ไม่ส่ง AT+RDATA พร่ำเพรื่อ
  // จะส่งเฉพาะเมื่อข้อมูล "เงียบหายไป" เกิน 500ms เท่านั้น (เพื่อกระตุ้น)
  static uint32_t t_last_poll_kick = 0;
  if (millis() - last_uwb_packet_time > 500) { // ถ้าเงียบเกิน 500ms
    if (millis() - t_last_poll_kick > 500) {    // ห้ามส่งถี่เกินทุก 500ms
      sendAT("AT+RDATA", 0); // ยิงคำสั่งกระตุ้นเบาๆ
      t_last_poll_kick = millis();
      // Serial.println("[RECOVERY] Kick UWB (Soft request)");
    }
  }

  // 2. อ่านข้อมูลตอบกลับจาก UWB UART
  // 2. อ่านข้อมูลตอบกลับจาก UWB UART
  while (SERIAL_AT.available()) {
    char c = SERIAL_AT.read(); // อ่านทีละตัวอักษร
    if (c == '\n') { // ถ้าเจอขึ้นบรรทัดใหม่ แสดงว่าจบคำสั่ง
      if (rxLine.startsWith("AT+RANGE")) { // ถ้าขึ้นต้นด้วย AT+RANGE แสดงว่าเป็นข้อมูลระยะ
        parseRange(rxLine); // เรียกฟังก์ชัน Parse
      }
      rxLine = ""; // ล้าง Buffer บรรทัด
    } else if (c != '\r') {
      if (rxLine.length() < 200) { // [FIX] ป้องกัน Buffer Overflow
        rxLine += c; // เก็บสะสมตัวอักษรเข้า Buffer
      } else {
        // ถ้า Buffer ยาวเกินปกติ (ขยะ) ให้เคลียร์ทิ้งทันทีเพื่อเริ่มใหม่
        rxLine = ""; 
      }
    }
  }

  // 3. ส่งข้อมูลผ่าน ESP-NOW ทุกๆ 200ms (ถ้าพร้อม)
  static uint32_t t_espnow = 0;
  if (espnow_ready && millis() - t_espnow > 200) {
    sendNow_AllData(); // เรียกฟังก์ชันส่งข้อมูล
    t_espnow = millis();
  }

  // 4. พิมพ์ Heartbeat ทุก 5 วินาที
  static uint32_t t_heartbeat = 0;
  if (millis() - t_heartbeat > 5000) {
    Serial.printf("[HEARTBEAT] Uptime: %lu ms | Regression: ACTIVE\n",
                  (unsigned long)millis());
    t_heartbeat = millis();
  }
  
  // [WATCHDOG] ตรวจสอบว่า UWB เงียบไปนานเกิน 2 วินาทีหรือไม่ (ตามที่ขอ < 3s)
  if (millis() - last_uwb_packet_time > 2000 && last_uwb_packet_time > 0) {
    Serial.println("[WARNING] UWB Silent > 2s! Resetting Module...");
    // สั่ง Reset Hardware UWB
    digitalWrite(UWB_RST, LOW);  // กด Reset
    delay(50);                   // กดแช่สั้นลงเพื่อความไว
    digitalWrite(UWB_RST, HIGH); // ปล่อย Reset
    // delay(1000);              // [REMOVE] ไม่รอ User นาน ปล่อยให้ตื่นแล้ว Kick ต่อเองในรอบหน้า
    last_uwb_packet_time = millis(); // รีเซ็ตเวลา
  }

  // 5. Serial Passthrough: ส่งข้อมูลจาก Serial Monitor ไปเข้า UWB โดยตรง (สำหรับ Manual Debug)
  while (Serial.available()) {
    SERIAL_AT.write(Serial.read());
  }
}
