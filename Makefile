ifeq ($(OS),Windows_NT)
    PLATFORM = Windows
else
    PLATFORM = $(shell uname -s)
endif

CONFIG ?= Debug

ifneq ($(MAKECMDGOALS),release)
    $(info Building for $(PLATFORM) in $(CONFIG))
endif

BUILD = build
GENERATED = build/Generated
TOOLS = build/Tools
SRC = src
ASSETS = assets

GAME_SOURCES = $(SRC)/game.c $(SRC)/game_png.c $(SRC)/game_ttf.c $(SRC)/game_rectangle_pack.c
HEADERS = $(wildcard $(SRC)/*.h) $(wildcard $(GENERATED)/*.h)

ifeq ($(PLATFORM),Windows)
    SHELL = cmd.exe

    OBJ = .obj
    EXE = .exe
    DLL = .dll
    CC = cl.exe
    LD = link.exe
    MKDIR = if not exist $(subst /,\,$1) mkdir $(subst /,\,$1)
    RUN_ASSET_PREPROCESS = $(subst /,\,$(ASSET_PREPROCESS))

    PLATFORM_SOURCES = $(SRC)/win32.c $(SRC)/win32_d3d12.c
    MSVC_SRC = $(SRC)/msvc.c
    RES_FILE = $(BUILD)/app.res
    OS_PREREQ = $(GENERATED)/BasicGeometryVS.h $(GENERATED)/BasicGeometryPS.h $(RES_FILE)

    CFLAGS = /nologo /W3 /GS- /I $(GENERATED)
    LDFLAGS = /nologo /NODEFAULTLIB /SUBSYSTEM:CONSOLE
    LIBS = kernel32.lib user32.lib d3d12.lib dxgi.lib dxguid.lib ole32.lib

    ifeq ($(CONFIG),Release)
        CFLAGS += /O2 /D RELEASE
        TARGET_OBJS = $(PLATFORM_OBJS) $(GAME_OBJS) $(MSVC_OBJ)
        BUILD_TARGETS = $(TARGET)
    else
        CFLAGS += /Od /Z7 /D DEBUG
        LDFLAGS += /DEBUG /INCREMENTAL:NO
        DLL_LDFLAGS = /nologo /NODEFAULTLIB /NOENTRY /DEBUG /INCREMENTAL:NO /EXPORT:UpdateAndRender /EXPORT:GetSoundSamples /DLL /PDB:$(BUILD)/GameDLL.pdb
        TARGET_OBJS = $(PLATFORM_OBJS) $(MSVC_OBJ)
        BUILD_TARGETS = $(GAME_DLL) $(TARGET)
    endif

    COMPILE_CMD = @$(CC) /c $(CFLAGS) /Fo$@ $<
    LINK_CMD = @$(LD) $^ $(LIBS) $(LDFLAGS) /OUT:$@
    DLL_LINK_CMD = @$(LD) $^ $(LIBS) $(DLL_LDFLAGS) /OUT:$@
    PREPROCESS_COMPILE = @$(CC) /nologo /W3 /GS- /Fe:$@ /Fo$(TOOLS)/ $<
endif

ifeq ($(PLATFORM),Linux)
    OBJ = .o
    EXE =
    DLL = .so
    CC = clang
    MKDIR = mkdir -p $1
    RUN_ASSET_PREPROCESS = $(ASSET_PREPROCESS)

    WAYLAND_PROTOCOLS = $(shell pkg-config --variable=pkgdatadir wayland-protocols)
    XDG_SHELL_XML = $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml

    PLATFORM_SOURCES = $(SRC)/linux.c $(GENERATED)/xdg-shell-client-protocol.c
    MSVC_SRC =
    RES_FILE =
    OS_PREREQ = $(GENERATED)/xdg-shell-client-protocol.h $(GENERATED)/xdg-shell-client-protocol.c

    CFLAGS = -Wall -Wextra -Wpedantic -Wno-strict-prototypes -g -I $(GENERATED)
    LDFLAGS =
    LIBS = -lwayland-client -lasound -pthread

    TARGET_OBJS = $(PLATFORM_OBJS) $(GAME_OBJS)
    BUILD_TARGETS = $(TARGET)

    COMPILE_CMD = @$(CC) -c $(CFLAGS) -o $@ $<
    LINK_CMD = @$(CC) $(CFLAGS) -o $@ $^ $(LIBS) $(LDFLAGS)
    PREPROCESS_COMPILE = @$(CC) -O2 -o $@ $<
endif

TARGET = $(BUILD)/Game$(EXE)
GAME_DLL = $(BUILD)/Game$(DLL)
ASSET_PREPROCESS = $(TOOLS)/AssetPreprocess$(EXE)

GAME_OBJS = $(patsubst $(SRC)/%.c, $(BUILD)/%$(OBJ), $(GAME_SOURCES))
MSVC_OBJ = $(if $(MSVC_SRC), $(patsubst $(SRC)/%.c, $(BUILD)/%$(OBJ), $(MSVC_SRC)),)

PLATFORM_OBJS = $(patsubst $(SRC)/%.c, $(BUILD)/%$(OBJ), $(filter $(SRC)/%, $(PLATFORM_SOURCES)))
PLATFORM_OBJS += $(patsubst $(GENERATED)/%.c, $(BUILD)/%$(OBJ), $(filter $(GENERATED)/%, $(PLATFORM_SOURCES)))

.PHONY: all release directories assets game

all: directories $(OS_PREREQ) assets game

release:
	@$(MAKE) --no-print-directory CONFIG=Release

directories:
	@$(call MKDIR,$(BUILD))
	@$(call MKDIR,$(GENERATED))
	@$(call MKDIR,$(TOOLS))


game: $(BUILD_TARGETS)

$(TARGET): $(TARGET_OBJS) $(RES_FILE)
	$(LINK_CMD)

$(GAME_DLL): $(GAME_OBJS) $(MSVC_OBJ)
	$(DLL_LINK_CMD)


$(BUILD)/%$(OBJ): $(SRC)/%.c $(HEADERS) | directories
	$(COMPILE_CMD)

$(BUILD)/%$(OBJ): $(GENERATED)/%.c $(HEADERS) | directories
	$(COMPILE_CMD)


assets: $(ASSET_PREPROCESS) $(GENERATED)/watermelon.png.h $(GENERATED)/arial.ttf.h

$(ASSET_PREPROCESS): $(SRC)/game_asset_preprocess.c | directories
	$(PREPROCESS_COMPILE)

$(GENERATED)/watermelon.png.h: $(ASSETS)/images/watermelon.png | $(ASSET_PREPROCESS) directories
	@$(RUN_ASSET_PREPROCESS) $< $@

$(GENERATED)/arial.ttf.h: $(ASSETS)/fonts/arial.ttf | $(ASSET_PREPROCESS) directories
	@$(RUN_ASSET_PREPROCESS) $< $@


$(GENERATED)/BasicGeometryVS.h: $(SRC)/hlsl/BasicGeometry.hlsl | directories
	@dxc.exe -T vs_6_6 -E VSMain -Fh $@ $<

$(GENERATED)/BasicGeometryPS.h: $(SRC)/hlsl/BasicGeometry.hlsl | directories
	@dxc.exe -T ps_6_6 -E PSMain -Fh $@ $<

$(BUILD)/app.res: $(SRC)/win32/app.rc | directories
	@rc.exe /nologo /fo $@ $<

$(GENERATED)/xdg-shell-client-protocol.h: | directories
	@wayland-scanner client-header "$(XDG_SHELL_XML)" $@

$(GENERATED)/xdg-shell-client-protocol.c: $(GENERATED)/xdg-shell-client-protocol.h | directories
	@wayland-scanner private-code "$(XDG_SHELL_XML)" $@
