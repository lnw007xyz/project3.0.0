"""
Extract new regression coefficients from regression_compare_old_vs_new_v3_0_7.xlsx
"""

import pandas as pd
import json

# Read the Excel file
excel_file = "regression_compare_old_vs_new_v3_0_7.xlsx"
print(f"Reading Excel file: {excel_file}")

xls = pd.ExcelFile(excel_file)
print(f"Available sheets: {xls.sheet_names}")

# Read all sheets
for sheet_name in xls.sheet_names:
    print(f"\n{'='*60}")
    print(f"Sheet: {sheet_name}")
    print('='*60)
    df = pd.read_excel(excel_file, sheet_name=sheet_name)
    print(f"Shape: {df.shape}")
    print(f"\nColumns: {df.columns.tolist()}")
    print(f"\nFirst 30 rows:")
    print(df.head(30).to_string())
    
    # Save to CSV
    csv_file = f"{sheet_name.replace(' ', '_')}_data.csv"
    df.to_csv(csv_file, index=False, encoding='utf-8-sig')
    print(f"\n✓ Saved to: {csv_file}")

print("\n" + "="*60)
print("EXTRACTION COMPLETE")
print("="*60)
