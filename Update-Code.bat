
@echo off
title Update Code from GitHub
color 0B

echo ==========================================
echo       Downloading latest code...
echo ==========================================
echo.

cd /d "%~dp0"

echo Pulling/Merging from GitHub...
git pull origin main

echo.
echo ==========================================
if %errorlevel% equ 0 (
    echo    SUCCESS! your code is up to date.
) else (
    echo    ERROR! Could not download.
)
echo ==========================================
echo.
timeout /t 5
