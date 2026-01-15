# Quick Reference: Regression Coefficients v3.0.8

## 📊 New Regression Equations

### Linear Regression (Type 1)
```
corrected_distance = A + B × raw_distance
```

### Polynomial Regression (Type 2)
```
corrected_distance = P0 + P1 × raw_distance + P2 × raw_distance²
```

---

## 📋 Coefficient Tables

### Anchor A1 (Index 0)

| Range | Distance | Linear A | Linear B | Poly P0 | Poly P1 | Poly P2 |
|-------|----------|----------|----------|---------|---------|---------|
| 0 | 10-50cm | -8.444 | 0.5967 | -35.528 | 2.1553 | -0.0221 |
| 1 | 50-100cm | -17.328 | 0.6982 | 3.098 | 0.1209 | 0.0039 |
| 2 | 100-500cm | -41.465 | 0.9592 | -25.342 | 0.7935 | 0.0004 |
| 3 | 500-1k | -9.864 | 0.9283 | -1713.639 | 6.2530 | -0.0040 |
| 4 | 1k-3k | -222.591 | 1.0769 | 623.122 | 0.0701 | 0.0003 |
| 5 | 3k+ | 3000.000 | 0.0000 | 3000.000 | 0.0000 | 0.0000 |

### Anchor A2 (Index 1)

| Range | Distance | Linear A | Linear B | Poly P0 | Poly P1 | Poly P2 |
|-------|----------|----------|----------|---------|---------|---------|
| 0 | 10-50cm | -19.540 | 0.8101 | -10.404 | 0.3716 | 0.0052 |
| 1 | 50-100cm | -9.381 | 0.5968 | -8.117 | 0.5608 | 0.0002 |
| 2 | 100-500cm | -22.956 | 0.8659 | -78.984 | 1.4605 | -0.0012 |
| 3 | 500-1k | -18.452 | 0.8475 | 1594.618 | -3.8212 | 0.0033 |
| 4 | 1k-3k | -1050.404 | 1.4450 | -1528.163 | 1.9276 | -0.0001 |
| 5 | 3k+ | 3000.000 | 0.0000 | 3000.000 | 0.0000 | 0.0000 |

### Anchor A3 (Index 2)

| Range | Distance | Linear A | Linear B | Poly P0 | Poly P1 | Poly P2 |
|-------|----------|----------|----------|---------|---------|---------|
| 0 | 10-50cm | -9.926 | 0.5969 | 20.321 | -0.9569 | 0.0195 |
| 1 | 50-100cm | -14.599 | 0.6649 | -15.613 | 0.6928 | -0.0002 |
| 2 | 100-500cm | -40.430 | 0.9530 | 27.474 | 0.2118 | 0.0017 |
| 3 | 500-1k | 239.122 | 0.5680 | 1110.884 | -2.0758 | 0.0019 |
| 4 | 1k-3k | -756.825 | 1.2881 | -649.214 | 1.1775 | 0.0000 |
| 5 | 3k+ | 3000.000 | 0.0000 | 3000.000 | 0.0000 | 0.0000 |

---

## 🎯 Usage Examples

### Example 1: A1, Raw Distance = 75cm (Range 1)

**Linear:**
```
corrected = -17.328 + 0.6982 × 75
         = -17.328 + 52.365
         = 35.037 cm
```

**Polynomial:**
```
corrected = 3.098 + 0.1209 × 75 + 0.0039 × 75²
         = 3.098 + 9.0675 + 21.94
         = 34.106 cm
```

### Example 2: A2, Raw Distance = 250cm (Range 2)

**Linear:**
```
corrected = -22.956 + 0.8659 × 250
         = -22.956 + 216.475
         = 193.519 cm
```

**Polynomial:**
```
corrected = -78.984 + 1.4605 × 250 + (-0.0012) × 250²
         = -78.984 + 365.125 - 75
         = 211.141 cm
```

### Example 3: A3, Raw Distance = 1500cm (Range 4)

**Linear:**
```
corrected = -756.825 + 1.2881 × 1500
         = -756.825 + 1932.15
         = 1175.325 cm
```

**Polynomial:**
```
corrected = -649.214 + 1.1775 × 1500 + 0.0000259 × 1500²
         = -649.214 + 1766.25 + 58.16
         = 1175.196 cm
```

---

## 🔍 How to Choose Range

```cpp
int getDistanceRange(float raw_cm) {
  if (raw_cm < 50.0f) return 0;        // 10-50cm
  else if (raw_cm < 100.0f) return 1;  // 50-100cm
  else if (raw_cm < 500.0f) return 2;  // 100-500cm
  else if (raw_cm < 1000.0f) return 3; // 500-1k
  else if (raw_cm < 3000.0f) return 4; // 1k-3k
  else return 5;                       // 3k+
}
```

---

## 💡 Tips

1. **Polynomial (Type 2) is recommended** - Generally more accurate
2. **Range 5 (3k+)** returns raw distance × 1 (no correction)
3. For **best accuracy**, always use the regression that matches your distance range
4. **Validation**: 
   - Minimum raw distance: 5cm
   - Maximum raw distance: 10000cm (beyond that, use previous value)

---

## 📈 Accuracy Improvements vs v3.0.7

| Range | Anchor | Improvement |
|-------|--------|-------------|
| 1 (50-100cm) | A1 | **82.6%** ✓✓✓ |
| 1 (50-100cm) | A2 | **97.9%** ✓✓✓ |
| 1 (50-100cm) | A3 | **88.8%** ✓✓✓ |
| 4 (1k-3k) | A1 | **98.1%** ✓✓✓ |
| 4 (1k-3k) | A2 | **58.1%** ✓✓ |
| 4 (1k-3k) | A3 | **86.3%** ✓✓✓ |

---

**Version**: 3.0.8  
**Date**: 2026-01-16  
**Source**: regression_compare_old_vs_new_v3_0_7.xlsx
