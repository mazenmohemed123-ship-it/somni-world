@echo off
REM ============================================================
REM  SOMNI - one-click launcher for Windows
REM  Double-click this file, or run it from any folder.
REM  No C++ / CMake / DLLs needed - pure-Python backend is used
REM  automatically if the compiled kernel isn't built.
REM ============================================================

REM Move to the folder this script lives in (handles spaces in the path).
cd /d "%~dp0"

echo.
echo   Starting SOMNI...
echo.

REM Try the "py" launcher first (recommended on Windows), then "python".
where py >nul 2>nul
if %errorlevel%==0 (
    py somni.py
) else (
    python somni.py
)

echo.
echo   SOMNI has stopped. Press any key to close this window.
pause >nul
