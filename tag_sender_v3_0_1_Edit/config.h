/*
 * =====================================================
 * CONFIG.H - Configuration & Pin Definitions
 * =====================================================
 * ไฟล์นี้รวบรวมค่า Configuration ทั้งหมดของระบบ
 * รวมถึง Pin Assignment และ Network Settings
 * =====================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

// ==================== TAG Configuration ====================
// TAG_ID: หมายเลขประจำตัว Tag (0-255)
// ใช้ระบุ Tag นี้ในระบบ UWB และ ESP-NOW
#define TAG_ID           0

// ==================== I2C Pin Configuration ====================
// I2C_SDA: พิน Serial Data สำหรับ OLED Display
// I2C_SCL: พิน Serial Clock สำหรับ OLED Display
#define I2C_SDA          39
#define I2C_SCL          38

// ==================== UART Pin Configuration ====================
// IO_RXD2: พิน RX สำหรับสื่อสารกับ UWB Module (UART2)
// IO_TXD2: พิน TX สำหรับสื่อสารกับ UWB Module (UART2)
#define IO_RXD2          18
#define IO_TXD2          17

// ==================== UWB Module Pin ====================
// UWB_RST: พินควบคุม Reset ของ UWB Module
#define UWB_RST          16

// ==================== Firmware Version ====================
// FW_VERSION: เวอร์ชันปัจจุบันของ Firmware
static const char* FW_VERSION = "v3.0.1";

// ==================== ESP-NOW Configuration ====================
// NODE_PEER_MAC: MAC Address ของ Node ที่จะส่งข้อมูลไป
// ESPNOW_CHANNEL: ช่องสัญญาณ Wi-Fi สำหรับ ESP-NOW (1-14)
uint8_t NODE_PEER_MAC[6] = { 0x30, 0xED, 0xA0, 0x1F, 0x00, 0x24 };
static const uint8_t ESPNOW_CHANNEL = 11;

#endif // CONFIG_H
