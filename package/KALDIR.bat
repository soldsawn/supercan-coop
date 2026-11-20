@echo off
rem Cift tikla. SuperCan Co-op modunu kaldirir ve supercan.exe.orig yedegini geri yukler.
cd /d "%~dp0"
set "GAME=%~dp0"
if exist "%GAME%supercan.exe" goto run
echo Modun kaldirilacagi SuperCan KOPYASININ Game klasorunu yaz ya da surukle-birak, sonra Enter:
echo   ornek: C:\SuperCanCoop\Game
set /p "GAME=> "
set "GAME=%GAME:"=%"
:run
if "%GAME:~-1%"=="\" set "GAME=%GAME:~0,-1%"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0kur.ps1" -Game "%GAME%" -Uninstall
echo.
pause
