# 🚀 คู่มือการตั้งค่า Node Gate ESP32S3

## 📌 ภาพรวม
คู่มือนี้จะพาคุณตั้งค่า Node Gate ESP32S3 ตั้งแต่ต้นจนสามารถใช้งานได้

---

## 🔧 ขั้นตอนที่ 1: ติดตั้ง Arduino IDE และ Library

### 1.1 ติดตั้ง Arduino IDE
1. ดาวน์โหลด Arduino IDE 2.x จาก: https://www.arduino.cc/en/software
2. ติดตั้งและเปิดโปรแกรม

### 1.2 ติดตั้ง ESP32 Board Support
1. เปิด Arduino IDE
2. ไปที่ **File > Preferences**
3. ในช่อง **Additional Board Manager URLs** ใส่:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. คลิก **OK**
5. ไปที่ **Tools > Board > Boards Manager**
6. ค้นหา **esp32**
7. ติดตั้ง **esp32 by Espressif Systems** (เวอร์ชัน 3.x หรือใหม่กว่า)

### 1.3 ติดตั้ง Arduino IoT Cloud Libraries
1. ไปที่ **Tools > Manage Libraries**
2. ค้นหาและติดตั้ง:
   - **ArduinoIoTCloud** (โดย Arduino)
   - **Arduino_ConnectionHandler** (โดย Arduino)

---

## ☁️ ขั้นตอนที่ 2: ตั้งค่า Arduino IoT Cloud

### 2.1 สร้าง Account
1. ไปที่: https://create.arduino.cc/iot/
2. สร้างบัญชี (ถ้ายังไม่มี) หรือ Login
3. เลือก **Free Plan** (รองรับ 5 Things)

### 2.2 สร้าง Thing
1. คลิก **CREATE THING**
2. ตั้งชื่อ Thing: `Tag_Tracking_System`
3. คลิก **ADD DEVICE**
4. เลือก **ESP32**
5. เลือก **ESP32S3 Dev Module**
6. ตั้งชื่อ Device: `Node_Gate_ESP32S3`
7. **บันทึก Device ID และ Secret Key** (จะใช้ในขั้นตอนถัดไป)

### 2.3 เพิ่มตัวแปร (Variables)

#### Uplink Variables (Read Only - ข้อมูลจาก Tag)

| ชื่อตัวแปร | ประเภท | Permission | Update Policy |
|-----------|--------|-----------|---------------|
| `tagId` | int | Read Only | On Change |
| `sequenceNumber` | CloudInt | Read Only | On Change |
| `dataAge` | CloudInt | Read Only | On Change |
| `rssi` | int | Read Only | On Change |
| `distanceA1Raw` | CloudInt | Read Only | On Change |
| `distanceA2Raw` | CloudInt | Read Only | On Change |
| `distanceA3Raw` | CloudInt | Read Only | On Change |
| `distanceA1Reg1` | CloudInt | Read Only | On Change |
| `distanceA2Reg1` | CloudInt | Read Only | On Change |
| `distanceA3Reg1` | CloudInt | Read Only | On Change |
| `distanceA1Reg2` | CloudInt | Read Only | On Change |
| `distanceA2Reg2` | CloudInt | Read Only | On Change |
| `distanceA3Reg2` | CloudInt | Read Only | On Change |
| `position` | CloudLocation | Read Only | On Change |
| `latitude` | float | Read Only | On Change |
| `longitude` | float | Read Only | On Change |
| `totalPackets` | CloudInt | Read Only | On Change |
| `packetsLost` | CloudInt | Read Only | On Change |

#### Downlink Variables (Read & Write - คำสั่งควบคุม)

| ชื่อตัวแปร | ประเภท | Permission | Update Policy |
|-----------|--------|-----------|---------------|
| `command` | String | Read & Write | On Change |
| `targetLat` | float | Read & Write | On Change |
| `targetLng` | float | Read & Write | On Change |
| `emergencyStop` | bool | Read & Write | On Change |

**วิธีเพิ่มตัวแปร:**
1. คลิก **ADD VARIABLE**
2. ใส่ชื่อตัวแปรตามตาราง
3. เลือกประเภทตัวแปร (Type)
4. เลือก Permission:
   - **Read Only** สำหรับข้อมูลที่ส่งจาก ESP32 ขึ้น Cloud
   - **Read & Write** สำหรับข้อมูลที่ควบคุมจาก Cloud
5. เลือก Update Policy: **On Change**
6. คลิก **ADD VARIABLE**

### 2.4 ตั้งค่า Network
1. ไปที่แท็บ **Network**
2. คลิก **CONFIGURE**
3. ใส่ **WiFi SSID** และ **Password**
4. คลิก **SAVE**

---

## 💻 ขั้นตอนที่ 3: แก้ไขโค้ด

### 3.1 แก้ไขไฟล์ `thingProperties.h`

เปิดไฟล์ `thingProperties.h` และแก้ไขดังนี้:

```cpp
// ==================== WiFi CREDENTIALS ====================
const char WIFI_SSID[]     = "ชื่อ_WiFi_ของคุณ";          // ⬅️ แก้ไขที่นี่
const char WIFI_PASSWORD[] = "รหัส_WiFi_ของคุณ";         // ⬅️ แก้ไขที่นี่

// ==================== ARDUINO IOT CLOUD CREDENTIALS ====================
const char DEVICE_LOGIN_NAME[] = "Device_ID_จาก_Cloud";  // ⬅️ แก้ไขที่นี่
const char DEVICE_KEY[]        = "Secret_Key_จาก_Cloud"; // ⬅️ แก้ไขที่นี่
```

**ตัวอย่าง:**
```cpp
const char WIFI_SSID[]     = "MyHome_WiFi";
const char WIFI_PASSWORD[] = "password12345";

const char DEVICE_LOGIN_NAME[] = "a1b2c3d4-e5f6-7890-abcd-ef1234567890";
const char DEVICE_KEY[]        = "ABCDEFGHIJKLMNOP";
```

### 3.2 (ถ้าต้องการ) กำหนด MAC Address ของ Tag

เปิดไฟล์ `Node_Gate_ESP32S3.ino` และแก้ไข:

```cpp
// กรณีที่ 1: รับจาก Tag ทุกตัว (Broadcast)
uint8_t TAG_SENDER_MAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// กรณีที่ 2: รับจาก Tag เฉพาะที่ระบุ MAC Address
// ⚠️ ให้ดู MAC Address จาก Tag_sender Serial Monitor
uint8_t TAG_SENDER_MAC[6] = { 0x30, 0xED, 0xA0, 0x1F, 0x00, 0x24 };
```

---

## 📤 ขั้นตอนที่ 4: อัปโหลดโค้ดไปยัง ESP32-S3

### 4.1 เชื่อมต่อ ESP32-S3
1. เสียบสาย USB เชื่อมต่อ ESP32-S3 กับคอมพิวเตอร์
2. รอให้ระบบติดตั้ง Driver (Windows จะติดตั้งอัตโนมัติ)

### 4.2 ตั้งค่า Board และ Port
1. เปิด Arduino IDE
2. ไปที่ **Tools > Board > esp32 > ESP32S3 Dev Module**
3. ไปที่ **Tools > Port** เลือก COM Port ที่ ESP32-S3 เชื่อมต่ออยู่

### 4.3 ตั้งค่า Upload Settings (สำคัญ!)
ไปที่ **Tools** และตั้งค่าดังนี้:

| Setting | ค่าที่แนะนำ |
|---------|------------|
| USB CDC On Boot | Enabled |
| CPU Frequency | 240MHz (WiFi) |
| Flash Mode | QIO |
| Flash Size | 4MB (หรือตามที่บอร์ดรองรับ) |
| Partition Scheme | Default 4MB with spiffs |
| PSRAM | QSPI PSRAM (ถ้ามี) |
| Upload Speed | 921600 |

### 4.4 Compile และ Upload
1. คลิก **Verify** (✓) เพื่อ Compile โค้ด
2. ถ้าไม่มี Error คลิก **Upload** (→)
3. รอจนกว่าจะขึ้นข้อความ **Done uploading**

### 4.5 เปิด Serial Monitor
1. คลิก **Serial Monitor** (ขวาบน)
2. ตั้งค่า Baud Rate: **115200**
3. ควรเห็นข้อความ:
   ```
   === NODE GATE ESP32S3 v1.0.0 ===
   Initializing...
   [INFO] Gateway MAC Address: AA:BB:CC:DD:EE:FF
   [✓] ESP-NOW Initialized
   ✓ Initialization Complete
   Waiting for data from Tag_sender...
   ```

---

## 🔗 ขั้นตอนที่ 5: เชื่อมต่อ Tag_sender

### 5.1 อัปเดท MAC Address ใน Tag_sender
1. เปิดโค้ด `Tag_sender_v3_0_8.ino`
2. ดู MAC Address ของ Node_Gate จาก Serial Monitor (ขั้นตอน 4.5)
3. แก้ไขใน Tag_sender:
   ```cpp
   // ใส่ MAC Address ของ Node_Gate ที่นี่
   uint8_t NODE_PEER_MAC[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
   ```
4. Upload โค้ดใหม่ไปยัง Tag_sender

### 5.2 ทดสอบการรับส่งข้อมูล
1. เปิด Serial Monitor ของ Node_Gate
2. เปิด Serial Monitor ของ Tag_sender
3. ตรวจสอบว่า Node_Gate ได้รับข้อมูล:
   ```
   [ESP-NOW] Received from Tag-7 | Seq: 123 | GPS: 13.756331, 100.501762
   [Cloud] Updated - Tag: 7, Pos: 13.756331, 100.501762
   ```

---

## 📊 ขั้นตอนที่ 6: สร้าง Dashboard

### 6.1 สร้าง Dashboard ใหม่
1. ไปที่: https://create.arduino.cc/iot/dashboards
2. คลิก **CREATE DASHBOARD**
3. ตั้งชื่อ: `Tag Tracking Monitor`

### 6.2 เพิ่ม Widgets

#### แผนที่แสดงตำแหน่ง (Map Widget)
1. คลิก **ADD** > **Map**
2. ผูกกับตัวแปร: `position`
3. ตั้งชื่อ: "Tag Position"
4. ปรับขนาด Widget ตามต้องการ

#### แสดงระยะทาง (Gauge Widget x 3)
1. คลิก **ADD** > **Gauge**
2. ผูกกับ: `distanceA1Reg2`
3. ตั้งชื่อ: "Distance A1"
4. ตั้งค่า Min: 0, Max: 3000
5. ทำซ้ำสำหรับ `distanceA2Reg2` และ `distanceA3Reg2`

#### สถานะระบบ (Value Widget)
1. คลิก **ADD** > **Value**
2. ผูกกับ: `sequenceNumber`
3. ตั้งชื่อ: "Sequence #"
4. ทำซ้ำสำหรับ `totalPackets`, `packetsLost`, `dataAge`, `rssi`

#### กราฟแสดงประวัติ (Chart Widget)
1. คลิก **ADD** > **Chart**
2. ผูกกับ: `latitude`, `longitude`
3. ตั้งชื่อ: "Position History"
4. ตั้งค่า Time Range: 1 Hour

#### ควบคุมคำสั่ง (Input Widget)
1. คลิก **ADD** > **Messenger**
2. ผูกกับ: `command`
3. ตั้งชื่อ: "Send Command"

#### ปุ่มหยุดฉุกเฉิน (Switch Widget)
1. คลิก **ADD** > **Switch**
2. ผูกกับ: `emergencyStop`
3. ตั้งชื่อ: "🚨 EMERGENCY STOP"
4. เปลี่ยนสีเป็นสีแดง

---

## ✅ ขั้นตอนที่ 7: ทดสอบระบบ

### 7.1 ตรวจสอบการเชื่อมต่อ
1. ดู Serial Monitor ของ Node_Gate:
   ```
   IoT Cloud: Connected
   ```
2. ดู Dashboard บน Arduino IoT Cloud ควรเห็นข้อมูลอัปเดทแบบ Real-time

### 7.2 ทดสอบการควบคุม
1. ใน Dashboard พิมพ์คำสั่ง: `START` ในช่อง **Send Command**
2. ดู Serial Monitor ของ Node_Gate ควรเห็น:
   ```
   [Cloud] Command changed to: START
   ```
3. ลองกดปุ่ม **EMERGENCY STOP** และดูว่า Tag_sender ตอบสนองหรือไม่

### 7.3 ตรวจสอบตำแหน่งบนแผนที่
1. ดูบน Map Widget ควรเห็นตำแหน่งของ Tag อัปเดทแบบ Real-time
2. ตรวจสอบว่าค่า Latitude และ Longitude ถูกต้อง

---

## 🐛 Troubleshooting

### ปัญหา: Compile Error
❌ **Error:** `ArduinoIoTCloud.h: No such file or directory`
✅ **แก้ไข:** ติดตั้ง Library **ArduinoIoTCloud** และ **Arduino_ConnectionHandler**

### ปัญหา: Upload Failed
❌ **Error:** `A fatal error occurred: Failed to connect to ESP32-S3`
✅ **แก้ไข:** 
1. กดปุ่ม **BOOT** ค้างไว้บนบอร์ด ESP32-S3
2. คลิก **Upload** ใน Arduino IDE
3. ปล่อยปุ่ม **BOOT** เมื่อเริ่ม Upload

### ปัญหา: WiFi ไม่เชื่อมต่อ
❌ **Serial:** `WiFi connection failed`
✅ **แก้ไข:**
1. ตรวจสอบ SSID และ Password ใน `thingProperties.h`
2. ตรวจสอบว่า WiFi เป็น 2.4GHz (ESP32 ไม่รองรับ 5GHz)

### ปัญหา: Arduino IoT Cloud ไม่เชื่อมต่อ
❌ **Serial:** `Connection to Arduino IoT Cloud failed`
✅ **แก้ไข:**
1. ตรวจสอบ Device ID และ Secret Key
2. ตรวจสอบว่า Thing บน Cloud มีตัวแปรครบถ้วน
3. ลอง Reset ESP32-S3 และเชื่อมต่อใหม่

### ปัญหา: ไม่ได้รับข้อมูลจาก Tag
❌ **Serial:** ไม่มีข้อความ `[ESP-NOW] Received`
✅ **แก้ไข:**
1. ตรวจสอบว่า Tag_sender ใส่ MAC Address ของ Node_Gate ถูกต้อง
2. ตรวจสอบว่า ESP-NOW Channel ตรงกัน (default = 11)
3. ลดระยะห่างระหว่าง Tag และ Gateway

---

## 📞 ติดต่อและช่วยเหลือ

หากพบปัญหาหรือต้องการความช่วยเหลือ:
- **Email:** micmark1123456@gmail.com
- **GitHub Issues:** https://github.com/lnw007xyz/project3.0.0/issues

---

## 🎉 สำเร็จ!

ตอนนี้ระบบของคุณพร้อมใช้งานแล้ว! คุณสามารถ:
- ✅ ติดตามตำแหน่ง Tag แบบ Real-time
- ✅ ดูข้อมูลระยะทางจาก Anchors
- ✅ ควบคุม Tag จาก Cloud
- ✅ เก็บประวัติข้อมูลสำหรับวิเคราะห์

**Happy Tracking! 🚀**
