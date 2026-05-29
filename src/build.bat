@echo off

set CFLAGS=/nologo /W3 /Fe:Game.exe
set LIBS=user32.lib

cl.exe %CFLAGS% win32.c %LIBS% || exit /b

del win32.obj
