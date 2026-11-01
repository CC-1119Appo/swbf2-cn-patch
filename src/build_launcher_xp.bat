@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
cd /d "D:\Documents\DSH\swbf2_chs\tools"
cl /nologo /O2 /GS- /GR- /EHs- /utf-8 /D_WIN32_WINNT=0x0501 launcher_xp.cpp /link /NODEFAULTLIB /ENTRY:EntryPoint /SUBSYSTEM:WINDOWS,5.01 /OUT:SWBF2_CN_XP.exe user32.lib kernel32.lib
echo EXE_EXIT=%ERRORLEVEL%