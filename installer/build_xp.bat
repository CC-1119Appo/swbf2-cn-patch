@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
cd /d "D:\Documents\DSH\swbf2_chs\installer"
cl /nologo /O2 /utf-8 /GS- installer_xp.cpp /link /OUT:installer_xp.exe user32.lib gdi32.lib comctl32.lib shell32.lib ole32.lib kernel32.lib
echo EXIT=%ERRORLEVEL%