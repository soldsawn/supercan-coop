@echo off
rem Cift tikla. Bu klasor SuperCan kopyasinin Game klasorunun icindeyse orayi kurar, degilse Game klasorunu sorar.
cd /d "%~dp0"
set "GAME=%~dp0"
if exist "%GAME%supercan.exe" goto run
echo SuperCan KOPYASININ Game klasorunu yaz ya da klasoru bu pencereye surukle-birak, sonra Enter:
echo   ornek: C:\SuperCanCoop\Game
set /p "GAME=> "
set "GAME=%GAME:"=%"
:run
if "%GAME:~-1%"=="\" set "GAME=%GAME:~0,-1%"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0kur.ps1" -Game "%GAME%"
echo.
pause
