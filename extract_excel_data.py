"""
Extract raw data from the experimental Excel file and create a comprehensive
regression analysis CSV file with all calculations.
"""

import pandas as pd
import os

# Path to the Excel file
excel_file = "การทดลองวัดค่าความคลาดเคลื่อน.xlsx"

print(f"Reading Excel file: {excel_file}")
try:
    # Try to read all sheets
    excel_data = pd.ExcelFile(excel_file)
    print(f"Available sheets: {excel_data.sheet_names}")
    
    # Read the first sheet (or you can specify which sheet)
    df = pd.read_excel(excel_file, sheet_name=0)
    
    print(f"\nDataframe shape: {df.shape}")
    print(f"\nColumn names:")
    print(df.columns.tolist())
    
    print(f"\nFirst few rows:")
    print(df.head(10))
    
    # Save the raw data to CSV for inspection
    output_csv = "extracted_raw_data.csv"
    df.to_csv(output_csv, index=False, encoding='utf-8-sig')
    print(f"\n✓ Raw data extracted to: {output_csv}")
    
    # Now create the comprehensive regression analysis file
    print("\nCreating comprehensive regression analysis...")
    
except FileNotFoundError:
    print(f"ERROR: File '{excel_file}' not found in current directory")
    print(f"Current directory: {os.getcwd()}")
    print(f"Files in directory: {os.listdir('.')}")
except Exception as e:
    print(f"ERROR: {e}")
    import traceback
    traceback.print_exc()
