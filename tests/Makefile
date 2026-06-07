CC := clang

CFLAGS_TEST := -g -O1 -fsanitize=address,undefined -Wall -Wextra -Wpedantic -std=c23 -I../src 
CFLAGS_FUZZ := $(CFLAGS_TEST) -DFUZZING -fsanitize=fuzzer

BUILD_DIRECTORY := build
BIN_DIRECTORY   := $(BUILD_DIRECTORY)/bin
DATA_DIRECTORY  := data

FUZZING_TIME_IN_SECONDS ?= 900

ALL_CORPUS_DIRS :=

.PHONY: all
all:

########################################

# NOTE: shocking news macro addict finds out that Makefiles can define them

define TEST

all: $$(BIN_DIRECTORY)/test_$(1)

$$(BIN_DIRECTORY)/test_$(1): test_$(1).c $(4)
	@mkdir -p $$(BIN_DIRECTORY)
	$$(CC) $$(CFLAGS_TEST) -o $$@ $$^

$$(BIN_DIRECTORY)/test_$(1)_fuzzer: test_$(1).c $(4)
	@mkdir -p $$(BIN_DIRECTORY)
	$$(CC) $$(CFLAGS_FUZZ) -o $$@ $$^

corpus_$(1)/seed.$(1): $$(DATA_DIRECTORY)/$$(strip $(2))
	@mkdir -p corpus_$(1)
	@cp $$< $$@

.PHONY: test-$(1)
test-$(1): $$(BIN_DIRECTORY)/test_$(1)
	@$$<

.PHONY: fuzz-$(1)
fuzz-$(1): $$(BIN_DIRECTORY)/test_$(1)_fuzzer corpus_$(1)/seed.$(1)
	@$$< corpus_$(1) -max_len=$$(strip $(3)) -jobs=8 -workers=8 -max_total_time=$$(FUZZING_TIME_IN_SECONDS)

ALL_CORPUS_DIRS += corpus_$(1)

endef

########################################

$(eval $(call TEST,png, image.png,    4096,  ../src/game_png.c))
$(eval $(call TEST,ttf, font.ttf,     65536, ../src/game_ttf.c ../src/game_rectangle_pack.c))
$(eval $(call TEST,wav, sine_440.wav, 4096,  ../src/game_wav.c))

.PHONY: clean
clean:
	rm -rf $(BUILD_DIRECTORY)
	rm -rf $(ALL_CORPUS_DIRS)
	rm -f fuzz-*.log
	rm -f *.ppm *.raw