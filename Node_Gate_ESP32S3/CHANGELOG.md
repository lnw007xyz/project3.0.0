# Changelog - Node Gate ESP32S3

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.0] - 2026-01-16

### 🎉 Initial Release

#### ✅ Added
- **ESP-NOW Communication**
  - Receive data from Tag_sender v3.0.8 via ESP-NOW protocol
  - Support for Channel 11 (configurable)
  - MAC address filtering (broadcast or specific Tag)
  - Packet loss detection and monitoring

- **Arduino IoT Cloud Integration**
  - Bidirectional communication with Arduino IoT Cloud
  - Real-time data upload (200ms interval)
  - Cloud variable synchronization
  - WiFi connection management

- **Data Receiving & Processing**
  - Tag ID detection
  - Sequence number tracking
  - Distance measurements (Raw, Reg1, Reg2) from 3 Anchors
  - GPS coordinates (Latitude, Longitude)
  - Timestamp and age calculation

- **Cloud Variables (Uplink)**
  - `tagId` - Tag identifier
  - `sequenceNumber` - Packet sequence number
  - `dataAge` - Data freshness indicator
  - `rssi` - WiFi signal strength
  - `distanceA1Raw`, `distanceA2Raw`, `distanceA3Raw` - Raw distances
  - `distanceA1Reg1`, `distanceA2Reg1`, `distanceA3Reg1` - Regression Type 1
  - `distanceA1Reg2`, `distanceA2Reg2`, `distanceA3Reg2` - Regression Type 2
  - `position` - CloudLocation for map display
  - `latitude`, `longitude` - GPS coordinates
  - `totalPackets` - Total packets received
  - `packetsLost` - Packet loss counter

- **Cloud Variables (Downlink)**
  - `command` - Control commands (START, STOP, GOTO, RETURN, EMERGENCY_STOP)
  - `targetLat`, `targetLng` - Target position
  - `emergencyStop` - Emergency stop trigger

- **System Monitoring**
  - Connection status monitoring (ESP-NOW + IoT Cloud)
  - Data age watchdog (10-second timeout)
  - Packet statistics (total received, lost)
  - Serial debugging with color-coded messages

- **Documentation**
  - Comprehensive README.md
  - Detailed SETUP_GUIDE.md
  - Dashboard_Widgets.md for UI design
  - Code comments in Thai and English

#### 🔧 Technical Details
- **Platform:** ESP32-S3
- **Arduino Core:** v3.x (IDF 5.x compatible)
- **Communication Protocols:**
  - ESP-NOW for Tag communication
  - WiFi for Arduino IoT Cloud
- **Update Rates:**
  - ESP-NOW receive: Real-time (event-driven)
  - Cloud upload: 200ms
  - Status print: 5 seconds

#### 📦 Dependencies
- ArduinoIoTCloud library
- Arduino_ConnectionHandler library
- ESP32 Arduino Core v3.x+

#### 🎯 Key Features
1. **Low Latency:** ESP-NOW provides <5ms latency for Tag data
2. **Reliable:** Packet loss detection and monitoring
3. **Scalable:** Easy to add more Tags (just update MAC filter)
4. **Cloud-Ready:** Full Arduino IoT Cloud integration
5. **Real-time Dashboard:** Map view with live tracking

---

## [Unreleased]

### 🚀 Planned Features
- Multi-Tag support (receive from multiple Tags simultaneously)
- Local data logging (SD card or SPIFFS)
- MQTT support (alternative to Arduino IoT Cloud)
- Web server for local dashboard
- OTA (Over-The-Air) firmware updates
- Battery monitoring for portable deployment
- Geofencing and alert system
- Historical data visualization

### 🐛 Known Issues
- None reported

### 🔮 Future Enhancements
- Automatic WiFi reconnection with exponential backoff
- Compressed data transmission for bandwidth optimization
- Edge AI for anomaly detection
- WebSocket API for custom dashboards
- RESTful API endpoints
- Support for ESP32-C3, ESP32-C6

---

## Version Compatibility

| Node_Gate Version | Compatible Tag_sender Version | Arduino Core | IoT Cloud |
|-------------------|-------------------------------|--------------|-----------|
| 1.0.0             | v3.0.8, v3.0.7               | v3.x         | Latest    |

---

## Migration Guide

### From Nothing to v1.0.0
This is the initial release. Follow the SETUP_GUIDE.md for complete setup instructions.

---

## Breaking Changes

### v1.0.0
- Initial release, no breaking changes

---

## Notes

### ESP-NOW Channel
- Default: Channel 11
- Must match Tag_sender configuration
- Can be changed in code: `ESPNOW_CHANNEL`

### Arduino IoT Cloud
- Requires valid Device ID and Secret Key
- Free plan supports up to 5 Things
- Variables must be created manually on Cloud Dashboard

### Data Structures
- **Uplink:** Tag → Gateway → Cloud (espnow_pkt_t)
- **Downlink:** Cloud → Gateway → Tag (UWB_Command_Downlink)

---

## Contributors

- **Main Developer:** Project 3.0.0 Team
- **Email:** micmark1123456@gmail.com
- **GitHub:** https://github.com/lnw007xyz/project3.0.0

---

## License

This project is part of the Project 3.0.0 Indoor Positioning System.
All rights reserved.

---

**Last Updated:** 2026-01-16  
**Current Version:** v1.0.0  
**Next Planned Release:** v1.1.0 (Multi-Tag Support)
