/********************************************************************
  TAG SENDER v3.4.5 - Fixed AT+SETCFG Parameters (WORKING!)
  
  แก้ไขจาก v3.4.4:
  ✅ 1. แก้ AT+SETCFG parameter x3 จาก 1 (channel) เป็น 0 (air_speed 850K)
  ✅ 2. TAG สามารถใช้ ID=0 ได้ (ไม่ซ้ำกับ ANCHOR เพราะแยกด้วย ROLE)
  ✅ 3. เพิ่ม AT+SETCAP ให้ตรงกับ ANCHOR
  
  ผู้พัฒนา: สำหรับโครงการเรือไร้คนขับด้วย UWB Navigation
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
#define TAG_ID           9    // สำหรับ ESP-NOW
#define TAG_ID_FOR_UWB   0    // ✅ ใช้ 0 ได้เพราะแยกด้วย ROLE

#define I2C_SDA          39
#define I2C_SCL          38
#define IO_RXD2          18
#define IO_TXD2          17
#define UWB_RST          16

uint8_t NODE_PEER_MAC[6] = { 0x30, 0xED, 0xA0, 0x1F, 0x00, 0x24 };
static const uint8_t ESPNOW_CHANNEL = 11;

// ==================== UKF Configuration ====================
#define UKF_ALPHA   0.5
#define UKF_BETA    2.0
#define UKF_KAPPA   0.0

// ==================== Regression Configuration ====================
#define REGRESSION_WINDOW 10
#define REGRESSION_DEGREE 2

// ==================== Matrix และ Vector Classes ====================
struct Matrix3x3 {
  float data[3][3];
  void identity() {
    for(int i=0; i<3; i++)
      for(int j=0; j<3; j++)
        data[i][j] = (i==j) ? 1.0 : 0.0;
  }
  void scale(float s) {
    for(int i=0; i<3; i++)
      for(int j=0; j<3; j++)
        data[i][j] *= s;
  }
};

struct Vector3 {
  float data[3];
  void set(float a, float b, float c) {
    data[0]=a; data[1]=b; data[2]=c;
  }
  float& operator[](int i) { return data[i]; }
};

// ==================== Polynomial Regression Class ====================
class PolynomialRegression {
private:
  float buffer[REGRESSION_WINDOW];
  int buffer_count;
  int buffer_idx;
  
  void fitPolynomial(float &a, float &b, float &c) {
    if(buffer_count < 3) {
      a = getAverage();
      b = 0;
      c = 0;
      return;
    }
    
    float sum_x = 0, sum_x2 = 0, sum_x3 = 0, sum_x4 = 0;
    float sum_y = 0, sum_xy = 0, sum_x2y = 0;
    int n = buffer_count;
    
    for(int i = 0; i < n; i++) {
      float x = i;
      float y = buffer[i];
      sum_x += x;
      sum_x2 += x * x;
      sum_x3 += x * x * x;
      sum_x4 += x * x * x * x;
      sum_y += y;
      sum_xy += x * y;
      sum_x2y += x * x * y;
    }
    
    float det = n * (sum_x2 * sum_x4 - sum_x3 * sum_x3)
              - sum_x * (sum_x * sum_x4 - sum_x3 * sum_x2)
              + sum_x2 * (sum_x * sum_x3 - sum_x2 * sum_x2);
    
    if(abs(det) < 1e-6) {
      if(n * sum_x2 - sum_x * sum_x != 0) {
        b = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
        a = (sum_y - b * sum_x) / n;
      } else {
        a = getAverage();
        b = 0;
      }
      c = 0;
      return;
    }
    
    float det_a = sum_y * (sum_x2 * sum_x4 - sum_x3 * sum_x3)
                - sum_x * (sum_xy * sum_x4 - sum_x3 * sum_x2y)
                + sum_x2 * (sum_xy * sum_x3 - sum_x2 * sum_x2y);
    
    float det_b = n * (sum_xy * sum_x4 - sum_x3 * sum_x2y)
                - sum_y * (sum_x * sum_x4 - sum_x3 * sum_x2)
                + sum_x2 * (sum_x * sum_x2y - sum_xy * sum_x2);
    
    float det_c = n * (sum_x2 * sum_x2y - sum_xy * sum_x3)
                - sum_x * (sum_x * sum_x2y - sum_y * sum_x3)
                + sum_y * (sum_x * sum_x3 - sum_x2 * sum_x2);
    
    a = det_a / det;
    b = det_b / det;
    c = det_c / det;
  }
  
  float getAverage() {
    if(buffer_count == 0) return 0;
    float sum = 0;
    for(int i = 0; i < buffer_count; i++) {
      sum += buffer[i];
    }
    return sum / buffer_count;
  }
  
public:
  PolynomialRegression() {
    buffer_count = 0;
    buffer_idx = 0;
    for(int i = 0; i < REGRESSION_WINDOW; i++) {
      buffer[i] = 0;
    }
  }
  
  void addSample(float value) {
    buffer[buffer_idx] = value;
    buffer_idx = (buffer_idx + 1) % REGRESSION_WINDOW;
    if(buffer_count < REGRESSION_WINDOW) buffer_count++;
  }
  
  float predict() {
    if(buffer_count == 0) return 0;
    float a, b, c;
    fitPolynomial(a, b, c);
    float x = buffer_count;
    return a + b * x + c * x * x;
  }
  
  void reset() {
    buffer_count = 0;
    buffer_idx = 0;
  }
};

// ==================== Helper Functions ====================
float calculatePiecewiseBias(float raw_distance_cm, int anchor_id) {
  float d = raw_distance_cm;
  float bias = 0;
  
  if(anchor_id == 1) {
    if(d < 15) bias = 21.0 + (d - 10.0) * 0.6;
    else if(d < 25) bias = 24.0 + (d - 15.0) * 1.095;
    else if(d < 50) bias = 34.95 + (d - 25.0) * 0.388;
    else if(d < 100) bias = 44.65 + (d - 50.0) * 0.084;
    else if(d < 200) bias = 48.85 + (d - 100.0) * 0.0565;
    else if(d < 500) bias = 54.50 - (d - 200.0) * 0.0153;
    else if(d < 1000) bias = 49.90 + (d - 500.0) * 0.068;
    else if(d < 2000) bias = 83.90 + (d - 1000.0) * 0.1046;
    else if(d < 3000) bias = 188.50 - (d - 2000.0) * 0.2715;
    else bias = -83.0;
  }
  else if(anchor_id == 2) {
    float base_bias = calculatePiecewiseBias(d, 1);
    if(d < 100) bias = base_bias + 5.0 + d * 0.02;
    else if(d < 500) bias = base_bias + 7.0 + d * 0.015;
    else bias = base_bias + 40.0 + d * 0.008;
  }
  else {
    float base_bias = calculatePiecewiseBias(d, 1);
    if(d < 100) bias = base_bias + 2.0 + d * 0.03;
    else if(d < 500) bias = base_bias + 15.0 + d * 0.02;
    else bias = base_bias + 10.0 + d * 0.012;
  }
  
  return bias;
}

void choleskyDecomposition(const Matrix3x3 &A, Matrix3x3 &L) {
  for(int i=0; i<3; i++)
    for(int j=0; j<3; j++)
      L.data[i][j] = 0;
  
  for(int i=0; i<3; i++) {
    for(int j=0; j<=i; j++) {
      float sum = 0;
      if(j == i) {
        for(int k=0; k<j; k++)
          sum += L.data[j][k] * L.data[j][k];
        float val = A.data[j][j] - sum;
        L.data[j][j] = (val > 0) ? sqrt(val) : 0.001;
      } else {
        for(int k=0; k<j; k++)
          sum += L.data[i][k] * L.data[j][k];
        if(L.data[j][j] > 1e-9)
          L.data[i][j] = (A.data[i][j] - sum) / L.data[j][j];
        else
          L.data[i][j] = 0;
      }
    }
  }
}

// ==================== UKF Class ====================
class SimpleUKF {
private:
  Vector3 x;
  Matrix3x3 P, Q, R;
  float lambda, gamma, Wm0, Wc0, Wi;
  Vector3 sigmas[7], sigmas_pred[7];
  
public:
  SimpleUKF() {
    int n = 3;
    lambda = UKF_ALPHA*UKF_ALPHA * (n + UKF_KAPPA) - n;
    gamma = sqrt(n + lambda);
    Wm0 = lambda / (n + lambda);
    Wc0 = Wm0 + (1 - UKF_ALPHA*UKF_ALPHA + UKF_BETA);
    Wi = 1.0 / (2.0 * (n + lambda));
  }
  
  void init(float init_A1, float init_A2, float init_A3) {
    x.set(init_A1, init_A2, init_A3);
    P.identity(); P.scale(100.0);
    Q.identity();
    Q.data[0][0] = 0.5; Q.data[1][1] = 1.0; Q.data[2][2] = 0.7;
    R.identity();
    R.data[0][0] = 0.001; R.data[1][1] = 0.002; R.data[2][2] = 0.004;
  }
  
  void updateAdaptiveR(float d1_cm, float d2_cm, float d3_cm) {
    R.identity();
    if(d1_cm < 50) R.data[0][0] = 0.001;
    else if(d1_cm < 200) R.data[0][0] = 0.01;
    else if(d1_cm < 1000) R.data[0][0] = 0.5;
    else R.data[0][0] = 1.0;
    
    if(d2_cm < 50) R.data[1][1] = 0.002;
    else if(d2_cm < 200) R.data[1][1] = 0.02;
    else if(d2_cm < 1000) R.data[1][1] = 5.0;
    else R.data[1][1] = 2.0;
    
    if(d3_cm < 50) R.data[2][2] = 0.004;
    else if(d3_cm < 200) R.data[2][2] = 0.04;
    else if(d3_cm < 1000) R.data[2][2] = 6.0;
    else R.data[2][2] = 3.0;
  }
  
  void updateAdaptiveQ(float d1_m, float d2_m, float d3_m) {
    Q.identity();
    if(d1_m < 0.5) Q.data[0][0] = 0.5;
    else if(d1_m < 2.0) Q.data[0][0] = 2.0;
    else if(d1_m < 10.0) Q.data[0][0] = 25.0;
    else Q.data[0][0] = 100.0;
    
    if(d2_m < 0.5) Q.data[1][1] = 1.0;
    else if(d2_m < 2.0) Q.data[1][1] = 3.0;
    else if(d2_m < 10.0) Q.data[1][1] = 50.0;
    else Q.data[1][1] = 150.0;
    
    if(d3_m < 0.5) Q.data[2][2] = 0.7;
    else if(d3_m < 2.0) Q.data[2][2] = 2.5;
    else if(d3_m < 10.0) Q.data[2][2] = 40.0;
    else Q.data[2][2] = 120.0;
  }
  
  void generateSigmaPoints() {
    Matrix3x3 sqrtP;
    choleskyDecomposition(P, sqrtP);
    sigmas[0] = x;
    for(int i=0; i<3; i++) {
      sigmas[i+1].data[0] = x[0] + gamma * sqrtP.data[0][i];
      sigmas[i+1].data[1] = x[1] + gamma * sqrtP.data[1][i];
      sigmas[i+1].data[2] = x[2] + gamma * sqrtP.data[2][i];
    }
    for(int i=0; i<3; i++) {
      sigmas[i+4].data[0] = x[0] - gamma * sqrtP.data[0][i];
      sigmas[i+4].data[1] = x[1] - gamma * sqrtP.data[1][i];
      sigmas[i+4].data[2] = x[2] - gamma * sqrtP.data[2][i];
    }
  }
  
  void predict() {
    updateAdaptiveQ(x[0], x[1], x[2]);
    generateSigmaPoints();
    for(int i=0; i<7; i++) sigmas_pred[i] = sigmas[i];
    x.set(0,0,0);
    x[0] = Wm0 * sigmas_pred[0][0];
    x[1] = Wm0 * sigmas_pred[0][1];
    x[2] = Wm0 * sigmas_pred[0][2];
    for(int i=1; i<7; i++) {
      x[0] += Wi * sigmas_pred[i][0];
      x[1] += Wi * sigmas_pred[i][1];
      x[2] += Wi * sigmas_pred[i][2];
    }
    P.identity(); P.scale(0);
    for(int i=0; i<7; i++) {
      float w = (i==0) ? Wc0 : Wi;
      for(int j=0; j<3; j++) {
        for(int k=0; k<3; k++) {
          float diff_j = sigmas_pred[i][j] - x[j];
          float diff_k = sigmas_pred[i][k] - x[k];
          P.data[j][k] += w * diff_j * diff_k;
        }
      }
    }
    for(int i=0; i<3; i++) P.data[i][i] += Q.data[i][i];
    for(int i=0; i<3; i++) {
      float fading = 1.0;
      if(x[i] > 2.0) fading = 1.05;
      else if(x[i] > 0.5) fading = 1.02;
      for(int j=0; j<3; j++) P.data[i][j] *= fading;
    }
  }
  
  void update(float meas_A1, float meas_A2, float meas_A3) {
    updateAdaptiveR(meas_A1 * 100, meas_A2 * 100, meas_A3 * 100);
    Vector3 z; z.set(meas_A1, meas_A2, meas_A3);
    Vector3 innovation;
    innovation[0] = z[0] - x[0];
    innovation[1] = z[1] - x[1];
    innovation[2] = z[2] - x[2];
    for(int i=0; i<3; i++) {
      if(abs(innovation[i]) > 0.1) Q.data[i][i] *= 5.0;
      else if(abs(innovation[i]) > 0.05) Q.data[i][i] *= 2.0;
    }
    Vector3 z_pred; z_pred.set(0,0,0);
    for(int i=0; i<7; i++) {
      float w = (i==0) ? Wm0 : Wi;
      z_pred[0] += w * sigmas_pred[i][0];
      z_pred[1] += w * sigmas_pred[i][1];
      z_pred[2] += w * sigmas_pred[i][2];
    }
    Matrix3x3 Pzz; Pzz.identity(); Pzz.scale(0);
    for(int i=0; i<7; i++) {
      float w = (i==0) ? Wc0 : Wi;
      for(int j=0; j<3; j++) {
        for(int k=0; k<3; k++) {
          float diff_j = sigmas_pred[i][j] - z_pred[j];
          float diff_k = sigmas_pred[i][k] - z_pred[k];
          Pzz.data[j][k] += w * diff_j * diff_k;
        }
      }
    }
    for(int i=0; i<3; i++) Pzz.data[i][i] += R.data[i][i];
    Matrix3x3 Pxz; Pxz.identity(); Pxz.scale(0);
    for(int i=0; i<7; i++) {
      float w = (i==0) ? Wc0 : Wi;
      for(int j=0; j<3; j++) {
        for(int k=0; k<3; k++) {
          float diff_x = sigmas_pred[i][j] - x[j];
          float diff_z = sigmas_pred[i][k] - z_pred[k];
          Pxz.data[j][k] += w * diff_x * diff_z;
        }
      }
    }
    Matrix3x3 Pzz_inv; Pzz_inv.identity();
    for(int i=0; i<3; i++) {
      if(abs(Pzz.data[i][i]) > 1e-9)
        Pzz_inv.data[i][i] = 1.0 / Pzz.data[i][i];
      else Pzz_inv.data[i][i] = 0;
    }
    Matrix3x3 K;
    for(int i=0; i<3; i++) {
      for(int j=0; j<3; j++) {
        K.data[i][j] = 0;
        for(int k=0; k<3; k++)
          K.data[i][j] += Pxz.data[i][k] * Pzz_inv.data[k][j];
      }
    }
    Vector3 innov;
    innov[0] = z[0] - z_pred[0];
    innov[1] = z[1] - z_pred[1];
    innov[2] = z[2] - z_pred[2];
    for(int i=0; i<3; i++) {
      for(int j=0; j<3; j++)
        x[i] += K.data[i][j] * innov[j];
    }
    Matrix3x3 K_Pzz;
    for(int i=0; i<3; i++) {
      for(int j=0; j<3; j++) {
        K_Pzz.data[i][j] = 0;
        for(int k=0; k<3; k++)
          K_Pzz.data[i][j] += K.data[i][k] * Pzz.data[k][j];
      }
    }
    Matrix3x3 K_Pzz_Kt;
    for(int i=0; i<3; i++) {
      for(int j=0; j<3; j++) {
        K_Pzz_Kt.data[i][j] = 0;
        for(int k=0; k<3; k++)
          K_Pzz_Kt.data[i][j] += K_Pzz.data[i][k] * K.data[j][k];
      }
    }
    for(int i=0; i<3; i++)
      for(int j=0; j<3; j++)
        P.data[i][j] -= K_Pzz_Kt.data[i][j];
  }
  
  void getState(float &out_A1, float &out_A2, float &out_A3) {
    out_A1 = x[0]; out_A2 = x[1]; out_A3 = x[2];
  }
  
  void getQR(float &q1, float &q2, float &q3, float &r1, float &r2, float &r3) {
    q1 = Q.data[0][0];
    q2 = Q.data[1][1];
    q3 = Q.data[2][2];
    r1 = R.data[0][0];
    r2 = R.data[1][1];
    r3 = R.data[2][2];
  }
};

// ==================== Global Variables ====================
HardwareSerial SERIAL_AT(2);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

SimpleUKF ukf;
bool ukf_initialized = false;

PolynomialRegression regressor_A1, regressor_A2, regressor_A3;

String rxLine;
uint32_t seq_num = 0;
bool espnow_ready = false;
uint32_t last_range_time = 0;
float exec_time_us = 0;

typedef struct __attribute__((packed)) {
  uint8_t  role;
  uint8_t  tag_id;
  uint8_t  reserved;
  uint8_t  a_count;
  uint32_t seq;
  uint32_t msec;
  int16_t  A1_raw, A2_raw, A3_raw;
  int16_t  A1_ukf, A2_ukf, A3_ukf;
  int16_t  A1_reg, A2_reg, A3_reg;
} espnow_pkt_t;

struct {
  float raw[3];
  float corrected[3];
  float ukf_filtered[3];
  float reg_filtered[3];
} last_distances;

// ==================== Helper Functions ====================
static void getStaMac(uint8_t mac[6]) {
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA)==ESP_OK) {
    bool z=true; 
    for(int i=0;i<6;i++) if(mac[i]){z=false;break;}
    if(!z) return;
  }
  String s=WiFi.macAddress();
  if (s.length()>=17) { 
    int v[6]; 
    if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x", &v[0],&v[1],&v[2],&v[3],&v[4],&v[5])==6) { 
      for(int i=0;i<6;i++) mac[i]=(uint8_t)v[i]; 
      return; 
    } 
  }
  memset(mac,0,6);
}

static uint8_t getPrimaryChannel() { 
  uint8_t p=0; 
  wifi_second_chan_t s; 
  esp_wifi_get_channel(&p,&s); 
  return p; 
}

String sendAT(const String &cmd, uint32_t timeout_ms=500, bool echo=false) {
  String resp;
  if(echo) Serial.println(cmd);
  SERIAL_AT.println(cmd);
  uint32_t t0=millis();
  while(millis()-t0 < timeout_ms) {
    while(SERIAL_AT.available()) resp+=(char)SERIAL_AT.read();
  }
  return resp;
}

void updateOLED(const String &status) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.printf("TAG %d v3.4.5\n", TAG_ID);
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  display.setCursor(0,15);
  display.println(status);
  display.display();
}

void drawScreen(float rawA1, float rawA2, float rawA3,
                float ukfA1, float ukfA2, float ukfA3,
                float regA1, float regA2, float regA3,
                float exec_us, float q1, float q2, float q3) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  
  display.setCursor(0, 0);
  display.printf("TAG %d - UKF+Reg", TAG_ID);
  
  display.setCursor(0, 12);
  display.printf("A1:%.1f>%.1f>%.1f", rawA1, ukfA1, regA1);
  
  display.setCursor(0, 24);
  display.printf("A2:%.1f>%.1f>%.1f", rawA2, ukfA2, regA2);
  
  display.setCursor(0, 36);
  display.printf("A3:%.1f>%.1f>%.1f", rawA3, ukfA3, regA3);
  
  display.setCursor(0, 48);
  display.printf("UKF: %.0fus", exec_us);
  
  display.setCursor(0, 56);
  display.printf("Q:%.0f,%.0f,%.0f", q1, q2, q3);
  
  display.display();
}

void drawBootSplash() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("TAG");
  display.println(TAG_ID);
  
  display.setTextSize(1);
  display.setCursor(0, 24);
  display.println("v3.4.5 - FIXED");
  
  display.setCursor(0, 40);
  display.println("UKF+Regression");
  
  display.setCursor(0, 52);
  display.println("Initializing...");
  
  display.display();
}

void parseRange(const String &line) {
  Serial.print("[RX] ");
  Serial.println(line);
  
  String s = line;
  
  int atPos = s.indexOf("AT+");
  if (atPos >= 0) {
    s = s.substring(atPos + 3);
  }
  
  int plusPos = s.indexOf("+");
  if (plusPos >= 0) {
    s = s.substring(plusPos + 1);
  }
  
  if (!s.startsWith("RANGE")) {
    return;
  }
  
  s = s.substring(5);
  
  if (s.length() > 0 && s[0] == ',') {
    s = s.substring(1);
  }
  
  int idx1 = s.indexOf(',');
  if (idx1 < 0) {
    return;
  }
  
  int idx2 = s.indexOf(',', idx1 + 1);
  if (idx2 < 0) idx2 = s.length();
  
  int anchor_id = s.substring(0, idx1).toInt();
  int dist_mm = s.substring(idx1 + 1, idx2).toInt();
  
  if (anchor_id < 1 || anchor_id > 3) {
    return;
  }
  
  if (dist_mm <= 0) {
    return;
  }
  
  last_range_time = millis();
  
  float raw_cm = dist_mm / 10.0f;
  float bias_cm = calculatePiecewiseBias(raw_cm, anchor_id);
  float corrected_cm = raw_cm - bias_cm;
  
  int aidx = anchor_id - 1;
  last_distances.raw[aidx] = raw_cm;
  last_distances.corrected[aidx] = corrected_cm;
  
  if (anchor_id == 1) regressor_A1.addSample(corrected_cm);
  else if (anchor_id == 2) regressor_A2.addSample(corrected_cm);
  else if (anchor_id == 3) regressor_A3.addSample(corrected_cm);
  
  Serial.printf("[A%d] raw=%.1fcm corr=%.1fcm\n", anchor_id, raw_cm, corrected_cm);
  
  if (last_distances.corrected[0] > 0 &&
      last_distances.corrected[1] > 0 &&
      last_distances.corrected[2] > 0) {
    
    float A1_m = last_distances.corrected[0] / 100.0f;
    float A2_m = last_distances.corrected[1] / 100.0f;
    float A3_m = last_distances.corrected[2] / 100.0f;
    
    if (!ukf_initialized) {
      ukf.init(A1_m, A2_m, A3_m);
      ukf_initialized = true;
      Serial.println("[UKF] Initialized!");
    }
    
    uint32_t t_start = micros();
    ukf.predict();
    ukf.update(A1_m, A2_m, A3_m);
    uint32_t t_end = micros();
    exec_time_us = t_end - t_start;
    
    float ukf_A1, ukf_A2, ukf_A3;
    ukf.getState(ukf_A1, ukf_A2, ukf_A3);
    
    last_distances.ukf_filtered[0] = ukf_A1 * 100.0f;
    last_distances.ukf_filtered[1] = ukf_A2 * 100.0f;
    last_distances.ukf_filtered[2] = ukf_A3 * 100.0f;
    
    last_distances.reg_filtered[0] = regressor_A1.predict();
    last_distances.reg_filtered[1] = regressor_A2.predict();
    last_distances.reg_filtered[2] = regressor_A3.predict();
    
    float q1, q2, q3, r1, r2, r3;
    ukf.getQR(q1, q2, q3, r1, r2, r3);
    
    drawScreen(
      last_distances.corrected[0], last_distances.corrected[1], last_distances.corrected[2],
      last_distances.ukf_filtered[0], last_distances.ukf_filtered[1], last_distances.ukf_filtered[2],
      last_distances.reg_filtered[0], last_distances.reg_filtered[1], last_distances.reg_filtered[2],
      exec_time_us, q1, q2, q3
    );
  } else {
    last_distances.ukf_filtered[aidx] = corrected_cm;
    last_distances.reg_filtered[aidx] = corrected_cm;
  }
}

void onEspNowSent(const uint8_t *mac, esp_now_send_status_t status) {
}

void sendNow_AllData() {
  bool has_data = false;
  for(int i = 0; i < 3; i++) {
    if(last_distances.corrected[i] > 0) {
      has_data = true;
      break;
    }
  }
  
  if(!has_data) return;
  
  espnow_pkt_t pkt;
  pkt.role = 0;
  pkt.tag_id = TAG_ID;
  pkt.reserved = 0;
  pkt.a_count = 3;
  pkt.seq = seq_num++;
  pkt.msec = millis();
  
  pkt.A1_raw = (int16_t)last_distances.corrected[0];
  pkt.A2_raw = (int16_t)last_distances.corrected[1];
  pkt.A3_raw = (int16_t)last_distances.corrected[2];
  
  pkt.A1_ukf = (int16_t)last_distances.ukf_filtered[0];
  pkt.A2_ukf = (int16_t)last_distances.ukf_filtered[1];
  pkt.A3_ukf = (int16_t)last_distances.ukf_filtered[2];
  
  pkt.A1_reg = (int16_t)last_distances.reg_filtered[0];
  pkt.A2_reg = (int16_t)last_distances.reg_filtered[1];
  pkt.A3_reg = (int16_t)last_distances.reg_filtered[2];
  
  esp_now_send(NODE_PEER_MAC, (uint8_t*)&pkt, sizeof(pkt));
}

// ==================== Setup ====================
void setup() {
  pinMode(UWB_RST, OUTPUT);
  digitalWrite(UWB_RST, HIGH);
  
  Serial.begin(115200);
  delay(300);
  Serial.println("\n========================================");
  Serial.printf("[BOOT] TAG v3.4.5 (ID=%d)\n", TAG_ID);
  Serial.println("========================================");
  
  SERIAL_AT.begin(115200, SERIAL_8N1, IO_RXD2, IO_TXD2);
  
  Wire.begin(I2C_SDA, I2C_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[ERROR] OLED init failed!");
    while(1) delay(1000);
  }
  
  drawBootSplash();
  delay(1000);
  
  updateOLED("Testing UWB...");
  Serial.println("[SETUP] Testing UWB...");
  String resp = sendAT("AT?", 300);
  Serial.print("Response: "); Serial.println(resp);
  
  updateOLED("Restoring...");
  Serial.println("[SETUP] Restoring defaults...");
  resp = sendAT("AT+RESTORE", 1500);
  Serial.print("Response: "); Serial.println(resp);
  delay(500);
  
  updateOLED("Configuring...");
  
  // ✅ แก้ไข: AT+SETCFG=<id>,<role>,<air_speed>,<network>
  // Parameter x3 = 0 (air_speed 850K), ไม่ใช่ 1 (channel)!
  char cfg[40];
  sprintf(cfg, "AT+SETCFG=%d,0,0,1", TAG_ID_FOR_UWB);
  Serial.printf("[UWB] Config: ID=%d, ROLE=0 (TAG), AIR=850K, NET=1\n", TAG_ID_FOR_UWB);
  resp = sendAT(cfg, 800);
  Serial.print("Response: "); Serial.println(resp);
  
  // ✅ ต้องตรงกับ ANCHOR: AT+SETCAP=10,15 หรือ AT+SETCAP=4,10,1
  Serial.println("[UWB] Setting capacity...");
  resp = sendAT("AT+SETCAP=10,15", 500);
  Serial.print("Response: "); Serial.println(resp);
  
  Serial.println("[UWB] Enabling ranging report...");
  resp = sendAT("AT+SETRPT=1", 500);
  Serial.print("Response: "); Serial.println(resp);
  
  Serial.println("[UWB] Saving config...");
  resp = sendAT("AT+SAVE", 300);
  Serial.print("Response: "); Serial.println(resp);
  
  updateOLED("Restarting UWB...");
  Serial.println("[UWB] Restarting...");
  resp = sendAT("AT+RESTART", 1500);
  Serial.print("Response: "); Serial.println(resp);
  delay(1000);
  
  Serial.println("[UWB] Testing after restart...");
  resp = sendAT("AT?", 500);
  Serial.print("Response: "); Serial.println(resp);
  
  updateOLED("Init ESP-NOW...");
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  delay(50);
  
  uint8_t mac[6];
  getStaMac(mac);
  Serial.printf("[MAC] %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  
  if (esp_now_init()!=ESP_OK) {
    Serial.println("[ERROR] ESP-NOW init failed!");
    updateOLED("ESP-NOW FAIL!");
    while(1) delay(1000);
  }
  
  esp_now_register_send_cb(onEspNowSent);
  
  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, NODE_PEER_MAC, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.ifidx   = WIFI_IF_STA;
  peer.encrypt = false;
  
  if (esp_now_add_peer(&peer)==ESP_OK) {
    espnow_ready = true;
    Serial.println("[ESP-NOW] Ready!");
  } else {
    Serial.println("[ERROR] Failed to add peer");
    updateOLED("Peer FAIL!");
    while(1) delay(1000);
  }
  
  updateOLED("Ready! Waiting...");
  delay(1000);
  
  Serial.println("========================================");
  Serial.printf("[READY] TAG %d (UWB_ID=%d)\n", TAG_ID, TAG_ID_FOR_UWB);
  Serial.println("[INFO] Config: ID=0, ROLE=0, AIR=850K");
  Serial.println("[INFO] Waiting for ANCHOR...");
  Serial.println("========================================");
}

// ==================== Main Loop ====================
void loop() {
  static uint32_t t_uwb = 0;
  if (millis() - t_uwb > 200) {
    sendAT("AT+RDATA", 50);
    t_uwb = millis();
  }
  
  while (SERIAL_AT.available()) {
    char c = SERIAL_AT.read();
    if (c == '\n') {
      if (rxLine.indexOf("RANGE") >= 0) {
        parseRange(rxLine);
      }
      rxLine = "";
    } else if (c != '\r') {
      rxLine += c;
    }
  }
  
  static uint32_t t_espnow = 0;
  if (espnow_ready && millis() - t_espnow > 200) {
    sendNow_AllData();
    t_espnow = millis();
  }
  
  static uint32_t t_timeout = 0;
  if (millis() - t_timeout > 5000) {
    if (last_range_time == 0 || (millis() - last_range_time) > 5000) {
      Serial.println("[WARNING] No RANGE - check ANCHOR");
      updateOLED("No RANGE!\nCheck ANCHOR...");
    }
    t_timeout = millis();
  }
  
  while (Serial.available()) {
    SERIAL_AT.write(Serial.read());
  }
}
