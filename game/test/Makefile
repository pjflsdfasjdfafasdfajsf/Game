CC=clang
CFLAGS_TEST = -g -O1 -fsanitize=address,undefined -Wall -Wextra -Wpedantic -I../
CFLAGS_FUZZ = $(CFLAGS_TEST) -DFUZZING -fsanitize=fuzzer

BUILD_DIRECTORY = build
BIN_DIRECTORY   = $(BUILD_DIRECTORY)/bin

DATA_DIRECTORY = data

CORPUS_PNG = corpus_png
CORPUS_TTF = corpus_ttf

DICT_PNG = png.dict
DICT_TTF = ttf.dict

###

TEST_PNG_SOURCE = test_png.c
GAME_PNG_SOURCE = ../game_png.c
FUZZER_PNG_EXE  = $(BIN_DIRECTORY)/test_png_fuzzer
TEST_PNG_EXE    = $(BIN_DIRECTORY)/test_png
SEED_PNG        = $(DATA_DIRECTORY)/image.png

###

TEST_TTF_SOURCE = test_ttf.c
GAME_TTF_SOURCE = ../game_ttf.c ../game_rectangle_pack.c
FUZZER_TTF_EXE  = $(BIN_DIRECTORY)/test_ttf_fuzzer
TEST_TTF_EXE    = $(BIN_DIRECTORY)/test_ttf
SEED_TTF        = $(DATA_DIRECTORY)/font.ttf

FUZZING_TIME_IN_SECONDS ?= 900

########################################

$(TEST_PNG_EXE): $(TEST_PNG_SOURCE) $(GAME_PNG_SOURCE)
	@mkdir -p $(BIN_DIRECTORY)
	@$(CC) $(CFLAGS_TEST) -o $@ $^

$(TEST_TTF_EXE): $(TEST_TTF_SOURCE) $(GAME_TTF_SOURCE)
	@mkdir -p $(BIN_DIRECTORY)
	@$(CC) $(CFLAGS_TEST) -o $@ $^

########################################

$(FUZZER_PNG_EXE): $(TEST_PNG_SOURCE) $(GAME_PNG_SOURCE)
	@mkdir -p $(BIN_DIRECTORY)
	@$(CC) $(CFLAGS_FUZZ) -o $@ $^

$(FUZZER_TTF_EXE): $(TEST_TTF_SOURCE) $(GAME_TTF_SOURCE)
	@mkdir -p $(BIN_DIRECTORY)
	@$(CC) $(CFLAGS_FUZZ) -o $@ $^

########################################

$(CORPUS_PNG)/seed.png: $(SEED_PNG)
	@mkdir -p $(CORPUS_PNG)
	@cp $< $@

$(CORPUS_TTF)/seed.ttf: $(SEED_TTF)
	@mkdir -p $(CORPUS_TTF)
	@cp $< $@

$(DICT_PNG):
	@echo "header=\"\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\"" > $@
	@echo "\"IHDR\"" >> $@
	@echo "\"IDAT\"" >> $@
	@echo "\"IEND\"" >> $@
	@echo "\"PLTE\"" >> $@

$(DICT_TTF):
	@echo "\"\\x00\\x01\\x00\\x00\"" > $@
	@echo "\"true\"" >> $@
	@echo "\"typ1\"" >> $@
	@echo "\"OTTO\"" >> $@
	@echo "\"head\"" >> $@
	@echo "\"maxp\"" >> $@
	@echo "\"cmap\"" >> $@
	@echo "\"loca\"" >> $@
	@echo "\"glyf\"" >> $@

########################################

test-png: $(TEST_PNG_EXE)
	@$(TEST_PNG_EXE)

test-ttf: $(TEST_TTF_EXE)
	@$(TEST_TTF_EXE)

###

fuzz-png: $(FUZZER_PNG_EXE) $(CORPUS_PNG)/seed.png $(DICT_PNG)
	@$(FUZZER_PNG_EXE) $(CORPUS_PNG) -max_len=4096 -dict=$(DICT_PNG) -jobs=8 -workers=8 -max_total_time=$(FUZZING_TIME_IN_SECONDS)

fuzz-ttf: $(FUZZER_TTF_EXE) $(CORPUS_TTF)/seed.ttf $(DICT_TTF)
	@$(FUZZER_TTF_EXE) $(CORPUS_TTF) -max_len=65536 -dict=$(DICT_TTF) -jobs=8 -workers=8 -max_total_time=$(FUZZING_TIME_IN_SECONDS)

########################################

clean:
	@rm -rf $(BUILD_DIRECTORY)
	@rm -rf $(CORPUS_PNG) $(CORPUS_TTF)
	@rm -f $(DICT_PNG) $(DICT_TTF)
	@rm -f fuzz-*.log
	@rm -f png.ppm ttf.ppm
