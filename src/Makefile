BUILD = ..\build
GENERATED = ..\build\Generated
TOOLS = ..\build\Tools

CFLAGS = /nologo /W3 /GS- /I $(GENERATED)
LDFLAGS = /link /NODEFAULTLIB /SUBSYSTEM:WINDOWS
LIBRARIES = kernel32.lib user32.lib d3d12.lib dxgi.lib dxguid.lib ole32.lib

SOURCES = win32.c win32_d3d12.c game_png.c game_ttf.c game_rectangle_pack.c


all: directories shaders assets game

directories:
	@if not exist $(BUILD) mkdir $(BUILD)
	@if not exist $(GENERATED) mkdir $(GENERATED)
	@if not exist $(TOOLS) mkdir $(TOOLS)


$(TOOLS)\AssetPreprocess.exe: game_asset_preprocess.c
	@cl.exe /nologo /W3 /GS- /Fe:$(TOOLS)\AssetPreprocess.exe /Fo$(TOOLS)\ game_asset_preprocess.c


shaders: $(GENERATED)\BasicGeometryVS.h $(GENERATED)\BasicGeometryPS.h

$(GENERATED)\BasicGeometryVS.h: hlsl\BasicGeometry.hlsl
	@dxc.exe -T vs_6_6 -E VSMain -Fh $(GENERATED)\BasicGeometryVS.h hlsl\BasicGeometry.hlsl

$(GENERATED)\BasicGeometryPS.h: hlsl\BasicGeometry.hlsl
	@dxc.exe -T ps_6_6 -E PSMain -Fh $(GENERATED)\BasicGeometryPS.h hlsl\BasicGeometry.hlsl


assets: $(TOOLS)\AssetPreprocess.exe $(GENERATED)\watermelon.png.h $(GENERATED)\arial.ttf.h

$(GENERATED)\watermelon.png.h: ..\assets\images\watermelon.png
	@$(TOOLS)\AssetPreprocess.exe ..\assets\images\watermelon.png $(GENERATED)\watermelon.png.h

$(GENERATED)\arial.ttf.h: ..\assets\fonts\arial.ttf
	@$(TOOLS)\AssetPreprocess.exe ..\assets\fonts\arial.ttf $(GENERATED)\arial.ttf.h


game: $(BUILD)\Game.exe

$(BUILD)\app.res: win32\app.rc
	@rc.exe /nologo /fo $(BUILD)\app.res win32\app.rc

$(BUILD)\Game.exe: $(SOURCES) $(BUILD)\app.res shaders assets
	@cl.exe $(CFLAGS) /Fo$(BUILD)\ /Fe:$(BUILD)\Game.exe $(SOURCES) $(BUILD)\app.res $(LIBRARIES) $(LDFLAGS)
