# ESP-NOW Callback Fix for IDF 5.5+

## 🐛 ปัญหา

เมื่อ compile โค้ด Tag_sender_v3_0_8.ino ด้วย ESP32 Arduino Core v3.x (IDF 5.5+) จะเกิด error:

```
error: invalid conversion from 'void (*)(const uint8_t*, esp_now_send_status_t)' 
to 'esp_now_send_cb_t' {aka 'void (*)(const wifi_tx_info_t*, esp_now_send_status_t)'}
```

## 🔍 สาเหตุ

ESP32 Arduino Core v3.x (IDF 5.5+) มีการเปลี่ยนแปลง **Breaking Changes** ใน ESP-NOW API:

### เปลี่ยนแปลงที่ 1: Send Callback
**เดิม (IDF < 5.5):**
```cpp
void onEspNowSent(const uint8_t *mac_addr, esp_now_send_status_t status)
```

**ใหม่ (IDF 5.5+):**
```cpp
void onEspNowSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
```

### เปลี่ยนแปลงที่ 2: Receive Callback
**เดิม (IDF < 5.5):**
```cpp
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
```

**ใหม่ (IDF 5.5+):**
```cpp
void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *data, int data_len)
```

## ✅ การแก้ไข

### โค้ดที่แก้ไขแล้ว (lines 309-341)

```cpp
// ==================== ESP-NOW CALLBACKS ====================

// Callback เมื่อได้รับข้อมูล (รองรับทั้ง Anchor GPS และ Node Command)
// Updated for ESP32 Arduino Core v3.x (IDF 5.5+)
void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *data, int data_len) {
  // กรณี 1: รับ GPS จาก Anchor
  if (data_len == sizeof(anchor_gps_t)) {
    anchor_gps_t recv_anchor;
    memcpy(&recv_anchor, data, sizeof(recv_anchor));
    
    if (recv_anchor.id >= 1 && recv_anchor.id <= 3) {
      anchor_positions[recv_anchor.id - 1] = recv_anchor;
      anchor_gps_ready[recv_anchor.id - 1] = true;
    }
  }
  // กรณี 2: รับคำสั่งจาก Node
  else if (data_len == sizeof(UWB_Command_Downlink)) {
    memcpy(&receivedCommand, data, sizeof(receivedCommand));
    Serial.printf("[CMD] %s X:%.1f Y:%.1f STOP:%d\n", 
                  receivedCommand.command, receivedCommand.target_x, 
                  receivedCommand.target_y, receivedCommand.emergency_stop);
    
    if (receivedCommand.emergency_stop) {
      Serial.println("!!! EMERGENCY STOP !!!");
    }
  }
}

// Updated for ESP32 Arduino Core v3.x (IDF 5.5+)
void onEspNowSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  // Serial.print(status == ESP_NOW_SEND_SUCCESS ? "." : "x");
}
```

## 📚 ข้อมูลเพิ่มเติม

### โครงสร้าง `wifi_tx_info_t`
```cpp
typedef struct {
    uint8_t dst_addr[6];  // MAC address ของปลายทาง
    // ... other fields
} wifi_tx_info_t;
```

### โครงสร้าง `esp_now_recv_info`
```cpp
typedef struct {
    uint8_t *src_addr;    // MAC address ของผู้ส่ง
    uint8_t *des_addr;    // MAC address ของผู้รับ
    // ... other fields
} esp_now_recv_info_t;
```

## 🎯 การใช้ MAC Address (ถ้าต้องการ)

ถ้าต้องการใช้ MAC address จาก callback ใหม่:

### Send Callback:
```cpp
void onEspNowSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  // เข้าถึง MAC address ของปลายทาง
  Serial.printf("Sent to: %02X:%02X:%02X:%02X:%02X:%02X\n",
                info->dst_addr[0], info->dst_addr[1], info->dst_addr[2],
                info->dst_addr[3], info->dst_addr[4], info->dst_addr[5]);
}
```

### Receive Callback:
```cpp
void OnDataRecv(const esp_now_recv_info *recv_info, const uint8_t *data, int data_len) {
  // เข้าถึง MAC address ของผู้ส่ง
  Serial.printf("Received from: %02X:%02X:%02X:%02X:%02X:%02X\n",
                recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
                recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
}
```

## ✅ สรุป

| การเปลี่ยนแปลง | เดิม | ใหม่ |
|----------------|------|------|
| Send Callback Parameter | `const uint8_t *mac_addr` | `const wifi_tx_info_t *info` |
| Recv Callback Parameter | `const uint8_t *mac_addr` | `const esp_now_recv_info *recv_info` |
| MAC Address Access | โดยตรง | ผ่าน struct member |

## 🔗 Reference

- [ESP-IDF ESP-NOW Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html)
- ESP32 Arduino Core v3.x Migration Guide

---

**อัพเดท**: 2026-01-16  
**สถานะ**: ✅ แก้ไขเรียบร้อย  
**เวอร์ชัน**: v3.0.8
