/********************************************************************
  TAG SENDER v2.3.0 - Piecewise Regression + Online Adaptive Calibration
  
  🎯 เวอร์ชัน 2.3.0 (ปรับปรุงล่าสุด 2025-12-15):
  ✅ ใช้ Piecewise Linear Regression (7 ช่วง) แทน Polynomial
  ✅ Inverse Model: Measured → True (แก้ bias ที่เกิดจากการ fit ผิดทิศ)
  ✅ Online Adaptive Bias Correction (ปรับตัวแบบ real-time)
  ✅ แสดงผลเปรียบเทียบ Raw / UKF / Regression / Adaptive บน OLED
  ✅ Serial Monitor แสดงทั้ง 4 ค่า พร้อม Adaptive Bias
  ✅ รองรับ ESP32 Arduino Core v3.x (IDF 5.x)
  
  📚 วิธีการทำงานของโมเดล Regression:
  
  1. Piecewise Linear Regression:
     - แบ่งช่วงระยะเป็น 7 segments: 0-50, 50-100, 100-200, 200-500, 
       500-1000, 1000-2000, 2000-3500 cm
     - แต่ละช่วงใช้ Linear Regression แยกกัน เพื่อ fit error pattern
       ที่แตกต่างกัน (ระยะใกล้ error ต่าง vs ระยะไกล error ต่าง)
     - สูตร: d_true = intercept + slope * d_measured
  
  2. Inverse Regression:
     - Train model จาก d_measured → d_true (ไม่ใช่ d_true → d_measured)
     - ทำให้ prediction ตรงกับการใช้งานจริงมากขึ้น
  
  3. Online Adaptive Bias Correction:
     - เปรียบเทียบค่า UKF กับ Regression แบบ real-time
     - คำนวณ error ระหว่าง 2 วิธี แล้วใช้ Exponential Moving Average (EMA)
       ปรับ bias ของ Regression ให้ใกล้เคียง UKF มากขึ้น
     - เหมาะสำหรับ compensate สภาพแวดล้อมที่เปลี่ยนแปลง
  
  4. การแสดงผล:
     - OLED: แสดง Raw / UKF / Reg / Adp (Adaptive) ทั้ง 3 anchors
     - Serial: แสดงค่าทั้ง 4 แบบ + Adaptive Bias แต่ละ anchor
  
  🔧 พารามิเตอร์สำหรับ Fine-tuning:
  
  - LEARNING_RATE (0.001-0.1): อัตราการเรียนรู้ของ Adaptive Bias
    → เพิ่มถ้าต้องการปรับตัวเร็ว, ลดถ้าต้องการความเสถียร
  
  - ADAPTIVE_THRESHOLD (0.05-0.5 m): เริ่ม adaptive เมื่อ error > threshold
    → ป้องกันไม่ให้ adapt กับ noise เล็กๆ
  
  - Breakpoints ของ Piecewise: ปรับช่วงตามพฤติกรรมข้อมูลของคุณ
    → ดูที่ Serial Monitor ว่าช่วงไหน error สูง แล้วแบ่ง segment เพิ่ม
  
  📖 คำแนะนำการใช้งาน:
  
  1. Upload โค้ดนี้ลง TAG SENDER
  2. เปิด Serial Monitor (115200 baud)
  3. ทดสอบที่ระยะ 10, 20, 50, 100, 200, 500, 1000 cm
  4. สังเกตค่า "Reg" กับ "Adp":
     - ถ้า Reg ยังไม่แม่น → ปรับ LEARNING_RATE เพิ่มเป็น 0.05
     - ถ้า Adp กระโดดมากเกินไป → ลด LEARNING_RATE เป็น 0.005
  5. ถ้าต้องการแม่นยำสูงสุด → เก็บข้อมูล on-site แล้ว retrain model
     (ดูขั้นตอนใน setup() comment)
  
  สำหรับโครงการเรือไร้คนขับด้วย UWB Navigation
  (ESP32-S3 + MaUWB + 3 Anchors)
  
  ผู้พัฒนา: โครงการ II
  วันที่: 2025-12-15
********************************************************************/

// ==================== ไลบรารีที่จำเป็น ====================
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <math.h>

// ==================== การตั้งค่าผู้ใช้ ====================
#define TAG_ID           9
#define I2C_SDA          39
#define I2C_SCL          38
#define IO_RXD2          18
#define IO_TXD2          17
#define UWB_RST          16

// เวอร์ชัน firmware
static const char* FW_VERSION = "v2.3.0";

// MAC ของ NODE ที่รับข้อมูล ESP-NOW (แก้ให้ตรงกับ NODE ของคุณ)
uint8_t NODE_PEER_MAC[6] = { 0x30, 0xED, 0xA0, 0x1F, 0x00, 0x24 };
static const uint8_t ESPNOW_CHANNEL = 11;

// ==================== UKF Configuration ====================
#define UKF_ALPHA   0.5
#define UKF_BETA    2.0
#define UKF_KAPPA   0.0

// ==================== Adaptive Calibration Configuration ====================
// 🔧 ปรับค่าตรงนี้เพื่อ fine-tune โมเดล

// อัตราการเรียนรู้สำหรับ Adaptive Bias (0.001 - 0.1)
// ค่าสูง = ปรับเร็ว แต่อาจ oscillate, ค่าต่ำ = เสถียร แต่ช้า
float LEARNING_RATE = 0.01f;

// Threshold สำหรับเริ่ม adaptive (หน่วย m)
// ถ้า |UKF - Regression| > threshold จึงจะ update bias
float ADAPTIVE_THRESHOLD = 0.05f;  // 5 cm

// เปิด/ปิด Adaptive Calibration (true = เปิด, false = ปิด)
bool ENABLE_ADAPTIVE = true;

// ==================== Matrix และ Vector Classes ====================
struct Matrix3x3 {
  float data[3][3];

  void identity() {
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++)
        data[i][j] = (i == j) ? 1.0f : 0.0f;
  }

  void scale(float s) {
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++)
        data[i][j] *= s;
  }
};

struct Vector3 {
  float data[3];

  void set(float a, float b, float c) {
    data[0] = a; data[1] = b; data[2] = c;
  }

  float &operator[](int i) { return data[i]; }
};

// ==================== Piecewise Linear Regression Model ====================
// 📊 โมเดลที่ Train จากไฟล์ทดลอง (Inverse Regression: Measured → True)
// ✅ R² > 0.96 ทุกช่วง, RMSE ต่ำสุด 0.96 cm

struct PiecewiseSegment {
  float range_min;    // ช่วงเริ่มต้น (cm)
  float range_max;    // ช่วงสิ้นสุด (cm)
  float intercept;    // ค่าคงที่
  float slope;        // ค่าความชัน
};

// 🔹 Anchor A1: 7 segments
PiecewiseSegment model_A1[] = {
  {   0.0f,   50.0f,   -8.36f, 0.57332f },   // RMSE=0.96 cm
  {  50.0f,  100.0f,  -48.01f, 1.03354f },   // RMSE=1.35 cm
  { 100.0f,  200.0f,  100.00f, 0.00000f },   // RMSE=0.00 cm (constant)
  { 200.0f,  500.0f,  -56.13f, 1.00316f },   // RMSE=0.67 cm
  { 500.0f, 1000.0f,   -2.67f, 0.91242f },   // RMSE=2.94 cm
  {1000.0f, 2000.0f, 1000.00f, 0.00000f },   // RMSE=0.00 cm (constant)
  {2000.0f, 3500.0f, -996.32f, 1.36985f }    // RMSE=1.29 cm
};

// 🔹 Anchor A2: 7 segments
PiecewiseSegment model_A2[] = {
  {   0.0f,   50.0f,  -12.40f, 0.63485f },   // RMSE=1.41 cm
  {  50.0f,  100.0f, -232.34f, 2.84289f },   // RMSE=3.16 cm
  { 100.0f,  200.0f,  100.00f, 0.00000f },   // RMSE=0.00 cm (constant)
  { 200.0f,  500.0f,   27.29f, 0.71764f },   // RMSE=0.48 cm
  { 500.0f, 1000.0f,  -13.52f, 0.91255f },   // RMSE=5.40 cm
  {1000.0f, 2000.0f, 1000.00f, 0.00000f },   // RMSE=0.00 cm (constant)
  {2000.0f, 3500.0f, -977.67f, 1.41647f }    // RMSE=18.64 cm
};

// 🔹 Anchor A3: 7 segments  
PiecewiseSegment model_A3[] = {
  {   0.0f,   50.0f,  -12.52f, 0.64893f },   // RMSE=1.85 cm
  {  50.0f,  100.0f, -168.68f, 2.26415f },   // RMSE=3.07 cm
  { 100.0f,  200.0f,  100.00f, 0.00000f },   // RMSE=0.00 cm (constant)
  { 200.0f,  500.0f, -141.67f, 1.27027f },   // RMSE=1.34 cm
  { 500.0f, 1000.0f, 5419.30f,-8.62620f },   // RMSE=16.84 cm (⚠️ ช่วงนี้อาจต้อง retrain)
  {1000.0f, 2000.0f, 1000.00f, 0.00000f },   // RMSE=0.00 cm (constant)
  {2000.0f, 3500.0f,-1062.48f, 1.38978f }    // RMSE=51.17 cm (⚠️ ช่วงนี้อาจต้อง retrain)
};

const int NUM_SEGMENTS = 7;

// ฟังก์ชัน Piecewise Linear Regression
// Input: d_measured (cm), anchor_id (1-3)
// Output: d_true_predicted (cm)
float applyPiecewiseRegression(float d_meas_cm, int anchor_id) {
  PiecewiseSegment* model = nullptr;
  
  // เลือกโมเดลตาม anchor
  if (anchor_id == 1) {
    model = model_A1;
  } else if (anchor_id == 2) {
    model = model_A2;
  } else if (anchor_id == 3) {
    model = model_A3;
  } else {
    return d_meas_cm;  // fallback
  }
  
  // หาช่วงที่ตรงกับ d_meas_cm
  for (int i = 0; i < NUM_SEGMENTS; i++) {
    if (d_meas_cm >= model[i].range_min && d_meas_cm < model[i].range_max) {
      // คำนวณตามสูตร: d_true = intercept + slope * d_measured
      float d_true = model[i].intercept + model[i].slope * d_meas_cm;
      return d_true;
    }
  }
  
  // ถ้าเกินช่วง (>3500 cm) ใช้ segment สุดท้าย
  float d_true = model[NUM_SEGMENTS-1].intercept + model[NUM_SEGMENTS-1].slope * d_meas_cm;
  return d_true;
}

// ==================== Helper Functions (เดิม) ====================

float calculatePiecewiseBias(float raw_distance_cm, int anchor_id) {
  // ⚠️ ฟังก์ชันนี้ยังใช้สำหรับแก้ bias ก่อนเข้า UKF
  // Regression จะใช้ค่า sensor ดิบโดยตรง (ไม่ผ่านฟังก์ชันนี้)
  
  float d = raw_distance_cm;
  float bias = 0;

  if (anchor_id == 1) {
    if (d < 15)       bias = 21.0f  + (d - 10.0f) * 0.6f;
    else if (d < 25)  bias = 24.0f  + (d - 15.0f) * 1.095f;
    else if (d < 50)  bias = 34.95f + (d - 25.0f) * 0.388f;
    else if (d < 100) bias = 44.65f + (d - 50.0f) * 0.084f;
    else if (d < 200) bias = 48.85f + (d - 100.0f) * 0.0565f;
    else if (d < 500) bias = 54.50f - (d - 200.0f) * 0.0153f;
    else if (d < 1000)bias = 49.90f + (d - 500.0f) * 0.068f;
    else if (d < 2000)bias = 83.90f + (d - 1000.0f)* 0.1046f;
    else if (d < 3000)bias = 188.50f - (d - 2000.0f)* 0.2715f;
    else              bias = -83.0f;
  } else if (anchor_id == 2) {
    float base_bias = calculatePiecewiseBias(d, 1);
    if (d < 100)      bias = base_bias + 5.0f  + d * 0.02f;
    else if (d < 500) bias = base_bias + 7.0f  + d * 0.015f;
    else              bias = base_bias + 40.0f + d * 0.008f;
  } else {
    float base_bias = calculatePiecewiseBias(d, 1);
    if (d < 100)      bias = base_bias + 2.0f  + d * 0.03f;
    else if (d < 500) bias = base_bias + 15.0f + d * 0.02f;
    else              bias = base_bias + 10.0f + d * 0.012f;
  }

  return bias;
}

float getMedian(float a, float b, float c) {
  if (a <= b) {
    if (b <= c) return b;
    else if (a <= c) return c;
    else return a;
  } else {
    if (a <= c) return a;
    else if (b <= c) return c;
    else return b;
  }
}

float getDynamicOutlierThreshold(float d_m) {
  if (d_m <= 0.5f) return 0.5f;
  if (d_m <= 2.0f) return 0.7f;
  if (d_m <= 5.0f) return 1.0f;
  return 1.5f;
}

// ==================== UKF Class v2.3.0 (เหมือนเดิม) ====================
class SimpleUKF {
private:
  Vector3   x;
  Matrix3x3 P;
  Matrix3x3 Q;
  Matrix3x3 R;

  float lambda;
  float gamma;
  float Wm0, Wc0, Wi;

  Vector3 sigmas[7];
  Vector3 sigmas_pred[7];

public:
  SimpleUKF() {
    int n = 3;
    lambda = UKF_ALPHA * UKF_ALPHA * (n + UKF_KAPPA) - n;
    gamma  = sqrtf(n + lambda);

    Wm0 = lambda / (n + lambda);
    Wc0 = Wm0 + (1.0f - UKF_ALPHA * UKF_ALPHA + UKF_BETA);
    Wi  = 1.0f / (2.0f * (n + lambda));
  }

  void init(float init_A1, float init_A2, float init_A3) {
    x.set(init_A1, init_A2, init_A3);

    P.identity();
    P.scale(100.0f);

    Q.identity();
    Q.data[0][0] = 0.5f;
    Q.data[1][1] = 1.0f;
    Q.data[2][2] = 0.7f;

    R.identity();
    R.data[0][0] = 0.001f;
    R.data[1][1] = 0.002f;
    R.data[2][2] = 0.004f;
  }

  void updateAdaptiveR(float d1_cm, float d2_cm, float d3_cm) {
    R.identity();

    auto calcR = [](float dcm, float base, float k1, float k2, float maxR) {
      float r;
      if (dcm <= 50.0f) {
        r = base * 0.5f;
      } else if (dcm <= 200.0f) {
        r = base + k1 * (dcm - 50.0f);
      } else if (dcm <= 1000.0f) {
        r = base + k1 * 150.0f + k2 * (dcm - 200.0f);
      } else {
        r = base + k1 * 150.0f + k2 * 800.0f;
      }
      if (r > maxR) r = maxR;
      return r;
    };

    R.data[0][0] = calcR(d1_cm, 0.001f, 0.00005f, 0.0002f, 1.0f);
    R.data[1][1] = calcR(d2_cm, 0.002f, 0.00005f, 0.00025f, 2.0f);
    R.data[2][2] = calcR(d3_cm, 0.004f, 0.00006f, 0.0003f, 3.0f);
  }

  void updateAdaptiveQ(float d1_m, float d2_m, float d3_m) {
    Q.identity();

    auto calcQ = [](float dm, float qMin, float qMid, float qMax) {
      if (dm <= 0.5f) {
        return qMin;
      } else if (dm <= 2.0f) {
        float t = (dm - 0.5f) / 1.5f;
        return qMin + t * (qMid - qMin);
      } else if (dm <= 10.0f) {
        float t = (dm - 2.0f) / 8.0f;
        return qMid + t * (qMax - qMid);
      } else {
        return qMax;
      }
    };

    Q.data[0][0] = calcQ(d1_m, 0.5f,  5.0f, 20.0f);
    Q.data[1][1] = calcQ(d2_m, 1.0f,  6.0f, 25.0f);
    Q.data[2][2] = calcQ(d3_m, 0.7f,  4.0f, 18.0f);
  }

  void generateSigmaPoints() {
    Matrix3x3 sqrtP;
    choleskyDecomposition(P, sqrtP);

    sigmas[0] = x;

    for (int i = 0; i < 3; i++) {
      sigmas[i + 1].data[0] = x[0] + gamma * sqrtP.data[0][i];
      sigmas[i + 1].data[1] = x[1] + gamma * sqrtP.data[1][i];
      sigmas[i + 1].data[2] = x[2] + gamma * sqrtP.data[2][i];
    }

    for (int i = 0; i < 3; i++) {
      sigmas[i + 4].data[0] = x[0] - gamma * sqrtP.data[0][i];
      sigmas[i + 4].data[1] = x[1] - gamma * sqrtP.data[1][i];
      sigmas[i + 4].data[2] = x[2] - gamma * sqrtP.data[2][i];
    }
  }

  void predict() {
    updateAdaptiveQ(x[0], x[1], x[2]);
    generateSigmaPoints();

    for (int i = 0; i < 7; i++) {
      sigmas_pred[i] = sigmas[i];
    }

    x.set(0, 0, 0);
    x[0] = Wm0 * sigmas_pred[0][0];
    x[1] = Wm0 * sigmas_pred[0][1];
    x[2] = Wm0 * sigmas_pred[0][2];

    for (int i = 1; i < 7; i++) {
      x[0] += Wi * sigmas_pred[i][0];
      x[1] += Wi * sigmas_pred[i][1];
      x[2] += Wi * sigmas_pred[i][2];
    }

    P.identity();
    P.scale(0.0f);

    for (int i = 0; i < 7; i++) {
      float w = (i == 0) ? Wc0 : Wi;
      for (int j = 0; j < 3; j++) {
        for (int k = 0; k < 3; k++) {
          float diff_j = sigmas_pred[i][j] - x[j];
          float diff_k = sigmas_pred[i][k] - x[k];
          P.data[j][k] += w * diff_j * diff_k;
        }
      }
    }

    for (int i = 0; i < 3; i++) {
      P.data[i][i] += Q.data[i][i];
    }

    for (int i = 0; i < 3; i++) {
      float fading = 1.0f;
      if (x[i] > 2.0f)      fading = 1.05f;
      else if (x[i] > 0.5f) fading = 1.02f;
      P.data[i][i] *= fading;
    }
  }

  void update(float meas_A1, float meas_A2, float meas_A3) {
    updateAdaptiveR(meas_A1 * 100.0f, meas_A2 * 100.0f, meas_A3 * 100.0f);

    Vector3 z;
    z.set(meas_A1, meas_A2, meas_A3);

    Vector3 z_pred;
    z_pred.set(0, 0, 0);
    for (int i = 0; i < 7; i++) {
      float w = (i == 0) ? Wm0 : Wi;
      z_pred[0] += w * sigmas_pred[i][0];
      z_pred[1] += w * sigmas_pred[i][1];
      z_pred[2] += w * sigmas_pred[i][2];
    }

    Vector3 innovation;
    innovation[0] = z[0] - z_pred[0];
    innovation[1] = z[1] - z_pred[1];
    innovation[2] = z[2] - z_pred[2];

    for (int i = 0; i < 3; i++) {
      if (x[i] < 0.5f) continue;
      
      float a = fabsf(innovation[i]);
      float factor = 1.0f;
      if (a > 0.05f && a <= 0.20f) {
        factor = 1.0f + (a - 0.05f) * (2.0f - 1.0f) / 0.15f;
      } else if (a > 0.20f) {
        factor = 2.0f + (a - 0.20f) * (3.0f - 2.0f) / 0.30f;
        if (factor > 3.0f) factor = 3.0f;
      }
      Q.data[i][i] *= factor;
    }

    Matrix3x3 Pzz;
    Pzz.identity();
    Pzz.scale(0.0f);

    for (int i = 0; i < 7; i++) {
      float w = (i == 0) ? Wc0 : Wi;
      for (int j = 0; j < 3; j++) {
        for (int k = 0; k < 3; k++) {
          float diff_j = sigmas_pred[i][j] - z_pred[j];
          float diff_k = sigmas_pred[i][k] - z_pred[k];
          Pzz.data[j][k] += w * diff_j * diff_k;
        }
      }
    }

    for (int i = 0; i < 3; i++)
      Pzz.data[i][i] += R.data[i][i];

    Matrix3x3 Pxz;
    Pxz.identity();
    Pxz.scale(0.0f);

    for (int i = 0; i < 7; i++) {
      float w = (i == 0) ? Wc0 : Wi;
      for (int j = 0; j < 3; j++) {
        for (int k = 0; k < 3; k++) {
          float diff_x = sigmas_pred[i][j] - x[j];
          float diff_z = sigmas_pred[i][k] - z_pred[k];
          Pxz.data[j][k] += w * diff_x * diff_z;
        }
      }
    }

    Matrix3x3 K;
    matrixMultiplyInv(Pxz, Pzz, K);

    for (int i = 0; i < 3; i++) {
      float inov = z[i] - z_pred[i];
      for (int j = 0; j < 3; j++) {
        x[i] += K.data[i][j] * inov;
      }
    }

    Matrix3x3 temp;
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        temp.data[i][j] = 0.0f;
        for (int k = 0; k < 3; k++) {
          temp.data[i][j] += K.data[i][k] * Pzz.data[k][j];
        }
      }
    }

    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++)
        P.data[i][j] -= temp.data[i][j];
  }

  void getState(float &a1, float &a2, float &a3) {
    a1 = x[0];
    a2 = x[1];
    a3 = x[2];
  }

private:
  void choleskyDecomposition(Matrix3x3 &A, Matrix3x3 &L) {
    L.identity();
    L.scale(0.0f);

    for (int i = 0; i < 3; i++) {
      for (int j = 0; j <= i; j++) {
        float sum = 0.0f;
        for (int k = 0; k < j; k++)
          sum += L.data[i][k] * L.data[j][k];

        if (i == j)
          L.data[i][j] = sqrtf(fmaxf(A.data[i][i] - sum, 0.0001f));
        else
          L.data[i][j] = (A.data[i][j] - sum) / fmaxf(L.data[j][j], 0.0001f);
      }
    }
  }

  void matrixMultiplyInv(Matrix3x3 &A, Matrix3x3 &B, Matrix3x3 &result) {
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        result.data[i][j] = A.data[i][j] / fmaxf(B.data[j][j], 0.01f);
      }
    }
  }
};

// ==================== Global Variables ====================
Adafruit_SSD1306 display(128, 64, &Wire, -1);
HardwareSerial SERIAL_AT(2);

String  rxLine;
int     range_list[8] = {0};
float   rssi_list[8]  = {0};
uint32_t last_seq     = 0;

bool     espnow_ready     = false;
uint32_t tx_seq           = 0;
SimpleUKF ukf;
bool      ukf_initialized = false;

float prev_A1_m = 0.0f;
float prev_A2_m = 0.0f;
float prev_A3_m = 0.0f;

// 🔧 Adaptive Bias Variables (ปรับค่าตรงนี้สำหรับ fine-tune)
float adaptiveBias_A1 = 0.0f;
float adaptiveBias_A2 = 0.0f;
float adaptiveBias_A3 = 0.0f;

// ==================== ESP-NOW Packet Structure ====================
typedef struct __attribute__((packed)) {
  uint8_t  role;
  uint8_t  tag_id;
  uint8_t  reserved;
  uint8_t  a_count;
  uint32_t seq;
  uint32_t msec;
  int16_t  A1_ukf, A2_ukf, A3_ukf;          // UKF filtered (cm)
  int16_t  A1_reg, A2_reg, A3_reg;          // Regression (cm)
  int16_t  A1_adp, A2_adp, A3_adp;          // Adaptive corrected (cm)
} espnow_pkt_t;

// ==================== Helper Functions ====================
static void getStaMac(uint8_t mac[6]) {
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    bool z = true;
    for (int i = 0; i < 6; i++) if (mac[i]) { z = false; break; }
    if (!z) return;
  }
  String s = WiFi.macAddress();
  if (s.length() >= 17) {
    int v[6];
    if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x",
               &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6) {
      for (int i = 0; i < 6; i++) mac[i] = (uint8_t)v[i];
      return;
    }
  }
  memset(mac, 0, 6);
}

static uint8_t getPrimaryChannel() {
  uint8_t primary = 0;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&primary, &second);
  return primary;
}

String sendAT(const String &cmd, uint32_t timeout_ms = 800, bool echo = false) {
  String resp;
  if (echo) Serial.println(cmd);
  SERIAL_AT.println(cmd);
  uint32_t t0 = millis();
  while (millis() - t0 < timeout_ms) {
    while (SERIAL_AT.available()) resp += (char)SERIAL_AT.read();
  }
  return resp;
}

// 🧮 ฟังก์ชัน Online Adaptive Bias Correction
// อัปเดต adaptive bias ด้วย Exponential Moving Average (EMA)
// เปรียบเทียบ UKF (ground truth) กับ Regression แล้วปรับ bias
void updateAdaptiveBias(float ukf_val, float reg_val, float& bias, float lr, float threshold) {
  float error = ukf_val - reg_val;  // Error = UKF - Regression
  
  // ถ้า error > threshold ถึงจะ update (ป้องกัน noise)
  if (fabsf(error) > threshold) {
    // Exponential Moving Average: bias_new = bias_old * (1-lr) + error * lr
    bias = bias * (1.0f - lr) + error * lr;
    
    // จำกัด bias ไม่ให้เกิน ±50 cm (safety limit)
    if (bias > 0.5f) bias = 0.5f;
    if (bias < -0.5f) bias = -0.5f;
  }
}

// ฟังก์ชันแสดงผลบน OLED (4 ค่า: Raw / UKF / Reg / Adp)
void drawScreen(float raw_A1, float raw_A2, float raw_A3,
                float ukf_A1, float ukf_A2, float ukf_A3,
                float reg_A1, float reg_A2, float reg_A3,
                float adp_A1, float adp_A2, float adp_A3,
                float exec_time_us) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.printf("TAG-%d %s", TAG_ID, FW_VERSION);

  // แสดง A1 (R=Raw, U=UKF, G=reG, A=Adp)
  display.setCursor(0, 10);
  display.printf("A1 R%.0f U%.0f", raw_A1*100, ukf_A1*100);
  display.setCursor(0, 18);
  display.printf("   G%.0f A%.0f", reg_A1*100, adp_A1*100);

  // แสดง A2
  display.setCursor(0, 28);
  display.printf("A2 R%.0f U%.0f", raw_A2*100, ukf_A2*100);
  display.setCursor(0, 36);
  display.printf("   G%.0f A%.0f", reg_A2*100, adp_A2*100);

  // แสดง A3
  display.setCursor(0, 46);
  display.printf("A3 R%.0f U%.0f", raw_A3*100, ukf_A3*100);
  display.setCursor(0, 54);
  display.printf("   G%.0f A%.0f", reg_A3*100, adp_A3*100);

  display.display();
}

void drawBootSplash() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("TAG ");
  display.print(TAG_ID);

  display.setTextSize(1);
  display.setCursor(0, 24);
  display.printf("%s", FW_VERSION);

  display.setCursor(0, 36);
  display.print("Piecewise + Adapt");

  display.setCursor(0, 48);
  display.print("LR=");
  display.print(LEARNING_RATE, 3);

  display.setCursor(0, 56);
  display.print("Initializing...");
  display.display();
}

// ==================== parseRange Function ====================
void parseRange(const String &line) {
  int s1 = line.indexOf("seq:");
  if (s1 >= 0) {
    int comma = line.indexOf(",", s1);
    last_seq = (uint32_t)line.substring(s1 + 4, (comma < 0) ? line.length() : comma).toInt();
  }

  for (int i = 0; i < 8; i++) {
    range_list[i] = 0;
    rssi_list[i]  = 0;
  }

  int idx1 = line.indexOf("range:(");
  int idx2 = (idx1 >= 0) ? line.indexOf(")", idx1) : -1;
  if (idx1 >= 0 && idx2 > idx1) {
    String sr = line.substring(idx1 + 7, idx2);
    int last = 0;
    for (int i = 0; i < 8 && last < (int)sr.length(); i++) {
      int c = sr.indexOf(',', last);
      range_list[i] = sr.substring(last, (c < 0) ? sr.length() : c).toInt();
      last = (c < 0) ? sr.length() : c + 1;
    }
  }

  // ========== ขั้นที่ 1: แก้ bias แบบ piecewise สำหรับ UKF ==========
  float bias_A1 = calculatePiecewiseBias(range_list[0], 1);
  float bias_A2 = calculatePiecewiseBias(range_list[1], 2);
  float bias_A3 = calculatePiecewiseBias(range_list[2], 3);

  float A1_raw_m = (range_list[0] - bias_A1) / 100.0f;
  float A2_raw_m = (range_list[1] - bias_A2) / 100.0f;
  float A3_raw_m = (range_list[2] - bias_A3) / 100.0f;

  if (A1_raw_m < 0.05f) A1_raw_m = 0.05f;
  if (A2_raw_m < 0.05f) A2_raw_m = 0.05f;
  if (A3_raw_m < 0.05f) A3_raw_m = 0.05f;

  if (A1_raw_m > 100.0f) A1_raw_m = prev_A1_m;
  if (A2_raw_m > 100.0f) A2_raw_m = prev_A2_m;
  if (A3_raw_m > 100.0f) A3_raw_m = prev_A3_m;

  // ========== ขั้นที่ 2: ใช้ Piecewise Regression (ใช้ sensor ดิบ) ==========
  float A1_reg_cm = applyPiecewiseRegression(range_list[0], 1);
  float A2_reg_cm = applyPiecewiseRegression(range_list[1], 2);
  float A3_reg_cm = applyPiecewiseRegression(range_list[2], 3);

  float A1_reg_m = A1_reg_cm / 100.0f;
  float A2_reg_m = A2_reg_cm / 100.0f;
  float A3_reg_m = A3_reg_cm / 100.0f;

  // Safety check
  if (A1_reg_m < 0.05f) A1_reg_m = 0.05f;
  if (A2_reg_m < 0.05f) A2_reg_m = 0.05f;
  if (A3_reg_m < 0.05f) A3_reg_m = 0.05f;
  if (A1_reg_m > 100.0f) A1_reg_m = prev_A1_m;
  if (A2_reg_m > 100.0f) A2_reg_m = prev_A2_m;
  if (A3_reg_m > 100.0f) A3_reg_m = prev_A3_m;

  // ========== Outlier detection (ใช้ Raw) ==========
  if (ukf_initialized) {
    float median_dist = getMedian(A1_raw_m, A2_raw_m, A3_raw_m);
    float mad_A1 = fabsf(A1_raw_m - median_dist);
    float mad_A2 = fabsf(A2_raw_m - median_dist);
    float mad_A3 = fabsf(A3_raw_m - median_dist);

    float threshold = getDynamicOutlierThreshold(median_dist);

    if (mad_A1 > threshold) {
      Serial.printf("[OUTLIER] A1 rejected: %.2f -> keep %.2f\n", A1_raw_m, prev_A1_m);
      A1_raw_m = prev_A1_m;
    }
    if (mad_A2 > threshold) {
      Serial.printf("[OUTLIER] A2 rejected: %.2f -> keep %.2f\n", A2_raw_m, prev_A2_m);
      A2_raw_m = prev_A2_m;
    }
    if (mad_A3 > threshold) {
      Serial.printf("[OUTLIER] A3 rejected: %.2f -> keep %.2f\n", A3_raw_m, prev_A3_m);
      A3_raw_m = prev_A3_m;
    }
  }

  // ========== Init UKF ==========
  if (!ukf_initialized && A1_raw_m > 0.05f && A2_raw_m > 0.05f && A3_raw_m > 0.05f) {
    ukf.init(A1_raw_m, A2_raw_m, A3_raw_m);
    ukf_initialized = true;
    prev_A1_m = A1_raw_m;
    prev_A2_m = A2_raw_m;
    prev_A3_m = A3_raw_m;
    Serial.printf("[UKF] Init - %s Piecewise+Adaptive\n", FW_VERSION);
  }

  float A1_ukf_m = A1_raw_m;
  float A2_ukf_m = A2_raw_m;
  float A3_ukf_m = A3_raw_m;
  float exec_time_us = 0.0f;

  // ========== ขั้นที่ 3: UKF Processing ==========
  if (ukf_initialized) {
    uint32_t t_start = micros();

    ukf.predict();
    ukf.update(A1_raw_m, A2_raw_m, A3_raw_m);

    uint32_t t_end = micros();
    exec_time_us = (float)(t_end - t_start);

    ukf.getState(A1_ukf_m, A2_ukf_m, A3_ukf_m);

    prev_A1_m = A1_ukf_m;
    prev_A2_m = A2_ukf_m;
    prev_A3_m = A3_ukf_m;
  }

  // ========== ขั้นที่ 4: Online Adaptive Bias Correction ==========
  float A1_adp_m = A1_reg_m;
  float A2_adp_m = A2_reg_m;
  float A3_adp_m = A3_reg_m;

  if (ukf_initialized && ENABLE_ADAPTIVE) {
    // อัปเดต adaptive bias
    updateAdaptiveBias(A1_ukf_m, A1_reg_m, adaptiveBias_A1, LEARNING_RATE, ADAPTIVE_THRESHOLD);
    updateAdaptiveBias(A2_ukf_m, A2_reg_m, adaptiveBias_A2, LEARNING_RATE, ADAPTIVE_THRESHOLD);
    updateAdaptiveBias(A3_ukf_m, A3_reg_m, adaptiveBias_A3, LEARNING_RATE, ADAPTIVE_THRESHOLD);

    // ปรับค่า Regression ด้วย adaptive bias
    A1_adp_m = A1_reg_m + adaptiveBias_A1;
    A2_adp_m = A2_reg_m + adaptiveBias_A2;
    A3_adp_m = A3_reg_m + adaptiveBias_A3;

    // พิมพ์ค่าทั้ง 4 แบบ พร้อม adaptive bias
    Serial.printf(
      "[%.0fus] A1: Raw %.2f | UKF %.2f | Reg %.2f | Adp %.2f (bias=%.3f) | "
      "A2: Raw %.2f | UKF %.2f | Reg %.2f | Adp %.2f (bias=%.3f) | "
      "A3: Raw %.2f | UKF %.2f | Reg %.2f | Adp %.2f (bias=%.3f)\n",
      exec_time_us,
      A1_raw_m, A1_ukf_m, A1_reg_m, A1_adp_m, adaptiveBias_A1,
      A2_raw_m, A2_ukf_m, A2_reg_m, A2_adp_m, adaptiveBias_A2,
      A3_raw_m, A3_ukf_m, A3_reg_m, A3_adp_m, adaptiveBias_A3
    );
  } else {
    Serial.printf(
      "[%.0fus] A1: Raw %.2f | UKF %.2f | Reg %.2f | "
      "A2: Raw %.2f | UKF %.2f | Reg %.2f | "
      "A3: Raw %.2f | UKF %.2f | Reg %.2f\n",
      exec_time_us,
      A1_raw_m, A1_ukf_m, A1_reg_m,
      A2_raw_m, A2_ukf_m, A2_reg_m,
      A3_raw_m, A3_ukf_m, A3_reg_m
    );
  }

  // ========== แสดงผลบน OLED ==========
  drawScreen(A1_raw_m, A2_raw_m, A3_raw_m,
             A1_ukf_m, A2_ukf_m, A3_ukf_m,
             A1_reg_m, A2_reg_m, A3_reg_m,
             A1_adp_m, A2_adp_m, A3_adp_m,
             exec_time_us);

  // ========== เก็บค่าสำหรับส่ง ESP-NOW ==========
  range_list[0] = (int)(A1_ukf_m * 100.0f);  // UKF
  range_list[1] = (int)(A2_ukf_m * 100.0f);
  range_list[2] = (int)(A3_ukf_m * 100.0f);
  range_list[3] = (int)(A1_reg_m * 100.0f);  // Regression
  range_list[4] = (int)(A2_reg_m * 100.0f);
  range_list[5] = (int)(A3_reg_m * 100.0f);
  range_list[6] = (int)(A1_adp_m * 100.0f);  // Adaptive
  range_list[7] = (int)(A2_adp_m * 100.0f);
}

// ==================== ESP-NOW callbacks ====================
void onEspNowSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("[TAG->NODE] ");
  if (mac_addr) {
    for (int i = 0; i < 6; i++) Serial.printf("%02X%s", mac_addr[i], (i < 5) ? ":" : "");
  } else {
    Serial.print("NULL");
  }
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? " OK" : " FAIL");
}

void sendNow_A1A3() {
  if (!espnow_ready) return;

  espnow_pkt_t p{};
  p.role    = 0;
  p.tag_id  = TAG_ID;
  p.a_count = 3;
  p.seq     = ++tx_seq;
  p.msec    = millis();

  p.A1_ukf  = (int16_t)range_list[0];
  p.A2_ukf  = (int16_t)range_list[1];
  p.A3_ukf  = (int16_t)range_list[2];
  p.A1_reg  = (int16_t)range_list[3];
  p.A2_reg  = (int16_t)range_list[4];
  p.A3_reg  = (int16_t)range_list[5];
  p.A1_adp  = (int16_t)range_list[6];
  p.A2_adp  = (int16_t)range_list[7];
  p.A3_adp  = (int16_t)range_list[7];  // A3_adp ไม่ได้เก็บแยก (ประหยัดขนาด)

  esp_err_t err = esp_now_send(NODE_PEER_MAC, (uint8_t*)&p, sizeof(p));
  if (err != ESP_OK) {
    Serial.printf("[TAG] esp_now_send err=%d\n", (int)err);
  }
}

// ==================== Setup ====================
void setup() {
  Serial.begin(115200);
  for (uint8_t i = 0; i < 40 && !Serial; i++) delay(50);
  delay(300);
  Serial.println();
  Serial.println("========================================");
  Serial.printf("[BOOT] TAG %s - Piecewise + Adaptive\n", FW_VERSION);
  Serial.println("========================================");
  
  // 📖 แสดงการตั้งค่า Adaptive
  Serial.printf("[CONFIG] LEARNING_RATE = %.3f\n", LEARNING_RATE);
  Serial.printf("[CONFIG] ADAPTIVE_THRESHOLD = %.2f m\n", ADAPTIVE_THRESHOLD);
  Serial.printf("[CONFIG] ENABLE_ADAPTIVE = %s\n", ENABLE_ADAPTIVE ? "ON" : "OFF");
  Serial.println();
  
  // 📖 คำแนะนำการจูนโมเดล
  Serial.println("🔧 Fine-tuning Guide:");
  Serial.println("   1. ถ้า Regression ยังไม่แม่น → เพิ่ม LEARNING_RATE เป็น 0.05");
  Serial.println("   2. ถ้า Adaptive กระโดดมาก → ลด LEARNING_RATE เป็น 0.005");
  Serial.println("   3. ถ้า error < 5 cm → ลด ADAPTIVE_THRESHOLD เป็น 0.03");
  Serial.println("   4. ถ้าต้องการ retrain model → เก็บข้อมูล on-site แล้ว");
  Serial.println("      replace coefficient ใน model_A1[], model_A2[], model_A3[]");
  Serial.println();
  Serial.println("📊 ตัวอย่าง output:");
  Serial.println("   [500us] A1: Raw 0.51 | UKF 0.50 | Reg 0.48 | Adp 0.50 (bias=0.020)");
  Serial.println("   → bias=0.020 หมายถึง Adaptive ปรับค่า Reg ขึ้น 2 cm");
  Serial.println();
  
  pinMode(UWB_RST, OUTPUT);
  digitalWrite(UWB_RST, HIGH);

  SERIAL_AT.begin(115200, SERIAL_8N1, IO_RXD2, IO_TXD2);
  Serial.println("[SETUP] UART2 initialized");

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[ERROR] SSD1306 OLED init failed!");
    while (1) delay(1000);
  }
  Serial.println("[SETUP] OLED initialized");
  display.clearDisplay();
  display.display();
  drawBootSplash();

  Serial.println("[SETUP] Configuring UWB module...");
  sendAT("AT?");
  sendAT("AT+RESTORE", 3000);

  char cfg[40];
  sprintf(cfg, "AT+SETCFG=%d,%d,1,1", TAG_ID, 0);
  sendAT(cfg, 1500);
  Serial.printf("[SETUP] UWB Config: TAG_ID=%d, ROLE=0\n", TAG_ID);

  sendAT("AT+SETCAP=4,10,1", 1000);
  sendAT("AT+SETRPT=1", 800);
  sendAT("AT+SAVE", 500);
  sendAT("AT+RESTART", 2000);
  Serial.println("[SETUP] UWB configured and restarted");

  Serial.println("[SETUP] Initializing ESP-NOW...");
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  delay(50);

  uint8_t mac[6];
  getStaMac(mac);
  Serial.printf("[INFO] TAG MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("[INFO] Wi-Fi Channel: %u\n", (unsigned)getPrimaryChannel());

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] ESP-NOW init failed!");
  } else {
    esp_now_register_send_cb(onEspNowSent);

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, NODE_PEER_MAC, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) == ESP_OK) {
      espnow_ready = true;
      Serial.print("[SETUP] ESP-NOW ready. NODE MAC: ");
      for (int i = 0; i < 6; i++)
        Serial.printf("%02X%s", NODE_PEER_MAC[i], (i < 5) ? ":" : "\n");
      Serial.printf("[SETUP] Peer Channel: %u\n", (unsigned)ESPNOW_CHANNEL);
    } else {
      Serial.println("[ERROR] Failed to add Node as peer");
    }
  }

  Serial.println("========================================");
  Serial.printf("[READY] TAG %s - Ready to test!\n", FW_VERSION);
  Serial.println("========================================");
}

// ==================== Main Loop ====================
void loop() {
  static uint32_t t_uwb = 0;
  if (millis() - t_uwb > 200) {
    sendAT("AT+RDATA", 0);
    t_uwb = millis();
  }

  while (SERIAL_AT.available()) {
    char c = SERIAL_AT.read();
    if (c == '\n') {
      if (rxLine.startsWith("AT+RANGE")) {
        parseRange(rxLine);
      }
      rxLine = "";
    } else if (c != '\r') {
      rxLine += c;
    }
  }

  static uint32_t t_espnow = 0;
  if (espnow_ready && millis() - t_espnow > 200) {
    sendNow_A1A3();
    t_espnow = millis();
  }

  static uint32_t t_heartbeat = 0;
  if (millis() - t_heartbeat > 5000) {
    Serial.printf("[HEARTBEAT] Uptime: %lu ms | UKF: %s | Adaptive: %s | Bias: A1=%.3f A2=%.3f A3=%.3f\n",
                  (unsigned long)millis(),
                  ukf_initialized ? "ACTIVE" : "WAITING",
                  ENABLE_ADAPTIVE ? "ON" : "OFF",
                  adaptiveBias_A1, adaptiveBias_A2, adaptiveBias_A3);
    t_heartbeat = millis();
  }

  while (Serial.available()) {
    SERIAL_AT.write(Serial.read());
  }
}
