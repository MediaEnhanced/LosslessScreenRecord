cls
@echo off

mingw32-make.exe WindowsExecutables
if %errorlevel% neq 0 exit /b %errorlevel%
Start "Playback" ".\bin\VulkanVideoPlayback.exe"

echo.
exit 0
