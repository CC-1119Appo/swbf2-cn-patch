@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
cd /d "D:\Documents\DSH\swbf2_chs\tools"
cl /nologo /O2 /utf-8 /LD cn_hook.cpp overlay.cpp /link /OUT:cn_hook.dll user32.lib gdi32.lib
echo DLL_EXIT=%ERRORLEVEL%
cl /nologo /O2 /utf-8 launcher.cpp /link /OUT:SWBF2_CN.exe /SUBSYSTEM:WINDOWS user32.lib
echo EXE_EXIT=%ERRORLEVEL%