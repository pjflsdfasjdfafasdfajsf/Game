# 
# NOTE: linux only!
# 
CC=clang
CFLAGS=-DDEBUG -Wall -Wextra -Wpedantic -fPIC -I$(GEN_DIRECTORY) -I$(GAME_DIRECTORY)

BUILD_DIRECTORY = build
BIN_DIRECTORY   = $(BUILD_DIRECTORY)/bin
OBJ_DIRECTORY   = $(BUILD_DIRECTORY)/obj
LIB_DIRECTORY   = $(BUILD_DIRECTORY)/lib
GEN_DIRECTORY   = $(BUILD_DIRECTORY)/gen

ASSETS_DIRECTORY = assets
GAME_DIRECTORY   = game
LINUX_DIRECTORY  = linux

PREPROCESSOR_SOURCE = $(GAME_DIRECTORY)/game_asset_preprocess.c
PREPROCESSOR_EXE    = $(BIN_DIRECTORY)/asset_preprocess

OUTPUT_EXECUTABLE = $(CC) $(CFLAGS) -o $@ $<
OUTPUT_OBJECT     = $(CC) $(CFLAGS) -o $@ -c $<

########################################
# NOTE: BUILD
########################################

# hush, make!
debug:
	@mkdir -p $(BUILD_DIRECTORY) \
		$(BIN_DIRECTORY) \
		$(OBJ_DIRECTORY) \
		$(LIB_DIRECTORY) \
		$(GEN_DIRECTORY)
	@$(MAKE) --no-print-directory targets


ASSETS =                                         \
	$(GEN_DIRECTORY)/arial.ttf.h                 \
	$(GEN_DIRECTORY)/watermelon.png.h            \
	$(GEN_DIRECTORY)/basic_geometry_vertex.spv.h \
	$(GEN_DIRECTORY)/basic_geometry_pixel.spv.h  \

GAME_SO = $(LIB_DIRECTORY)/Game.so
LINUX_EXE = $(BIN_DIRECTORY)/Game_Linux

targets: $(ASSETS) $(GAME_SO) $(LINUX_EXE)

########################################
# NOTE: ASSETS
########################################

$(PREPROCESSOR_EXE): $(PREPROCESSOR_SOURCE)
	@$(OUTPUT_EXECUTABLE)

$(GEN_DIRECTORY)/%.ttf.h: $(ASSETS_DIRECTORY)/fonts/%.ttf $(PREPROCESSOR_EXE)
	@$(PREPROCESSOR_EXE) $< $@

$(GEN_DIRECTORY)/%.png.h: $(ASSETS_DIRECTORY)/images/%.png $(PREPROCESSOR_EXE)
	@$(PREPROCESSOR_EXE) $< $@

###

# NOTE: our boys shaders are very special!
$(GEN_DIRECTORY)/basic_geometry_vertex.spv.h: $(LINUX_DIRECTORY)/shaders/basic_geometry.hlsl
	@dxc -spirv -T vs_6_6 -E VSMain $< -Fh $@ -Vn GlobalVertexShader

$(GEN_DIRECTORY)/basic_geometry_pixel.spv.h: $(LINUX_DIRECTORY)/shaders/basic_geometry.hlsl
	@dxc -spirv -T ps_6_6 -E PSMain $< -Fh $@ -Vn GlobalPixelShader

########################################
# NOTE: Game.so
########################################

GAME_OBJECTS =                             \
	$(OBJ_DIRECTORY)/game.o                \
	$(OBJ_DIRECTORY)/game_png.o            \
	$(OBJ_DIRECTORY)/game_rectangle_pack.o \
	$(OBJ_DIRECTORY)/game_ttf.o            \

$(GAME_OBJECTS): $(ASSETS)

$(GAME_SO) : $(GAME_OBJECTS)
	@$(CC) $(CFLAGS) -shared -o $@ $(GAME_OBJECTS)

###

$(OBJ_DIRECTORY)/game.o: $(GAME_DIRECTORY)/game.c
	@$(OUTPUT_OBJECT)
$(OBJ_DIRECTORY)/game_png.o: $(GAME_DIRECTORY)/game_png.c
	@$(OUTPUT_OBJECT)
$(OBJ_DIRECTORY)/game_rectangle_pack.o: $(GAME_DIRECTORY)/game_rectangle_pack.c
	@$(OUTPUT_OBJECT)
$(OBJ_DIRECTORY)/game_ttf.o: $(GAME_DIRECTORY)/game_ttf.c
	@$(OUTPUT_OBJECT)

########################################
# NOTE: WAYLAND
########################################

XDG_SHELL_XML = /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml
XDG_SHELL_H = $(GEN_DIRECTORY)/xdg-shell-client-protocol.h
XDG_SHELL_C = $(GEN_DIRECTORY)/xdg-shell-protocol.c

$(XDG_SHELL_H):
	@wayland-scanner client-header $(XDG_SHELL_XML) $@

$(XDG_SHELL_C):
	@wayland-scanner private-code $(XDG_SHELL_XML) $@

$(OBJ_DIRECTORY)/xdg-shell-protocol.o: $(XDG_SHELL_C)
	@$(OUTPUT_OBJECT)

########################################
# NOTE: Executable
########################################

LINUX_OBJECTS = \
	$(OBJ_DIRECTORY)/linux.o \
	$(OBJ_DIRECTORY)/linux_vulkan.o \
	$(OBJ_DIRECTORY)/xdg-shell-protocol.o

$(LINUX_OBJECTS): $(XDG_SHELL_H) $(ASSETS)

$(LINUX_EXE) : $(LINUX_OBJECTS)
	@$(CC) $(CFLAGS) -o $@ $(LINUX_OBJECTS) -lwayland-client -lasound

###
	
$(OBJ_DIRECTORY)/linux.o: $(LINUX_DIRECTORY)/linux.c
	@$(OUTPUT_OBJECT)
$(OBJ_DIRECTORY)/linux_vulkan.o: $(LINUX_DIRECTORY)/linux_vulkan.c
	@$(OUTPUT_OBJECT)

########################################
# NOTE: MISC
########################################

clean:
	@rm -rf $(BUILD_DIRECTORY)
