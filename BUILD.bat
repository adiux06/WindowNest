@echo off
title Window Hider Builder
color 0A
cls

echo.
echo  =============================================
echo   WINDOW HIDER - AUTO BUILD
echo  =============================================
echo.

REM ── Find g++ by checking common MinGW install locations ──────────────────
set GXX=

REM Check PATH first
where g++ >nul 2>&1
if %errorlevel% equ 0 (
    set GXX=g++
    goto :found
)

REM Check common install locations
if exist "C:\mingw64\bin\g++.exe"             set GXX=C:\mingw64\bin\g++.exe
if exist "C:\mingw32\bin\g++.exe"             set GXX=C:\mingw32\bin\g++.exe
if exist "C:\MinGW\bin\g++.exe"               set GXX=C:\MinGW\bin\g++.exe
if exist "C:\msys64\mingw64\bin\g++.exe"      set GXX=C:\msys64\mingw64\bin\g++.exe
if exist "C:\msys64\mingw32\bin\g++.exe"      set GXX=C:\msys64\mingw32\bin\g++.exe
if exist "C:\TDM-GCC-64\bin\g++.exe"          set GXX=C:\TDM-GCC-64\bin\g++.exe
if exist "C:\TDM-GCC-32\bin\g++.exe"          set GXX=C:\TDM-GCC-32\bin\g++.exe

if not "%GXX%"=="" goto :found

echo  [ERROR] g++ compiler not found on this PC.
echo.
echo  Please install MinGW (TDM-GCC is easiest):
echo.
echo   1. Go to: https://jmeubank.github.io/tdm-gcc/
echo   2. Click "Download tdm64-gcc-..." and install it
echo   3. During install, make sure "Add to PATH" is checked
echo   4. Double-click BUILD.bat again after installing
echo.
pause
exit /b 1

:found
echo  [OK] Compiler: %GXX%
echo.

REM ── Check source file exists ─────────────────────────────────────────────
if not exist "%~dp0hidden.cpp" (
    echo  [ERROR] hidden.cpp not found!
    echo  Make sure hidden.cpp and BUILD.bat are in the SAME folder.
    echo  Current folder: %~dp0
    echo.
    pause
    exit /b 1
)

echo  [OK] Found: hidden.cpp
echo  Compiling... please wait...
echo.

REM ── Compile ──────────────────────────────────────────────────────────────
"%GXX%" "%~dp0hidden.cpp" -o "%~dp0hidden.exe" -mwindows -lcomctl32 -lgdi32 -luser32 -O2 -static-libgcc -static-libstdc++ 2>&1

set ERR=%errorlevel%
echo.

if %ERR% neq 0 (
    color 0C
    echo  =============================================
    echo   BUILD FAILED  ^(error %ERR%^)
    echo  =============================================
    echo  Screenshot the red errors above and share for help.
) else (
    color 0A
    echo  =============================================
    echo   SUCCESS! hidden.exe is ready!
    echo  =============================================
    echo.
    echo  Location: %~dp0hidden.exe
    echo.
    echo  Copy hidden.exe to any Windows PC and run it.
    echo  No installation or compiler needed on other PCs.
)

echo.
pause
