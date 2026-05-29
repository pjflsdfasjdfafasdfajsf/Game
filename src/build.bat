@echo off

if not exist generated mkdir generated

echo hlsl\BasicGeometry.hlsl
dxc.exe -T vs_6_6 -E VSMain -Fh BasicGeometryVS.h hlsl\BasicGeometry.hlsl
dxc.exe -T ps_6_6 -E PSMain -Fh BasicGeometryPS.h hlsl\BasicGeometry.hlsl

set CFLAGS=/nologo /W3 /GS- /Fe:Game.exe
set LDFLAGS=/link /NODEFAULTLIB /SUBSYSTEM:WINDOWS

set LIBRARIES=kernel32.lib user32.lib d3d12.lib dxgi.lib dxguid.lib ole32.lib
set SOURCES=win32.c win32_d3d12.c

REM Microsoft just cannot stop putting their stupidass headers everywhere.
rc.exe /nologo win32\app.rc || exit /b
cl.exe %CFLAGS% %SOURCES% win32\app.res %LIBRARIES% %LDFLAGS% || exit /b

del win32\app.res
del win32.obj
del win32_d3d12.obj
