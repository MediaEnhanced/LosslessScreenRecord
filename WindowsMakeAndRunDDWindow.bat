cls
@echo off

mingw32-make.exe ./bin/DesktopDuplicationWindow.exe
if %errorlevel% neq 0 exit /b %errorlevel%
Start "Playback" ".\bin\DesktopDuplicationWindow.exe"

echo.
exit 0
