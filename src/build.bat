@echo off

set CFLAGS=/nologo /W3 /GS- /Fe:Game.exe
set LDFLAGS=/link /NODEFAULTLIB /SUBSYSTEM:WINDOWS

set LIBS=kernel32.lib user32.lib d3d12.lib dxgi.lib d3dcompiler.lib dxguid.lib

cl.exe %CFLAGS% win32.c %LIBS% %LDFLAGS% || exit /b

del win32.obj
