@echo off
setlocal enabledelayedexpansion

:: Check for CMake
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo CMake is not found in PATH
    echo Please install CMake and add it to your PATH
    exit /b 1
)

:: Create build directories
if not exist "build-x86" mkdir build-x86
if not exist "build-x64" mkdir build-x64

:: Build 32-bit version
echo Building 32-bit version...
cd build-x86
cmake -A Win32 ..
if %ERRORLEVEL% neq 0 (
    echo Failed to configure 32-bit build
    exit /b 1
)
cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo Failed to build 32-bit version
    exit /b 1
)
cd ..

:: Build 64-bit version
echo Building 64-bit version...
cd build-x64
cmake -A x64 ..
if %ERRORLEVEL% neq 0 (
    echo Failed to configure 64-bit build
    exit /b 1
)
cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo Failed to build 64-bit version
    exit /b 1
)
cd ..

echo.
echo Build completed successfully!
echo.
echo Binaries can be found in:
echo   32-bit: build-x86\bin\Release\exe2dll_x86.exe
echo   64-bit: build-x64\bin\Release\exe2dll_x64.exe
echo.
