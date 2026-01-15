"""
Parse and organize new regression coefficients from compare_data.csv
Generate C++ arrays for Arduino code
"""

import pandas as pd
import numpy as np

# Read the CSV file
df = pd.read_csv('compare_data.csv')

# Filter out rows where new coefficients are NaN (range 5 - 3k+)
df_valid = df[df['new_lin_a'].notna()].copy()

# Initialize arrays for 6 ranges x 3 anchors
LINEAR_A_NEW = np.full((6, 3), 3000.0)
LINEAR_B_NEW = np.full((6, 3), 0.0)
POLY_P0_NEW = np.full((6, 3), 3000.0)
POLY_P1_NEW = np.full((6, 3), 0.0)
POLY_P2_NEW = np.full((6, 3), 0.0)

# Fill in the coefficients
for _, row in df_valid.iterrows():
    anchor_idx = int(row['anchor_idx'])
    range_idx = int(row['range'])
    
    LINEAR_A_NEW[range_idx][anchor_idx] = row['new_lin_a']
    LINEAR_B_NEW[range_idx][anchor_idx] = row['new_lin_b']
    POLY_P0_NEW[range_idx][anchor_idx] = row['new_poly_p0']
    POLY_P1_NEW[range_idx][anchor_idx] = row['new_poly_p1']
    POLY_P2_NEW[range_idx][anchor_idx] = row['new_poly_p2']

# Print C++ style arrays
print("// ==================== NEW REGRESSION COEFFICIENTS (v3.0.8) ====================")
print("// Based on regression_compare_old_vs_new_v3_0_7.xlsx - 'new' equations")
print()

print("const float LINEAR_A_NEW[6][3] = {")
for i in range(6):
    print(f"  {{{LINEAR_A_NEW[i][0]:.6f}, {LINEAR_A_NEW[i][1]:.6f}, {LINEAR_A_NEW[i][2]:.6f}}},")
print("};")
print()

print("const float LINEAR_B_NEW[6][3] = {")
for i in range(6):
    print(f"  {{{LINEAR_B_NEW[i][0]:.12f}, {LINEAR_B_NEW[i][1]:.12f}, {LINEAR_B_NEW[i][2]:.12f}}},")
print("};")
print()

print("const float POLY_P0_NEW[6][3] = {")
for i in range(6):
    print(f"  {{{POLY_P0_NEW[i][0]:.6f}, {POLY_P0_NEW[i][1]:.6f}, {POLY_P0_NEW[i][2]:.6f}}},")
print("};")
print()

print("const float POLY_P1_NEW[6][3] = {")
for i in range(6):
    print(f"  {{{POLY_P1_NEW[i][0]:.12f}, {POLY_P1_NEW[i][1]:.12f}, {POLY_P1_NEW[i][2]:.12f}}},")
print("};")
print()

print("const float POLY_P2_NEW[6][3] = {")
for i in range(6):
    print(f"  {{{POLY_P2_NEW[i][0]:.18f}, {POLY_P2_NEW[i][1]:.18f}, {POLY_P2_NEW[i][2]:.18f}}},")
print("};")
print()

print("// Comparison table (for reference):")
print("// Range | Anchor | OLD vs NEW Linear A | OLD vs NEW Poly P0")
for _, row in df_valid.iterrows():
    anchor = row['anchor']
    range_idx = int(row['range'])
    range_names = ["10-50cm", "50-100cm", "100-500cm", "500-1k", "1k-3k", "3k+"]
    print(f"// {range_names[range_idx]:10s} | {anchor} | OLD:{row['old_lin_a']:10.2f} NEW:{row['new_lin_a']:10.2f} | OLD:{row['old_poly_p0']:10.2f} NEW:{row['new_poly_p0']:10.2f}")

print("\n// ✓ Coefficients organized and ready for v3.0.8")
