# TODO: Make incremental building work :(
ifeq ($(OS),Windows_NT)
    PLATFORM = Windows
else
    PLATFORM = $(shell uname -s)
endif

CONFIG ?= Debug

# NOTE: only print the info if we are not just forwarding to the release target since otherwise it would print:
#  Building for Windows in Debug
#  Building for Windows in Release
ifneq ($(MAKECMDGOALS),release)
    $(info Building for $(PLATFORM) in $(CONFIG))
endif

BUILD = build
GENERATED = build/Generated
TOOLS = build/Tools
SRC = src
ASSETS = assets

GAME_SOURCES = $(SRC)/game.c $(SRC)/game_png.c $(SRC)/game_ttf.c $(SRC)/game_rectangle_pack.c

# NOTE: Windows configuration.
ifeq ($(PLATFORM),Windows)
    SHELL = cmd.exe
    CC = cl.exe

    BASE_CFLAGS = /nologo /W3 /GS- /I $(GENERATED)
    BASE_LDFLAGS = /link /NODEFAULTLIB /SUBSYSTEM:CONSOLE
    LIBRARIES = kernel32.lib user32.lib d3d12.lib dxgi.lib dxguid.lib ole32.lib
    
    PLATFORM_SOURCES = $(SRC)/win32.c $(SRC)/win32_d3d12.c
    MSVC = $(SRC)/msvc.c

    TARGET = $(BUILD)/Game.exe
    GAME_DLL = $(BUILD)/Game.dll
    
    ASSET_PREPROCESS = $(TOOLS)/AssetPreprocess.exe
    RUN_ASSET_PREPROCESS = $(subst /,\,$(ASSET_PREPROCESS))

    MKDIR = if not exist $(subst /,\,$1) mkdir $(subst /,\,$1)

    ifeq ($(CONFIG),Release)
        CFLAGS = $(BASE_CFLAGS) /O2 /D RELEASE
        LDFLAGS = $(BASE_LDFLAGS)
        SOURCES = $(PLATFORM_SOURCES) $(GAME_SOURCES) $(MSVC)
    else
        CFLAGS = $(BASE_CFLAGS) /Od /Z7 /D DEBUG
        LDFLAGS = $(BASE_LDFLAGS) /DEBUG
        SOURCES = $(PLATFORM_SOURCES) $(MSVC)
        
        DLL_LDFLAGS = /link /NODEFAULTLIB /NOENTRY /DEBUG /EXPORT:UpdateAndRender /EXPORT:GetSoundSamples
    endif
endif

# NOTE: Linux configuration.
ifeq ($(PLATFORM),Linux)
    CC = clang
    CFLAGS = -Wall -Wextra -Wpedantic -Wno-strict-prototypes -g -I $(GENERATED)
    LIBRARIES = -lwayland-client -lasound -pthread
    
    PLATFORM_SOURCES = $(SRC)/linux.c $(GENERATED)/xdg-shell-client-protocol.c
    
    # TODO: Separate game and platform 
    SOURCES = $(PLATFORM_SOURCES) $(GAME_SOURCES)
    
    TARGET = $(BUILD)/Game
    ASSET_PREPROCESS = $(TOOLS)/AssetPreprocess
    RUN_ASSET_PREPROCESS = $(ASSET_PREPROCESS)

    WAYLAND_PROTOCOLS = $(shell pkg-config --variable=pkgdatadir wayland-protocols)
    XDG_SHELL_XML = $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml

    MKDIR = mkdir -p $1
endif

.PHONY: all release directories os_prerequisites assets game

all: directories os_prerequisites assets game

release:
	@$(MAKE) --no-print-directory CONFIG=Release

directories:
	@$(call MKDIR,$(BUILD))
	@$(call MKDIR,$(GENERATED))
	@$(call MKDIR,$(TOOLS))

# NOTE: Asset processing.

assets: $(ASSET_PREPROCESS) $(GENERATED)/watermelon.png.h $(GENERATED)/arial.ttf.h

$(ASSET_PREPROCESS): $(SRC)/game_asset_preprocess.c
ifeq ($(PLATFORM),Windows)
	@cl.exe /nologo /W3 /GS- /Fe:$@ /Fo$(TOOLS)/ $<
else # Linux
	@$(CC) -O2 -o $@ $<
endif # ($(PLATFORM),Windows)

$(GENERATED)/watermelon.png.h: $(ASSETS)/images/watermelon.png
	@$(RUN_ASSET_PREPROCESS) $< $@

$(GENERATED)/arial.ttf.h: $(ASSETS)/fonts/arial.ttf
	@$(RUN_ASSET_PREPROCESS) $< $@

# NOTE: OS-specific stuff.

ifeq ($(PLATFORM),Windows)

os_prerequisites: $(GENERATED)/BasicGeometryVS.h $(GENERATED)/BasicGeometryPS.h $(BUILD)/app.res

$(GENERATED)/BasicGeometryVS.h: $(SRC)/hlsl/BasicGeometry.hlsl
	@dxc.exe -T vs_6_6 -E VSMain -Fh $@ $<

$(GENERATED)/BasicGeometryPS.h: $(SRC)/hlsl/BasicGeometry.hlsl
	@dxc.exe -T ps_6_6 -E PSMain -Fh $@ $<

$(BUILD)/app.res: $(SRC)/win32/app.rc
	@rc.exe /nologo /fo $@ $<

else # Linux

os_prerequisites: $(GENERATED)/xdg-shell-client-protocol.h $(GENERATED)/xdg-shell-client-protocol.c

$(GENERATED)/xdg-shell-client-protocol.h:
	@wayland-scanner client-header "$(XDG_SHELL_XML)" $@

$(GENERATED)/xdg-shell-client-protocol.c: $(GENERATED)/xdg-shell-client-protocol.h
	@wayland-scanner private-code "$(XDG_SHELL_XML)" $@

endif # ($(PLATFORM),Windows)

# NOTE: Executable.

ifeq ($(PLATFORM),Windows)

ifeq ($(CONFIG),Release)

# NOTE: Release
game: $(TARGET)

$(TARGET): $(SOURCES) os_prerequisites assets
	@cl.exe $(CFLAGS) /Fo$(BUILD)/ /Fe:$@ $(SOURCES) $(BUILD)/app.res $(LIBRARIES) $(LDFLAGS)

else 

# NOTE: Debug
game: $(GAME_DLL) $(TARGET)

$(GAME_DLL): $(GAME_SOURCES) $(MSVC) os_prerequisites assets
	@cl.exe $(CFLAGS) /Fo$(BUILD)/ /Fe:$@ /LD $(GAME_SOURCES) $(MSVC) $(LIBRARIES) $(DLL_LDFLAGS)

$(TARGET): $(PLATFORM_SOURCES) $(MSVC) os_prerequisites assets
	@cl.exe $(CFLAGS) /Fo$(BUILD)/ /Fe:$@ $(PLATFORM_SOURCES) $(MSVC) $(BUILD)/app.res $(LIBRARIES) $(LDFLAGS)

endif # ($(CONFIG),Release)

else # Linux

game: $(TARGET)
$(TARGET): $(SOURCES) os_prerequisites assets
	@$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LIBRARIES)

endif # ($(PLATFORM),Windows)
