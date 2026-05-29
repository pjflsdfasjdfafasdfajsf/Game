@echo off

set CFLAGS=/nologo /W3 /GS- /Fe:Game.exe
set LDFLAGS=/link /NODEFAULTLIB /SUBSYSTEM:WINDOWS

set LIBRARIES=kernel32.lib user32.lib d3d12.lib dxgi.lib d3dcompiler.lib dxguid.lib mmdevapi.lib ole32.lib
set SOURCES=win32.c win32_d3d12.c

REM Microsoft just cannot stop putting their stupidass headers everywhere.
rc.exe /nologo win32\app.rc || exit /b
cl.exe %CFLAGS% %SOURCES% win32\app.res %LIBRARIES% %LDFLAGS% || exit /b

del win32\app.res
del win32.obj
del win32_d3d12.obj
