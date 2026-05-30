ifeq ($(OS),Windows_NT)
    PLATFORM = Windows
else
    PLATFORM = $(shell uname -s)
endif

$(info Building for $(PLATFORM))

BUILD = build
GENERATED = build/Generated
TOOLS = build/Tools
SRC = src
ASSETS = assets

# NOTE: Windows configuration.
ifeq ($(PLATFORM),Windows)
    CC = cl.exe
    CFLAGS = /nologo /W3 /GS- /I $(GENERATED)
    LDFLAGS = /link /NODEFAULTLIB /SUBSYSTEM:WINDOWS
    LIBRARIES = kernel32.lib user32.lib d3d12.lib dxgi.lib dxguid.lib ole32.lib
    
    SOURCES = $(SRC)/win32.c $(SRC)/win32_d3d12.c $(SRC)/game_png.c $(SRC)/game_ttf.c $(SRC)/game_rectangle_pack.c
    
    TARGET = $(BUILD)/Game.exe
    ASSET_TOOL = $(TOOLS)/AssetPreprocess.exe

    MKDIR = if not exist $(subst /,\,$1) mkdir $(subst /,\,$1)
endif

# NOTE: Linux configuration.
ifeq ($(PLATFORM),Linux)
    CC = clang
    CFLAGS = -Wall -Wextra -Wpedantic -Wno-strict-prototypes -g -I $(GENERATED)
    LIBRARIES = -lwayland-client -lvulkan -lasound -pthread
    
    SOURCES = $(SRC)/linux.c $(SRC)/linux_vulkan.c $(GENERATED)/xdg-shell-client-protocol.c $(SRC)/game_png.c $(SRC)/game_ttf.c $(SRC)/game_rectangle_pack.c
    
    TARGET = $(BUILD)/Game
    ASSET_TOOL = $(TOOLS)/AssetPreprocess

    WAYLAND_PROTOCOLS = $(shell pkg-config --variable=pkgdatadir wayland-protocols)
    XDG_SHELL_XML = $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml

    MKDIR = mkdir -p $1
endif

all: directories os_prerequisites assets game

directories:
	@$(call MKDIR,$(BUILD))
	@$(call MKDIR,$(GENERATED))
	@$(call MKDIR,$(TOOLS))

# NOTE: Asset processing.

assets: $(ASSET_TOOL) $(GENERATED)/watermelon.png.h $(GENERATED)/arial.ttf.h

$(ASSET_TOOL): $(SRC)/game_asset_preprocess.c
ifeq ($(PLATFORM),Windows)
	@cl.exe /nologo /W3 /GS- /Fe:$@ /Fo$(TOOLS)/ $<

else # Linux

	@$(CC) -O2 -o $@ $<
endif # ($(PLATFORM),Windows)

$(GENERATED)/watermelon.png.h: $(ASSETS)/images/watermelon.png
	@$(ASSET_TOOL) $< $@

$(GENERATED)/arial.ttf.h: $(ASSETS)/fonts/arial.ttf
	@$(ASSET_TOOL) $< $@

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

os_prerequisites: $(GENERATED)/xdg-shell-client-protocol.h $(GENERATED)/xdg-shell-client-protocol.c $(GENERATED)/BasicGeometry.vert.h $(GENERATED)/BasicGeometry.frag.h

$(GENERATED)/xdg-shell-client-protocol.h:
	@wayland-scanner client-header "$(XDG_SHELL_XML)" $@

$(GENERATED)/xdg-shell-client-protocol.c: $(GENERATED)/xdg-shell-client-protocol.h
	@wayland-scanner private-code "$(XDG_SHELL_XML)" $@

$(GENERATED)/BasicGeometry.vert.h: $(SRC)/glsl/BasicGeometry.vert
	@glslc $< -o $(GENERATED)/BasicGeometry.vert.spv
	@$(ASSET_TOOL) $(GENERATED)/BasicGeometry.vert.spv $@
	@rm $(GENERATED)/BasicGeometry.vert.spv
	
$(GENERATED)/BasicGeometry.frag.h: $(SRC)/glsl/BasicGeometry.frag
	@glslc $< -o $(GENERATED)/BasicGeometry.frag.spv
	@$(ASSET_TOOL) $(GENERATED)/BasicGeometry.frag.spv $@
	@rm $(GENERATED)/BasicGeometry.frag.spv

endif # ($(PLATFORM),Windows)

# NOTE: Executable.

game: $(TARGET)

ifeq ($(PLATFORM),Windows)
$(TARGET): $(SOURCES) os_prerequisites assets
	@cl.exe $(CFLAGS) /Fo$(BUILD)/ /Fe:$@ $(SOURCES) $(BUILD)/app.res $(LIBRARIES) $(LDFLAGS)

else # Linux

$(TARGET): $(SOURCES) os_prerequisites assets
	@$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LIBRARIES)
endif # ($(PLATFORM),Windows)
