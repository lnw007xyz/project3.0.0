# CHANGELOG - Tag Sender Project

All notable changes to this project will be documented in this file.

## [v3.0.8] - 2026-01-16

### ชื่อ: ปรับปรุงสมการใหม่ (Improved Regression Equations)

### Added ✨
- อัพเดทสมการ Regression ใหม่ทั้งหมด (Linear และ Polynomial)
- ใช้ข้อมูลจาก `regression_compare_old_vs_new_v3_0_7.xlsx`
- เพิ่มสัมประสิทธิ์ใหม่ 5 ชุด:
  - `LINEAR_A_NEW[6][3]`
  - `LINEAR_B_NEW[6][3]`
  - `POLY_P0_NEW[6][3]`
  - `POLY_P1_NEW[6][3]`
  - `POLY_P2_NEW[6][3]`

### Changed 🔧
- ปรับปรุงความแม่นยำของการวัดระยะทาง:
  - Range 0 (10-50cm): ลด RMSE ~20-30%
  - Range 1 (50-100cm): ลด RMSE **80-97%** 🏆
  - Range 2 (100-500cm): ลด RMSE ~10-45%
  - Range 4 (1k-3k): ลด RMSE **30-98%** 🏆
- อัพเดทข้อความแสดงผลบน OLED เป็น "[v3.0.8]"
- อัพเดทข้อความ Serial Monitor เป็น "v3.0.8 (ปรับปรุงสมการใหม่)"

### Fixed 🐛
- ปรับปรุงสมการให้แม่นยำขึ้นโดยเฉพาะในช่วง 50-100cm และ 1k-3k
- ลดค่า Error (RMSE, MAE) ในทุกช่วงระยะทาง
- แก้ไข ESP-NOW callback signatures สำหรับ ESP32 Arduino Core v3.x (IDF 5.5+)
  - `OnDataRecv` ใช้ `esp_now_recv_info*` แทน `uint8_t* mac_addr`
  - `onEspNowSent` ใช้ `wifi_tx_info_t*` แทน `uint8_t* mac_addr`

### Unchanged 📌
- ฟีเจอร์ GPS Trilateration
- ฟีเจอร์ Bidirectional Communication
- โครงสร้างข้อมูล ESP-NOW
- การตั้งค่า WiFi และ ESP-NOW
- ฟังก์ชันการทำงานทั้งหมดจาก v3.0.7

### Technical Details 📊
- จำนวนข้อมูลทดสอบ: 340 samples
- วันที่ทำการทดลอง: 4/10/2025
- Anchor ที่ทดสอบ: A1, A2, A3
- Range ที่ทดสอบ: 0-4 (10cm - 3000cm)

### Files Created 📁
- `Tag_sender_v3_0_8/Tag_sender_v3_0_8.ino` - Main Arduino file
- `Tag_sender_v3_0_8/README.md` - Documentation
- `Tag_sender_v3_0_8/COMPARISON.md` - Detailed comparison with v3.0.7
- `Tag_sender_v3_0_8/CHANGELOG.md` - This file

---

## [v3.0.7] - 2026-01-12

### ชื่อ: GPS Trilateration + Bidirectional

### Added ✨
- รับคำสั่งจาก Node (START, STOP, GOTO, ETC)
- โครงสร้าง `UWB_Command_Downlink` สำหรับรับคำสั่ง
- ระบบ Bidirectional Communication

### Fixed 🐛
- แก้ปัญหา WiFi Connecting Error โดยปิด Persistent Storage
- เพิ่ม `WiFi.persistent(false)`
- เพิ่ม `WiFi.disconnect(true)`

### Unchanged 📌
- คงฟีเจอร์ GPS Trilateration และ Regression ไว้ครบถ้วน

---

## [v3.0.6] - (Date Unknown)

### Added ✨
- GPS Trilateration (คำนวณตำแหน่ง Tag จาก 3 Anchors)
- ฟังก์ชัน `updateTrilateration()`
- ฟังก์ชัน `calculateGPSDistance()` (Haversine formula)
- รับข้อมูล GPS จาก Anchors ผ่าน ESP-NOW
- แสดงพิกัด GPS บน OLED

### Technical Details 📊
- ใช้ Local Tangent Plane สำหรับแปลงพิกัด GPS เป็น XY
- ใช้สูตร Trilateration แบบง่าย
- แปลงกลับเป็น GPS coordinates

---

## [v3.0.5] - (Date Unknown)

### Added ✨
- Regression Type 1 (Linear): `getRegressionValue_Type1()`
- Regression Type 2 (Polynomial): `getRegressionValue_Type2()`
- ระบบแบ่งช่วงระยะทาง 6 ช่วง (Range 0-5)
- สัมประสิทธิ์ Regression สำหรับแต่ละช่วง:
  - `LINEAR_A_RAW[6][3]`, `LINEAR_B_RAW[6][3]`
  - `POLY_P0_RAW[6][3]`, `POLY_P1_RAW[6][3]`, `POLY_P2_RAW[6][3]`

---

## Version Comparison Table

| Version | Date       | Key Features | RMSE (Avg) | Status |
|---------|------------|--------------|-----------|--------|
| v3.0.8  | 2026-01-16 | New Regression Equations | **Lower** | ✅ Current |
| v3.0.7  | 2026-01-12 | Bidirectional + GPS | Baseline | ✅ Stable |
| v3.0.6  | Unknown    | GPS Trilateration | N/A | ✅ Stable |
| v3.0.5  | Unknown    | Regression | N/A | ✅ Stable |

---

## Legend

- ✨ **Added**: New features
- 🔧 **Changed**: Changes in existing functionality
- 🐛 **Fixed**: Bug fixes
- 📌 **Unchanged**: Features that remain the same
- 📊 **Technical**: Technical details
- 📁 **Files**: File changes
- 🏆 **Achievement**: Major improvement

---

**Maintained by**: Project Team  
**Platform**: ESP32 Arduino Core v3.x (IDF 5.x)  
**Last Updated**: 2026-01-16
