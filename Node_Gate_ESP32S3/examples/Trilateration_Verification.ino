/*
 * =====================================================
 * EXAMPLE: Trilateration Verification at Gateway
 * =====================================================
 * ตัวอย่างการคำนวณพิกัด GPS ของ Tag ใน Node_Gate
 * เพื่อ Verify กับค่าที่ Tag ส่งมา
 * 
 * วิธีใช้งาน:
 * 1. Copy ฟังก์ชันด้านล่างไปใส่ใน Node_Gate_ESP32S3.ino
 * 2. เรียกใช้ใน updateCloudVariables()
 * =====================================================
 */

// โครงสร้างพิกัด Anchor (ต้องมีใน Node_Gate)
typedef struct {
  float lat;
  float lng;
  bool valid;
} AnchorGPS;

// พิกัด Anchor ทั้ง 3 (ต้องตั้งค่าจริง)
AnchorGPS anchors[3] = {
  {13.756331, 100.501762, true},  // Anchor 1
  {13.756450, 100.502100, true},  // Anchor 2
  {13.756200, 100.502300, true}   // Anchor 3
};

// ==================== HELPER FUNCTIONS ====================

float toRad(float degree) { return degree * PI / 180.0; }
float toDeg(float rad) { return rad * 180.0 / PI; }

// คำนวณระยะทางระหว่าง 2 พิกัด (Haversine)
float calculateGPSDistance(float lat1, float lng1, float lat2, float lng2) {
  float R = 6371000.0; // รัศมีโลก (เมตร)
  float dLat = toRad(lat2 - lat1);
  float dLon = toRad(lng2 - lng1);
  float a = sin(dLat / 2) * sin(dLat / 2) +
            cos(toRad(lat1)) * cos(toRad(lat2)) *
            sin(dLon / 2) * sin(dLon / 2);
  float c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c;
}

// ==================== MAIN TRILATERATION FUNCTION ====================

bool calculateTagPosition(float d1_cm, float d2_cm, float d3_cm, 
                         float &out_lat, float &out_lng) {
  // ตรวจสอบว่ามีข้อมูลครบหรือไม่
  if (!anchors[0].valid || !anchors[1].valid || !anchors[2].valid) {
    Serial.println("[ERROR] Anchor GPS not available");
    return false;
  }
  
  if (d1_cm <= 0 || d2_cm <= 0 || d3_cm <= 0) {
    Serial.println("[ERROR] Invalid distances");
    return false;
  }
  
  // แปลงเป็นเมตร
  float d1 = d1_cm / 100.0;
  float d2 = d2_cm / 100.0;
  float d3 = d3_cm / 100.0;
  
  // ใช้ Anchor 1 เป็นจุดอ้างอิง (0, 0)
  float lat0 = anchors[0].lat;
  float lng0 = anchors[0].lng;
  
  // Anchor 1 at (0, 0)
  float x1 = 0;
  float y1 = 0;
  
  // Anchor 2 position relative to Anchor 1
  float x2 = calculateGPSDistance(lat0, lng0, lat0, anchors[1].lng);
  if (anchors[1].lng < lng0) x2 = -x2;
  float y2 = calculateGPSDistance(lat0, lng0, anchors[1].lat, lng0);
  if (anchors[1].lat < lat0) y2 = -y2;
  
  // Anchor 3 position relative to Anchor 1
  float x3 = calculateGPSDistance(lat0, lng0, lat0, anchors[2].lng);
  if (anchors[2].lng < lng0) x3 = -x3;
  float y3 = calculateGPSDistance(lat0, lng0, anchors[2].lat, lng0);
  if (anchors[2].lat < lat0) y3 = -y3;
  
  // Trilateration calculation
  float A = 2 * x2;
  float B = 2 * y2;
  float C = 2 * x3;
  float D = 2 * y3;
  float E = (d1*d1) - (d2*d2) + (x2*x2) + (y2*y2);
  float F = (d1*d1) - (d3*d3) + (x3*x3) + (y3*y3);
  
  // Check for singularity
  float denominator = (A*D - B*C);
  if (abs(denominator) < 0.0001) {
    Serial.println("[ERROR] Singular matrix in trilateration");
    return false;
  }
  
  // Calculate Tag position in local coordinates
  float tag_x = (E*D - B*F) / denominator;
  float tag_y = (A*F - E*C) / denominator;
  
  // Convert back to GPS coordinates
  float meters_per_deg_lat = 111320.0;
  float meters_per_deg_lng = 111320.0 * cos(toRad(lat0));
  
  out_lat = lat0 + (tag_y / meters_per_deg_lat);
  out_lng = lng0 + (tag_x / meters_per_deg_lng);
  
  return true;
}

// ==================== VERIFICATION FUNCTION ====================

void verifyTagPosition() {
  // คำนวณพิกัดจากระยะทาง (ใช้ Reg2 ที่แม่นยำที่สุด)
  float calculated_lat, calculated_lng;
  
  bool success = calculateTagPosition(
    distanceA1Reg2,  // ระยะทาง A1 (cm)
    distanceA2Reg2,  // ระยะทาง A2 (cm)
    distanceA3Reg2,  // ระยะทาง A3 (cm)
    calculated_lat,
    calculated_lng
  );
  
  if (!success) {
    return; // ไม่สามารถคำนวณได้
  }
  
  // เปรียบเทียบกับค่าที่ Tag ส่งมา
  float lat_from_tag = latitude;   // จาก Tag
  float lng_from_tag = longitude;  // จาก Tag
  
  // คำนวณระยะห่าง (error)
  float error_meters = calculateGPSDistance(
    calculated_lat, calculated_lng,
    lat_from_tag, lng_from_tag
  );
  
  Serial.println("\n========== TRILATERATION VERIFICATION ==========");
  Serial.printf("Tag Reported:  %.6f, %.6f\n", lat_from_tag, lng_from_tag);
  Serial.printf("GW Calculated: %.6f, %.6f\n", calculated_lat, calculated_lng);
  Serial.printf("Error: %.2f meters\n", error_meters);
  
  // แจ้งเตือนถ้า error มากกว่า 5 เมตร
  if (error_meters > 5.0) {
    Serial.println("⚠️  WARNING: Large position discrepancy!");
  } else {
    Serial.println("✓ Position verified");
  }
  
  // (Optional) ใช้ค่าที่คำนวณใน Gateway แทนค่าจาก Tag
  // latitude = calculated_lat;
  // longitude = calculated_lng;
  
  Serial.println("================================================\n");
}

// ==================== INTEGRATION ====================

/*
 * วิธีเพิ่มใน Node_Gate_ESP32S3.ino:
 * 
 * 1. Copy ตัวแปร anchors[3] ไปวางด้านบน
 * 2. Copy ฟังก์ชันทั้งหมดไปวางก่อน updateCloudVariables()
 * 3. เรียกใช้ใน updateCloudVariables():
 * 
 *    void updateCloudVariables() {
 *      if (!newDataReceived) return;
 *      
 *      // ... (โค้ดเดิม)
 *      
 *      // เพิ่ม verification
 *      verifyTagPosition();
 *      
 *      newDataReceived = false;
 *    }
 * 
 * 4. ตั้งค่าพิกัด Anchor จริง:
 *    anchors[0] = {13.756331, 100.501762, true};  // Anchor 1
 *    anchors[1] = {13.756450, 100.502100, true};  // Anchor 2
 *    anchors[2] = {13.756200, 100.502300, true};  // Anchor 3
 */

// ==================== USAGE NOTES ====================

/*
 * ข้อดี:
 * ✅ Verify ว่า Tag คำนวณถูกต้อง
 * ✅ ตรวจจับ error/outlier
 * ✅ สามารถใช้ค่าจาก Gateway แทนถ้าแม่นกว่า
 * ✅ Logging สำหรับ debug
 * 
 * ข้อควรระวัง:
 * ⚠️  ต้องตั้งค่าพิกัด Anchor ให้ถูกต้อง
 * ⚠️  ถ้า Anchor เคลื่อนที่ต้องอัปเดท
 * ⚠️  การคำนวณซ้ำใช้ CPU (ประมาณ 1-2%)
 * 
 * ทางเลือก:
 * 1. Verify ทุกครั้ง (แนะนำสำหรับ development)
 * 2. Verify เฉพาะเมื่อสงสัย (production)
 * 3. ปิด verification (ถ้าไว้ใจ Tag 100%)
 */
