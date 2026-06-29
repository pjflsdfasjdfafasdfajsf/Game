# TODO: I am planning to replace this with Build.txt which will just be a command
# that game needs to invoke to get the .wasm flle
#
# So as you could have guessed this Makefile is temporary and has a bunch of lazy
# stuff.
CC  := clang

ifeq (, $(shell command -v $(CC) 2>/dev/null))
$(error "Your system is missing '$(CC)' compiler.")
endif

ROOT_DIR := ..
BUILD_DIR := Build

SDK_WASM_LIB := $(ROOT_DIR)/Build/libSDK_wasm.a
ZIP_CLI := $(ROOT_DIR)/Build/Tools/Zip

WASM_CFLAGS  := -I$(ROOT_DIR)/Code -I$(ROOT_DIR)/Code/Public --target=wasm32 -nostdlib
WASM_LDFLAGS := -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined

SRC      := ExampleMod.c
WASM_OUT := $(BUILD_DIR)/ExampleMod.wasm
MANIFEST := Manifest.txt
ZIP_OUT  := $(BUILD_DIR)/ExampleMod.zip

all: $(ZIP_OUT)

$(BUILD_DIR):
	@mkdir -p $@

$(WASM_OUT): $(SRC) $(SDK_WASM_LIB) | $(BUILD_DIR)
	$(CC) $(WASM_CFLAGS) $(SRC) -Wl,--whole-archive $(SDK_WASM_LIB) -Wl,--no-whole-archive $(WASM_LDFLAGS) -o $@

$(ZIP_OUT): $(ZIP_CLI) $(WASM_OUT) $(MANIFEST) | $(BUILD_DIR)
	$(ZIP_CLI) $@ $(WASM_OUT) $(MANIFEST)

$(ZIP_CLI):
	$(error "Run the main Makefile first")

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
