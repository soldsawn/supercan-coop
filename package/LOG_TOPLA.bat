@echo off
rem Cift tikla. Son 3 oyun logunu + coop.ini + DLL/EXE hash ozetini Masaustu'nde anonim .zip yapar.
cd /d "%~dp0"
set "GAME=%~dp0"
if exist "%GAME%supercan.exe" goto run
echo Loglarin toplanacagi SuperCan KOPYASININ Game klasorunu yaz ya da surukle-birak, sonra Enter:
echo   ornek: C:\SuperCanCoop\Game
set /p "GAME=> "
set "GAME=%GAME:"=%"
:run
if "%GAME:~-1%"=="\" set "GAME=%GAME:~0,-1%"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0log_topla.ps1" -Game "%GAME%"
echo.
pause
