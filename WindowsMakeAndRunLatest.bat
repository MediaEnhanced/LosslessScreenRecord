cls
@echo off

mingw32-make.exe ./bin/BitstreamFrameExtract.exe
if %errorlevel% neq 0 exit /b %errorlevel%
Start "Playback" ".\bin\BitstreamFrameExtract.exe"

echo.
exit 0
