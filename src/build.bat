@echo off

set CFLAGS=/nologo /W3 /GS- /Fe:Game.exe
set LDFLAGS=/link /NODEFAULTLIB /SUBSYSTEM:WINDOWS

set LIBRARIES=kernel32.lib user32.lib d3d12.lib dxgi.lib d3dcompiler.lib dxguid.lib
set SOURCES=win32.c win32_d3d12.c

cl.exe %CFLAGS% %SOURCES% %LIBRARIES% %LDFLAGS% || exit /b

del win32.obj
del win32_d3d12.obj
