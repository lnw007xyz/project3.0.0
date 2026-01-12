/********************************************************************
  TAG SENDER v2.1.4 - UWB UKF A1/A2 Tuned Like A3
  
  เวอร์ชัน 2.1.4 (ปรับปรุงล่าสุด 2025-12-15):
  ✅ ปรับ A1/A2 ให้มีพฤติกรรมคล้าย A3 (R/Q ใกล้เคียงกัน)
  ✅ R base: A1=0.003, A2=0.0035, A3=0.004 (ใกล้เคียง)
  ✅ Q init: A1/A2/A3 = 0.4 (เท่ากันหมด)
  ✅ A1 Bias Slope 1.75 คงไว้
  ✅ เป้าหมาย: A1/A2/A3 คลาดเคลื่อนใกล้เคียงกัน ±5-8 cm
  
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
#define TAG_ID           0
#define I2C_SDA          39
#define I2C_SCL          38
#define IO_RXD2          18
#define IO_TXD2          17
#define UWB_RST          16

// เวอร์ชัน firmware
static const char* FW_VERSION = "v2.1.4";

// MAC ของ NODE ที่รับข้อมูล ESP-NOW (แก้ให้ตรงกับ NODE ของคุณ)
uint8_t NODE_PEER_MAC[6] = { 0x30, 0xED, 0xA0, 0x1F, 0x00, 0x24 };
static const uint8_t ESPNOW_CHANNEL = 11;

// ==================== UKF Configuration ====================
#define UKF_ALPHA   0.5
#define UKF_BETA    2.0
#define UKF_KAPPA   0.0

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

// ==================== Helper Functions ====================

float calculatePiecewiseBias(float raw_distance_cm, int anchor_id) {
  float d = raw_distance_cm;
  float bias = 0;

  if (anchor_id == 1) {
    // A1: แก้ slope ช่วง 15-25 cm (แก้ปัญหาสูงเกิน +3.27cm ที่ 20cm)
    if (d < 15)       bias = 21.0f  + (d - 10.0f) * 0.6f;
    else if (d < 20)  bias = 24.0f  + (d - 15.0f) * 1.75f;
    else if (d < 25)  bias = 32.75f + (d - 20.0f) * 0.44f;
    else if (d < 50)  bias = 34.95f + (d - 25.0f) * 0.388f;
    else if (d < 100) bias = 44.65f + (d - 50.0f) * 0.084f;
    else if (d < 200) bias = 48.85f + (d - 100.0f) * 0.0565f;
    else if (d < 500) bias = 54.50f - (d - 200.0f) * 0.0153f;
    else if (d < 1000)bias = 49.90f + (d - 500.0f) * 0.068f;
    else if (d < 2000)bias = 83.90f + (d - 1000.0f)* 0.1046f;
    else if (d < 3000)bias = 188.50f - (d - 2000.0f)* 0.2715f;
    else              bias = -83.0f;
    
  } else if (anchor_id == 2) {
    // A2: Piecewise แยกเอง (จากข้อมูลจริง CSV)
    if (d < 15)       bias = 26.8f;
    else if (d < 20)  bias = 26.8f  + (d - 15.0f) * 0.56f;
    else if (d < 25)  bias = 29.6f  + (d - 20.0f) * 0.44f;
    else if (d < 50)  bias = 31.8f  + (d - 25.0f) * 0.74f;
    else if (d < 100) bias = 50.3f  + (d - 50.0f) * (-0.213f);
    else if (d < 200) bias = 39.65f + (d - 100.0f) * 0.10f;
    else if (d < 500) bias = 49.65f + (d - 200.0f) * 0.05f;
    else if (d < 1000)bias = 64.65f + (d - 500.0f) * 0.08f;
    else if (d < 2000)bias = 104.65f + (d - 1000.0f)* 0.10f;
    else if (d < 3000)bias = 204.65f - (d - 2000.0f)* 0.20f;
    else              bias = 4.65f;
    
  } else {
    // A3: Piecewise แยกเอง (จากข้อมูลจริง CSV)
    if (d < 15)       bias = 23.3f;
    else if (d < 20)  bias = 23.3f  + (d - 15.0f) * 1.37f;
    else if (d < 25)  bias = 30.15f + (d - 20.0f) * 1.26f;
    else if (d < 50)  bias = 36.45f + (d - 25.0f) * 0.47f;
    else if (d < 100) bias = 48.2f  + (d - 50.0f) * (-0.049f);
    else if (d < 200) bias = 45.75f + (d - 100.0f) * 0.12f;
    else if (d < 500) bias = 57.75f + (d - 200.0f) * 0.06f;
    else if (d < 1000)bias = 75.75f + (d - 500.0f) * 0.07f;
    else if (d < 2000)bias = 110.75f + (d - 1000.0f)* 0.09f;
    else if (d < 3000)bias = 200.75f - (d - 2000.0f)* 0.25f;
    else              bias = -49.25f;
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

// Dynamic outlier threshold ตามระยะ (หน่วย m)
float getDynamicOutlierThreshold(float d_m) {
  if (d_m <= 0.5f) return 0.5f;
  if (d_m <= 2.0f) return 0.7f;
  if (d_m <= 5.0f) return 1.0f;
  return 1.5f;
}

// ==================== UKF Class v2.1.4 - A1/A2 Like A3 ====================
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

    // Q เท่ากันทั้ง 3 anchors (ให้พฤติกรรมเหมือนกัน)
    Q.identity();
    Q.data[0][0] = 0.4f;  // ⬅️ A1: เปลี่ยนจาก 0.3 → 0.4 (เท่า A3)
    Q.data[1][1] = 0.4f;  // ⬅️ A2: เปลี่ยนจาก 0.5 → 0.4 (เท่า A3)
    Q.data[2][2] = 0.4f;  // A3: คงเดิม

    R.identity();
    R.data[0][0] = 0.003f;  // ⬅️ A1: เปลี่ยนจาก 0.001 → 0.003 (ใกล้ A3)
    R.data[1][1] = 0.0035f; // ⬅️ A2: เปลี่ยนจาก 0.002 → 0.0035 (ใกล้ A3)
    R.data[2][2] = 0.004f;  // A3: คงเดิม
  }

  // ==== Adaptive R: A1/A2 ใกล้เคียง A3 ====
  void updateAdaptiveR(float d1_cm, float d2_cm, float d3_cm) {
    R.identity();

    auto calcR = [](float dcm, float base, float k1, float k2, float maxR) {
      float r;
      if (dcm <= 50.0f) {
        r = base * 0.7f;  // stable mode
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

    // R base ใกล้เคียงกัน (0.003, 0.0035, 0.004)
    R.data[0][0] = calcR(d1_cm, 0.003f,  0.00005f, 0.0002f, 1.0f);  // A1
    R.data[1][1] = calcR(d2_cm, 0.0035f, 0.00005f, 0.00025f, 2.0f); // A2
    R.data[2][2] = calcR(d3_cm, 0.004f,  0.00006f, 0.0003f, 3.0f);  // A3
  }

  // ==== Adaptive Q: A1/A2/A3 ใช้ค่าเดียวกัน ====
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

    // ใช้ค่า Q เดียวกันทั้ง 3 anchors
    Q.data[0][0] = calcQ(d1_m, 0.4f, 3.0f, 12.0f);  // A1: เท่า A3
    Q.data[1][1] = calcQ(d2_m, 0.4f, 3.0f, 12.0f);  // A2: เท่า A3
    Q.data[2][2] = calcQ(d3_m, 0.4f, 3.0f, 12.0f);  // A3: คงเดิม
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

    // Selective Fading: ลดลงเล็กน้อย
    for (int i = 0; i < 3; i++) {
      float fading = 1.0f;
      if (x[i] > 2.0f)      fading = 1.01f;
      else if (x[i] > 0.5f) fading = 1.005f;
      P.data[i][i] *= fading;
    }
  }

  void update(float meas_A1, float meas_A2, float meas_A3) {
    updateAdaptiveR(meas_A1 * 100.0f, meas_A2 * 100.0f, meas_A3 * 100.0f);

    Vector3 z;
    z.set(meas_A1, meas_A2, meas_A3);

    // Innovation-based Q: ปิดทั้งหมด < 1.0 m
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
      if (x[i] < 1.0f) continue;  // ปิด < 1.0 m
      
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

  void getQR(float &q1, float &q2, float &q3, float &r1, float &r2, float &r3) {
    q1 = Q.data[0][0];
    q2 = Q.data[1][1];
    q3 = Q.data[2][2];
    r1 = R.data[0][0];
    r2 = R.data[1][1];
    r3 = R.data[2][2];
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

// ==================== ESP-NOW Packet Structure ====================
typedef struct __attribute__((packed)) {
  uint8_t  role;
  uint8_t  tag_id;
  uint8_t  reserved;
  uint8_t  a_count;
  uint32_t seq;
  uint32_t msec;
  int16_t  A1, A2, A3;
} espnow_pkt_t;

// ==================== Helper Functions (ESP-NOW / OLED / AT) ====================
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

void drawScreen(float raw_A1, float raw_A2, float raw_A3,
                float filtered_A1, float filtered_A2, float filtered_A3,
                float exec_time_us,
                float q1, float q2, float q3) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.printf("TAG-%d %s", TAG_ID, FW_VERSION);

  display.setCursor(0, 12);
  display.printf("A1 R%.2f F%.2f", raw_A1, filtered_A1);

  display.setCursor(0, 24);
  display.printf("A2 R%.2f F%.2f", raw_A2, filtered_A2);

  display.setCursor(0, 36);
  display.printf("A3 R%.2f F%.2f", raw_A3, filtered_A3);

  display.setCursor(0, 48);
  display.printf("Q:%.1f %.1f %.1f", q1, q2, q3);

  display.setCursor(0, 56);
  display.printf("UKF %.0fus", exec_time_us);

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
  display.printf("%s - UKF", FW_VERSION);

  display.setCursor(0, 40);
  display.print("A1/A2 Like A3");

  display.setCursor(0, 52);
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

  if (!ukf_initialized && A1_raw_m > 0.05f && A2_raw_m > 0.05f && A3_raw_m > 0.05f) {
    ukf.init(A1_raw_m, A2_raw_m, A3_raw_m);
    ukf_initialized = true;
    prev_A1_m = A1_raw_m;
    prev_A2_m = A2_raw_m;
    prev_A3_m = A3_raw_m;
    Serial.printf("[UKF] Init - %s A1/A2 Like A3!\n", FW_VERSION);
  }

  float A1_filtered_m = A1_raw_m;
  float A2_filtered_m = A2_raw_m;
  float A3_filtered_m = A3_raw_m;
  float exec_time_us   = 0.0f;

  float qval1 = 0, qval2 = 0, qval3 = 0;
  float rval1 = 0, rval2 = 0, rval3 = 0;

  if (ukf_initialized) {
    uint32_t t_start = micros();

    ukf.predict();
    ukf.update(A1_raw_m, A2_raw_m, A3_raw_m);

    uint32_t t_end = micros();
    exec_time_us = (float)(t_end - t_start);

    ukf.getState(A1_filtered_m, A2_filtered_m, A3_filtered_m);
    ukf.getQR(qval1, qval2, qval3, rval1, rval2, rval3);

    prev_A1_m = A1_filtered_m;
    prev_A2_m = A2_filtered_m;
    prev_A3_m = A3_filtered_m;

    Serial.printf(
      "[UKF] %.0fus | Raw:%.2f,%.2f,%.2f | Filt:%.2f,%.2f,%.2f | "
      "Q:%.1f,%.1f,%.1f | R:%.3f,%.3f,%.3f\n",
      exec_time_us,
      A1_raw_m, A2_raw_m, A3_raw_m,
      A1_filtered_m, A2_filtered_m, A3_filtered_m,
      qval1, qval2, qval3,
      rval1, rval2, rval3
    );
  }

  range_list[0] = (int)(A1_filtered_m * 100.0f);
  range_list[1] = (int)(A2_filtered_m * 100.0f);
  range_list[2] = (int)(A3_filtered_m * 100.0f);

  drawScreen(A1_raw_m, A2_raw_m, A3_raw_m,
             A1_filtered_m, A2_filtered_m, A3_filtered_m,
             exec_time_us, qval1, qval2, qval3);
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
  p.A1      = (int16_t)range_list[0];
  p.A2      = (int16_t)range_list[1];
  p.A3      = (int16_t)range_list[2];

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
  Serial.printf("[BOOT] TAG %s - A1/A2 Like A3\n", FW_VERSION);
  Serial.println("========================================");

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
  Serial.printf("[READY] TAG %s - A1/A2 Like A3\n", FW_VERSION);
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
    Serial.printf("[HEARTBEAT] Uptime: %lu ms | UKF: %s\n",
                  (unsigned long)millis(),
                  ukf_initialized ? "ACTIVE" : "WAITING");
    t_heartbeat = millis();
  }

  while (Serial.available()) {
    SERIAL_AT.write(Serial.read());
  }
}
