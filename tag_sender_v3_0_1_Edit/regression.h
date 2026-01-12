/*
 * =====================================================
 * REGRESSION.H - Regression Coefficients & Functions
 * =====================================================
 * ไฟล์นี้รวบรวมค่าสัมประสิทธิ์ Regression ทั้ง 3 ประเภท
 * และฟังก์ชันคำนวณค่า Regression
 * =====================================================
 */

#ifndef REGRESSION_H
#define REGRESSION_H

#include <Arduino.h>

// ==================== Regression Coefficients ====================
/*
 * ช่วงระยะ (Range Index):
 *   0 = 10-50 cm    (ระยะใกล้มาก)
 *   1 = 50-100 cm   (ระยะใกล้)
 *   2 = 100-500 cm  (ระยะกลาง)
 *   3 = 500-1000 cm (ระยะไกล)
 *   4 = 1000-3000 cm (ระยะไกลมาก)
 *   5 = 3000+ cm    (ระยะนอกขอบเขต)
 *
 * Anchor Index:
 *   0 = Anchor A1
 *   1 = Anchor A2
 *   2 = Anchor A3
 */

// ---------- Type 1: Linear Regression (RAW train) ----------
// สูตร: y = a + b*x
// LINEAR_A_RAW[range][anchor] = ค่า intercept (a)
// LINEAR_B_RAW[range][anchor] = ค่า slope (b)
const float LINEAR_A_RAW[6][3] = {
  {-7.920987, -11.788778, -10.975864},       // range 0: 10-50 cm
  {-45.320531, -150.276048, -72.250721},     // range 1: 50-100 cm
  {-46.127010, -6.897944, -48.541441},       // range 2: 100-500 cm
  {-9.863671, -18.451526, 238.863974},       // range 3: 500-1000 cm
  {16.207090, -1189.281891, -722.593423},    // range 4: 1000-3000 cm
  {3000.000000, 3000.000000, 3000.000000},   // range 5: 3000+ cm (fallback)
};

const float LINEAR_B_RAW[6][3] = {
  {0.560629874, 0.629175295, 0.614821903},   // range 0
  {1.011238899, 2.015586367, 1.267072780},   // range 1
  {0.974567451, 0.815575627, 0.979988595},   // range 2
  {0.928290530, 0.847455072, 0.568337768},   // range 3
  {0.906326793, 1.528761112, 1.266922277},   // range 4
  {0.000000000, 0.000000000, -0.000000000},  // range 5
};

// ---------- Type 2: Polynomial Regression (RAW train) ----------
// สูตร: y = p0 + p1*x + p2*x^2
// POLY_P0_RAW = constant term (p0)
// POLY_P1_RAW = linear term coefficient (p1)
// POLY_P2_RAW = quadratic term coefficient (p2)
const float POLY_P0_RAW[6][3] = {
  {0.506960, -22.550835, -4.121911},
  {-269.371748, -56.450537, -798.792456},
  {-30.914491, -84.559781, 79.446354},
  {-1713.639273, 1594.617826, 1095.019806},
  {1971.562486, -1591.815677, -1610.998913},
  {3000.000000, 3000.000000, 3000.000000},
};

const float POLY_P1_RAW[6][3] = {
  {0.232764752, 1.015710318, 0.353741242},
  {5.345956243, 0.212203772, 15.067108369},
  {0.838229897, 1.500325769, -0.219436726},
  {6.252989621, -3.821157546, -2.027975940},
  {-1.789529706, 2.002782068, 2.332959723},
  {0.000000000, 0.000000000, 0.000000000},
};

const float POLY_P2_RAW[6][3] = {
  {0.002877630304, -0.003227560187, 0.002288546413},
  {-0.020781949043, 0.008649595507, -0.065233037975},
  {0.000271495075, -0.001293928520, 0.002477209871},
  {-0.004048981892, 0.003318121619, 0.001904415423},
  {0.000823687047, -0.000134730616, -0.000303717763},
  {0.000000000000, 0.000000000000, 0.000000000000},
};

// ---------- Type 3: Linear Regression (Bias-corrected train) ----------
// สูตร: y = a + b*x_corrected
// ใช้สำหรับเปรียบเทียบ โดย x_corrected = raw - bias
const float LINEAR_A_BIASCORR[6][3] = {
  {14.922785, 19.044369, 16.907343},
  {0.404563, -53.644725, 33.645219},
  {4.479074, 42.143090, 17.636734},
  {5.973045, 32.817672, 254.863918},
  {62.409720, -1041.511129, -605.126993},
  {3000.000000, 3000.000000, 3000.000000},
};

const float LINEAR_B_BIASCORR[6][3] = {
  {0.688663616, 0.756355616, 0.772887050},
  {1.080779719, 2.419191474, 0.612479014},
  {0.977251735, 0.828020098, 1.006625052},
  {0.996019882, 0.917159169, 0.617758443},
  {0.944525786, 1.631358961, 1.325598606},
  {0.000000000, 0.000000000, -0.000000000},
};

// ==================== Range Helper Functions ====================

/**
 * getDistanceRange - แปลงระยะทาง (cm) เป็น range index
 * @param raw_cm: ระยะทางดิบ (เซนติเมตร)
 * @return: range index (0-5)
 */
inline int getDistanceRange(float raw_cm) {
  if (raw_cm < 50.0f) return 0;         // 10-50 cm
  else if (raw_cm < 100.0f) return 1;   // 50-100 cm
  else if (raw_cm < 500.0f) return 2;   // 100-500 cm
  else if (raw_cm < 1000.0f) return 3;  // 500-1000 cm
  else if (raw_cm < 3000.0f) return 4;  // 1000-3000 cm
  else return 5;                        // 3000+ cm
}

/**
 * getRangeLabel - แปลง range index เป็น string label
 * @param range: range index (0-5)
 * @return: string label สำหรับแสดงผล
 */
inline const char* getRangeLabel(int range) {
  switch (range) {
    case 0: return "10-50";
    case 1: return "50-100";
    case 2: return "100-500";
    case 3: return "500-1k";
    case 4: return "1k-3k";
    case 5: return "3k+";
    default: return "UNK";
  }
}

// ==================== Regression Calculation Functions ====================

/**
 * getRegressionValue_Type1 - คำนวณ Linear Regression (RAW train)
 * สูตร: y = a + b*x
 * @param raw_cm: ระยะทางดิบ (cm)
 * @param anchor_idx: index ของ anchor (0=A1, 1=A2, 2=A3)
 * @return: ระยะทางที่ผ่านการ regression
 */
inline float getRegressionValue_Type1(float raw_cm, int anchor_idx) {
  int range = getDistanceRange(raw_cm);  // หา range index จากระยะทาง
  // คำนวณ y = a + b*x
  return LINEAR_A_RAW[range][anchor_idx] + LINEAR_B_RAW[range][anchor_idx] * raw_cm;
}

/**
 * getRegressionValue_Type2 - คำนวณ Polynomial Regression (RAW train)
 * สูตร: y = p0 + p1*x + p2*x^2
 * @param raw_cm: ระยะทางดิบ (cm)
 * @param anchor_idx: index ของ anchor (0=A1, 1=A2, 2=A3)
 * @return: ระยะทางที่ผ่านการ regression
 */
inline float getRegressionValue_Type2(float raw_cm, int anchor_idx) {
  int range = getDistanceRange(raw_cm);  // หา range index
  float p0 = POLY_P0_RAW[range][anchor_idx];  // constant term
  float p1 = POLY_P1_RAW[range][anchor_idx];  // linear coefficient
  float p2 = POLY_P2_RAW[range][anchor_idx];  // quadratic coefficient
  // คำนวณ y = p0 + p1*x + p2*x^2
  return p0 + p1 * raw_cm + p2 * raw_cm * raw_cm;
}

// ==================== Bias Calculation (สำหรับ Type 3 เท่านั้น) ====================

/**
 * calculatePiecewiseBias - คำนวณค่า bias แบบ piecewise
 * ใช้สำหรับ Type 3 เพื่อแก้ค่าระยะทางก่อนทำ regression
 * @param raw_distance_cm: ระยะทางดิบ (cm)
 * @param anchor_id_1based: หมายเลข anchor (1, 2, หรือ 3)
 * @return: ค่า bias ที่ต้องลบออกจากระยะทางดิบ
 */
float calculatePiecewiseBias(float raw_distance_cm, int anchor_id_1based) {
  float d = raw_distance_cm;  // ระยะทางดิบ
  float bias = 0;             // ค่า bias เริ่มต้น

  // Anchor 1: ใช้เป็น base สำหรับคำนวณ bias
  if (anchor_id_1based == 1) {
    // แบ่งช่วงระยะทางและคำนวณ bias ตามสูตรเฉพาะของแต่ละช่วง
    if (d < 10)        bias = 21.0f;                          // < 10 cm
    else if (d < 15)   bias = 21.0f  + (d - 10.0f) * 0.6f;    // 10-15 cm
    else if (d < 25)   bias = 24.0f  + (d - 15.0f) * 1.095f;  // 15-25 cm
    else if (d < 50)   bias = 34.95f + (d - 25.0f) * 0.388f;  // 25-50 cm
    else if (d < 100)  bias = 44.65f + (d - 50.0f) * 0.084f;  // 50-100 cm
    else if (d < 200)  bias = 48.85f + (d - 100.0f) * 0.0565f; // 100-200 cm
    else if (d < 500)  bias = 54.50f - (d - 200.0f) * 0.0153f; // 200-500 cm
    else if (d < 1000) bias = 49.90f + (d - 500.0f) * 0.068f;  // 500-1000 cm
    else if (d < 2000) bias = 83.90f + (d - 1000.0f)* 0.1046f; // 1000-2000 cm
    else if (d < 3000) bias = 188.50f - (d - 2000.0f)* 0.2715f;// 2000-3000 cm
    else               bias = -83.0f;                          // >= 3000 cm
  } 
  // Anchor 2: bias = base_bias + offset ตามช่วงระยะ
  else if (anchor_id_1based == 2) {
    float base_bias = calculatePiecewiseBias(d, 1);  // เรียกซ้ำเพื่อหา base
    if (d < 100)       bias = base_bias + 5.0f  + d * 0.02f;
    else if (d < 500)  bias = base_bias + 7.0f  + d * 0.015f;
    else               bias = base_bias + 40.0f + d * 0.008f;
  } 
  // Anchor 3: bias = base_bias + offset ตามช่วงระยะ (รูปแบบต่างจาก A2)
  else {
    float base_bias = calculatePiecewiseBias(d, 1);  // เรียกซ้ำเพื่อหา base
    if (d < 100)       bias = base_bias + 2.0f  + d * 0.03f;
    else if (d < 500)  bias = base_bias + 15.0f + d * 0.02f;
    else               bias = base_bias + 10.0f + d * 0.012f;
  }

  return bias;  // คืนค่า bias
}

/**
 * getRegressionValue_Type3_BiasLinear - คำนวณ Bias-corrected Linear Regression
 * ขั้นตอน: 1) ลบ bias ออกจาก raw  2) ทำ linear regression
 * @param raw_cm: ระยะทางดิบ (cm)
 * @param anchor_idx: index ของ anchor (0=A1, 1=A2, 2=A3)
 * @return: ระยะทางที่ผ่านการ regression
 */
inline float getRegressionValue_Type3_BiasLinear(float raw_cm, int anchor_idx) {
  // แปลง anchor_idx (0-2) เป็น anchor_id (1-3) สำหรับ bias calculation
  float bias = calculatePiecewiseBias(raw_cm, anchor_idx + 1);
  float x_corr = raw_cm - bias;  // ลบ bias ออกจากค่าดิบ

  // ป้องกันค่าติดลบ (ขั้นต่ำ 5 cm)
  if (x_corr < 5.0f) x_corr = 5.0f;

  int range = getDistanceRange(raw_cm);  // ใช้ raw เลือกช่วงเพื่อความสอดคล้อง
  // คำนวณ y = a + b*x_corrected
  return LINEAR_A_BIASCORR[range][anchor_idx] + LINEAR_B_BIASCORR[range][anchor_idx] * x_corr;
}

#endif // REGRESSION_H
