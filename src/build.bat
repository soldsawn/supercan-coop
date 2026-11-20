@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cd /d %~dp0
if not exist ..\local\build mkdir ..\local\build
cl /nologo /LD /O1 /MT /W3 /D_CRT_SECURE_NO_WARNINGS probe.c /Fe:..\local\build\dinput8.dll /Fo:..\local\build\ /link /DEF:probe.def user32.lib kernel32.lib ws2_32.lib
