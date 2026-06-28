CC  := clang
AR  := ar
ZIP ?= zip

ifeq (, $(shell command -v $(ZIP) 2>/dev/null))
$(error "Your system is missing '$(ZIP)' for game packaging.")
endif
ifeq (, $(shell command -v $(CC) 2>/dev/null))
$(error "Your system is missing '$(CC)' compiler.")
endif

# NOTE: Directories
BUILD_DIR     := Build
TOOLS_DIR     := $(BUILD_DIR)/Tools
TEST_DIR      := $(BUILD_DIR)/Test
TEST_DATA_DIR := $(TEST_DIR)/Data
OBJ_DIR       := $(BUILD_DIR)/Obj

# NOTE: Libraries
SDL3_LIB    := Ext/SDL3/Bin/Linux/libSDL3.a
WAMR_LIB    := Ext/WAMR/Bin/Linux/libiwasm.a
SYSTEM_LIBS := -lm -lpthread -ldl -lrt -lstdc++

# NOTE: Source groups
ATLAS_SRC   := Code/Host/AtlasPack.c Code/Host/STB.c
SDK_SRC     := Code/Public/Mem.c Code/Public/Math.c
HOST_SRC    := Code/Host/Main.c Code/Host/Runtime.c Code/Host/STB.c Code/Host/SDL_Renderer.c \
			   Code/Host/SDL.c Code/Host/Zip.c Code/Host/KeyValue.c Code/Host/Ent.c
TEST_SRC    := Code/Host/Zip_Test.c Code/Host/Zip.c
GAME_SRC    := Code/Game/Game.c

# NOTE: Objects
SDK_OBJS      := $(patsubst Code/Public/%.c, $(OBJ_DIR)/SDK/Native/%.o, $(SDK_SRC))
SDK_WASM_OBJS := $(patsubst Code/Public/%.c, $(OBJ_DIR)/SDK/WASM/%.o, $(SDK_SRC))
HOST_OBJS     := $(patsubst Code/Host/%.c, $(OBJ_DIR)/Host/%.o, $(HOST_SRC))
TEST_OBJS     := $(patsubst Code/Host/%.c, $(OBJ_DIR)/Test/%.o, $(TEST_SRC))

# NOTE: Global flags
CPPFLAGS := -ICode -IExt/WAMR/Include -IExt/SDL3/Include -I$(BUILD_DIR) -MMD -MP
# TODO: We need to have Release and Debug flags
CFLAGS   := -Wall -Wextra -g
LDFLAGS  := -no-pie
LDLIBS   := $(BUILD_DIR)/libSDK.a $(SDL3_LIB) $(WAMR_LIB) $(SYSTEM_LIBS)

# NOTE: Flags
ATLAS_CFLAGS := $(CFLAGS) -IExt/STB -O2
HOST_CFLAGS  := -IExt/STB
WASM_CFLAGS  := -ICode/Public --target=wasm32 -nostdlib
GAME_LDFLAGS := -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined

IMAGES := $(wildcard Assets/Images/*)

all: $(BUILD_DIR)/Game $(BUILD_DIR)/Game.zip Makefile

.PHONY: all clean test pack

-include $(SDK_OBJS:.o=.d)
-include $(SDK_WASM_OBJS:.o=.d)
-include $(HOST_OBJS:.o=.d)
-include $(TEST_OBJS:.o=.d)

#
# NOTE: Dirs
#
$(BUILD_DIR) $(TOOLS_DIR) $(TEST_DIR) $(TEST_DATA_DIR) \
$(OBJ_DIR)/SDK/WASM $(OBJ_DIR)/SDK/Native $(OBJ_DIR)/Host $(OBJ_DIR)/Test:
	@mkdir -p $@

Makefile:
	@ln -sf Linux.make Makefile

#
# NOTE: Atlas packer
#
$(TOOLS_DIR)/AtlasPack: $(ATLAS_SRC) | $(TOOLS_DIR)
	$(CC) $(ATLAS_CFLAGS) $(CPPFLAGS) $^ -lm -o $@

$(BUILD_DIR)/GameAtlas.png $(BUILD_DIR)/GameAtlas.txt &: $(TOOLS_DIR)/AtlasPack $(IMAGES) | $(BUILD_DIR)
	$(TOOLS_DIR)/AtlasPack $(BUILD_DIR)/GameAtlas Assets/Images/*

pack: $(BUILD_DIR)/GameAtlas.png $(BUILD_DIR)/GameAtlas.txt

#
# NOTE: SDK
#
$(OBJ_DIR)/SDK/Native/%.o: Code/Public/%.c | $(OBJ_DIR)/SDK/Native
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/libSDK.a: $(SDK_OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $^

$(OBJ_DIR)/SDK/WASM/%.o: Code/Public/%.c | $(OBJ_DIR)/SDK/WASM
	$(CC) $(WASM_CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/libSDK_wasm.a: $(SDK_WASM_OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $^

#
# NOTE: Host executable
#
$(OBJ_DIR)/Host/%.o: Code/Host/%.c | $(OBJ_DIR)/Host
	$(CC) $(CFLAGS) $(HOST_CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/Game: $(HOST_OBJS) $(BUILD_DIR)/libSDK.a $(BUILD_DIR)/GameAtlas.png $(BUILD_DIR)/GameAtlas.txt | $(BUILD_DIR)
	$(CC) $(LDFLAGS) $(HOST_OBJS) $(LDLIBS) -o $@

#
# NOTE: Game
#
$(BUILD_DIR)/Game.wasm: $(GAME_SRC) $(BUILD_DIR)/libSDK_wasm.a | $(BUILD_DIR)
	$(CC) $(WASM_CFLAGS) $(CPPFLAGS) $(GAME_SRC) -Wl,--whole-archive $(BUILD_DIR)/libSDK_wasm.a -Wl,--no-whole-archive $(GAME_LDFLAGS) -o $@

$(BUILD_DIR)/Game.zip: $(BUILD_DIR)/Game.wasm Code/Game/Manifest.txt $(BUILD_DIR)/GameAtlas.png $(BUILD_DIR)/GameAtlas.txt | $(BUILD_DIR)
	$(ZIP) -9 -j $@ $^ > /dev/null

#
# NOTE: Tests
# 
$(TEST_DATA_DIR)/Archive.zip: | $(TEST_DATA_DIR)
	@mkdir -p $(TEST_DATA_DIR)/Archive
	@echo "Hi i love u <3 AAAAAAANBBBBBCBCCCBCBCABAAAABCBCBCBABA" > $(TEST_DATA_DIR)/Archive/Hello.txt
	@echo "Imagine this is a cool binary" > $(TEST_DATA_DIR)/Archive/SomeAsset.bin
	@rm -f $@
	$(ZIP) -9 -r $@ $(TEST_DATA_DIR)/Archive >/dev/null

$(OBJ_DIR)/Test/%.o: Code/Host/%.c | $(OBJ_DIR)/Test
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(TEST_DIR)/Zip: $(TEST_OBJS) $(BUILD_DIR)/libSDK.a $(TEST_DATA_DIR)/Archive.zip | $(TEST_DIR)
	$(CC) $(LDFLAGS) $(TEST_OBJS) $(BUILD_DIR)/libSDK.a $(SDL3_LIB) $(SYSTEM_LIBS) -o $@

test: $(TEST_DIR)/Zip
	@./$(TEST_DIR)/Zip

clean:
	rm -rf $(BUILD_DIR)
