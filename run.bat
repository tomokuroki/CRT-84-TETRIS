@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title CRT-84 TETRIS

if not exist "main.cpp" (
    echo ERROR: main.cpp is missing beside run.bat.
    echo Extract the whole ZIP archive into one folder first.
    pause
    exit /b 1
)

where g++ >nul 2>nul
if errorlevel 1 (
    echo ERROR: g++ compiler not found.
    echo Install MinGW-w64 or MSYS2 and add its bin folder to PATH.
    pause
    exit /b 1
)

echo Building compact CRT-84 TETRIS...
g++ -std=c++17 -Os -s -ffunction-sections -fdata-sections -Wl,--gc-sections -Wall -Wextra "main.cpp" -o "CRT84_Tetris.exe"
if errorlevel 1 (
    echo.
    echo Build failed. Check the compiler output above.
    pause
    exit /b 1
)
echo Starting game...
"CRT84_Tetris.exe"
