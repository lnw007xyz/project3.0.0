# 📊 Arduino IoT Cloud Dashboard Widgets

## ภาพรวม Dashboard
ไฟล์นี้แสดงตัวอย่างการออกแบบ Dashboard บน Arduino IoT Cloud สำหรับระบบติดตาม Tag

---

## 🗺️ Layout แนะนำ

```
┌─────────────────────────────────────────────────────────────────┐
│                         TAG TRACKING SYSTEM                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────────────────────────┐  ┌──────────────────────┐ │
│  │                                   │  │   System Status      │ │
│  │                                   │  │  ┌────────────────┐  │ │
│  │        MAP WIDGET                │  │  │ Tag ID:      7 │  │ │
│  │   (Tag Real-time Position)       │  │  │ Seq:    12,345 │  │ │
│  │                                   │  │  │ RSSI:      -45 │  │ │
│  │         🔴 Tag-7                 │  │  │ Age:     120ms │  │ │
│  │                                   │  │  └────────────────┘  │ │
│  │                                   │  │                      │ │
│  └──────────────────────────────────┘  │  Packets             │ │
│                                         │  ┌────────────────┐  │ │
│                                         │  │ Total: 12,345  │  │ │
│  ┌──────────────────────────────────┐  │  │ Lost:       12 │  │ │
│  │      Distance Gauges              │  │  └────────────────┘  │ │
│  │  ┌──────┐  ┌──────┐  ┌──────┐   │  └──────────────────────┘ │
│  │  │  A1  │  │  A2  │  │  A3  │   │                            │
│  │  │ 245  │  │ 189  │  │ 312  │   │  ┌──────────────────────┐ │
│  │  │  cm  │  │  cm  │  │  cm  │   │  │   GPS Coordinates    │ │
│  │  └──────┘  └──────┘  └──────┘   │  │  ┌────────────────┐  │ │
│  └──────────────────────────────────┘  │  │ Lat: 13.75633  │  │ │
│                                         │  │ Lng: 100.5017  │  │ │
│  ┌──────────────────────────────────┐  │  └────────────────┘  │ │
│  │      Position History             │  └──────────────────────┘ │
│  │                                   │                            │
│  │  Latitude & Longitude Chart       │  ┌──────────────────────┐ │
│  │  ────────────────────────────────│  │   Control Panel      │ │
│  │  (Last 1 Hour)                    │  │  ┌────────────────┐  │ │
│  │                                   │  │  │ Command:       │  │ │
│  │                                   │  │  │ [START      ▼]│  │ │
│  │                                   │  │  └────────────────┘  │ │
│  └──────────────────────────────────┘  │  Target Position     │ │
│                                         │  Lat: [_________]    │ │
│                                         │  Lng: [_________]    │ │
│                                         │                      │ │
│                                         │  🚨 EMERGENCY STOP   │ │
│                                         │     [  OFF  ]        │ │
│                                         └──────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

---

## 📱 Widget Configurations

### 1. Map Widget - Tag Position
**Type:** Map  
**Variable:** `position` (CloudLocation)  
**Settings:**
- Title: "Tag Real-time Position"
- Zoom Level: 18 (Street level)
- Map Style: Satellite or Hybrid
- Marker Color: Red 🔴
- Show Coordinates: Yes

**Description:**  
แสดงตำแหน่งของ Tag แบบ Real-time บนแผนที่ โดย Marker จะเคลื่อนที่ตามตำแหน่งที่คำนวณได้จาก Trilateration

---

### 2. Value Widgets - System Status

#### 2.1 Tag ID
**Type:** Value  
**Variable:** `tagId` (int)  
**Settings:**
- Title: "Tag ID"
- Font Size: Large
- Color: Blue

#### 2.2 Sequence Number
**Type:** Value  
**Variable:** `sequenceNumber` (CloudInt)  
**Settings:**
- Title: "Sequence Number"
- Font Size: Medium
- Format: Number with comma separators

#### 2.3 RSSI
**Type:** Gauge  
**Variable:** `rssi` (int)  
**Settings:**
- Title: "WiFi Signal (RSSI)"
- Min: -100
- Max: 0
- Color Ranges:
  - -100 to -80: Red (Weak)
  - -80 to -50: Yellow (Fair)
  - -50 to 0: Green (Good)

#### 2.4 Data Age
**Type:** Value  
**Variable:** `dataAge` (CloudInt)  
**Settings:**
- Title: "Data Age (ms)"
- Font Size: Small
- Alert: Show warning if > 1000ms

---

### 3. Gauge Widgets - Distance Measurements

#### 3.1 Distance A1 (Anchor 1)
**Type:** Gauge  
**Variable:** `distanceA1Reg2` (CloudInt)  
**Settings:**
- Title: "Distance A1"
- Min: 0
- Max: 3000
- Unit: "cm"
- Color: Blue
- Threshold: 500 (Yellow), 1000 (Orange), 2000 (Red)

#### 3.2 Distance A2 (Anchor 2)
**Type:** Gauge  
**Variable:** `distanceA2Reg2` (CloudInt)  
**Settings:**
- Title: "Distance A2"
- Min: 0
- Max: 3000
- Unit: "cm"
- Color: Green
- Threshold: 500 (Yellow), 1000 (Orange), 2000 (Red)

#### 3.3 Distance A3 (Anchor 3)
**Type:** Gauge  
**Variable:** `distanceA3Reg2` (CloudInt)  
**Settings:**
- Title: "Distance A3"
- Min: 0
- Max: 3000
- Unit: "cm"
- Color: Purple
- Threshold: 500 (Yellow), 1000 (Orange), 2000 (Red)

---

### 4. Chart Widget - Position History

**Type:** Chart  
**Variables:** `latitude`, `longitude`  
**Settings:**
- Title: "Position History (Last 1 Hour)"
- Chart Type: Line Chart (2 series)
- Time Range: 1 Hour
- Y-Axis 1 (Left): Latitude (13.755 - 13.758)
- Y-Axis 2 (Right): Longitude (100.500 - 100.503)
- Colors:
  - Latitude: Blue
  - Longitude: Red
- Update Interval: On Change

**Description:**  
แสดงกราฟประวัติการเปลี่ยนแปลงของ Latitude และ Longitude ใน 1 ชั่วโมงที่ผ่านมา

---

### 5. Value Widgets - GPS Coordinates

#### 5.1 Latitude
**Type:** Value  
**Variable:** `latitude` (float)  
**Settings:**
- Title: "Latitude"
- Font Size: Medium
- Decimal Places: 6
- Color: Blue

#### 5.2 Longitude
**Type:** Value  
**Variable:** `longitude` (float)  
**Settings:**
- Title: "Longitude"
- Font Size: Medium
- Decimal Places: 6
- Color: Red

---

### 6. Value Widgets - Packet Statistics

#### 6.1 Total Packets
**Type:** Value  
**Variable:** `totalPackets` (CloudInt)  
**Settings:**
- Title: "Total Packets Received"
- Font Size: Large
- Format: Number with comma separators
- Color: Green

#### 6.2 Packets Lost
**Type:** Value  
**Variable:** `packetsLost` (CloudInt)  
**Settings:**
- Title: "Packets Lost"
- Font Size: Large
- Format: Number with comma separators
- Color: Red
- Alert: Show warning if > 10

---

### 7. Control Widgets

#### 7.1 Command Input
**Type:** Messenger (String Input)  
**Variable:** `command` (String)  
**Settings:**
- Title: "Send Command"
- Placeholder: "Enter command (START, STOP, GOTO, RETURN)"
- Button Text: "Send"

**Available Commands:**
- `START` - เริ่มการทำงาน
- `STOP` - หยุดการทำงาน
- `GOTO` - ไปยังจุดเป้าหมาย
- `RETURN` - กลับจุดเริ่มต้น
- `EMERGENCY_STOP` - หยุดฉุกเฉิน

#### 7.2 Target Latitude
**Type:** Slider or Value Input  
**Variable:** `targetLat` (float)  
**Settings:**
- Title: "Target Latitude"
- Min: 13.0
- Max: 14.0
- Step: 0.000001
- Decimal Places: 6

#### 7.3 Target Longitude
**Type:** Slider or Value Input  
**Variable:** `targetLng` (float)  
**Settings:**
- Title: "Target Longitude"
- Min: 100.0
- Max: 101.0
- Step: 0.000001
- Decimal Places: 6

#### 7.4 Emergency Stop
**Type:** Switch  
**Variable:** `emergencyStop` (bool)  
**Settings:**
- Title: "🚨 EMERGENCY STOP"
- Color: Red
- Font Size: X-Large
- ON Label: "STOPPED"
- OFF Label: "RUNNING"

---

## 🎨 Color Scheme

### Primary Colors
- **Blue:** #2196F3 - Tag ID, Latitude, A1
- **Green:** #4CAF50 - A2, Total Packets
- **Purple:** #9C27B0 - A3
- **Red:** #F44336 - Longitude, Packets Lost, Emergency Stop
- **Orange:** #FF9800 - Warnings
- **Yellow:** #FFEB3B - Alerts

### Background
- **Light Theme:** #FFFFFF
- **Dark Theme:** #1E1E1E

---

## 📏 Widget Sizes (Grid Units)

### Large Widgets
- Map: 6x4
- Position History Chart: 6x3

### Medium Widgets
- Distance Gauges (3x): 2x2 each
- System Status Panel: 2x4

### Small Widgets
- Value Displays: 1x1
- Control Inputs: 2x1

---

## 🔔 Notifications & Alerts

### Critical Alerts (Email + Push)
1. **Packets Lost > 50**
   - Message: "⚠️ High packet loss detected! Lost: {packetsLost}"
   - Action: Check ESP-NOW connection

2. **Data Age > 5000ms**
   - Message: "⚠️ Tag not responding for 5+ seconds!"
   - Action: Check Tag_sender status

3. **Emergency Stop Activated**
   - Message: "🚨 EMERGENCY STOP activated!"
   - Action: Immediate notification

### Warning Alerts (Dashboard only)
1. **RSSI < -80dBm**
   - Message: "Weak WiFi signal"
   
2. **Distance > 2000cm**
   - Message: "Tag far from anchors"

---

## 📱 Mobile View Optimization

### Priority Order (Top to Bottom)
1. Emergency Stop Button
2. Map Widget
3. System Status (Tag ID, Seq, RSSI)
4. Distance Gauges (A1, A2, A3)
5. GPS Coordinates
6. Control Panel
7. Packet Statistics
8. Position History Chart

---

## 🎯 Dashboard Templates

### Basic Template (Free Plan)
- Map Widget
- Distance Gauges (3x)
- System Status Values (5x)
- **Total: 9 widgets** (within Free plan limit)

### Advanced Template (Entry+ Plan)
- Map Widget
- Distance Gauges (3x)
- System Status Values (All)
- Position History Chart
- GPS Coordinates
- Control Panel (Full)
- Packet Statistics
- **Total: 20+ widgets**

---

## 💡 Tips & Best Practices

### Update Frequency
- **High Priority** (100-200ms): Map, Distance Gauges
- **Medium Priority** (500ms-1s): Values, Charts
- **Low Priority** (5s+): Statistics

### Data Retention
- **High Resolution:** Last 1 hour (every change)
- **Medium Resolution:** Last 24 hours (every 1 min)
- **Low Resolution:** Last 7 days (every 15 min)

### Mobile Optimization
- Use fewer widgets on mobile view
- Prioritize Map and Emergency Stop
- Hide statistics on small screens

---

## 🔗 References

- Arduino IoT Cloud Documentation: https://docs.arduino.cc/arduino-cloud/
- Widget Types: https://docs.arduino.cc/arduino-cloud/cloud-interface/dashboard-widgets
- CloudLocation: https://docs.arduino.cc/arduino-cloud/features/sharing-data/#cloudlocation

---

**Happy Dashboarding! 📊🚀**
