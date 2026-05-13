@echo off
title Smart City System Build

echo ==========================================
echo Building Smart City System...
echo ==========================================
echo.

REM ==========================================
REM Remove old build folder
REM ==========================================
if exist build (
    echo Removing old build directory...
    rmdir /s /q build
)

REM ==========================================
REM Create build folder
REM ==========================================
mkdir build
cd build

echo.
echo Configuring CMake...
echo.

REM ==========================================
REM Configure Project
REM ==========================================
cmake -G "MinGW Makefiles" ^
-DCMAKE_C_COMPILER="C:/MinGW/bin/gcc.exe" ^
-DCMAKE_CXX_COMPILER="C:/MinGW/bin/g++.exe" ^
..

if %ERRORLEVEL% neq 0 (
    echo.
    echo CMake configuration failed!
    pause
    exit /b
)

echo.
echo Building project...
echo.

REM ==========================================
REM Build Project
REM ==========================================
mingw32-make

if %ERRORLEVEL% neq 0 (
    echo.
    echo Build failed!
    pause
    exit /b
)
cd ..
echo.
echo ==========================================
echo Build completed successfully!
echo ==========================================
echo.

echo Executables location:
echo build\bin\
echo or run all from run.bat
echo.

pause