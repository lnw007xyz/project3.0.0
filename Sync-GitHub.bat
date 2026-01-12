
@echo off
title Auto Sync to GitHub
color 0A

echo ==========================================
echo       Backing up code to GitHub...
echo ==========================================
echo.

cd /d "%~dp0"

echo [1/3] Adding all files...
git add .

echo [2/3] Saving changes (Commit)...
git commit -m "Auto update: %date% %time%"

echo [3/3] Uploading to Server (Push)...
git push origin main

echo.
echo ==========================================
if %errorlevel% equ 0 (
    echo    SUCCESS! Code is safe on GitHub.
) else (
    echo    ERROR! Something went wrong.
)
echo ==========================================
echo.
timeout /t 5
