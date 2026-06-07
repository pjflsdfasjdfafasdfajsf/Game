CC     := clang
GLSLC  := glslc

CFLAGS := -Wall -Wextra -Wpedantic -fPIC -std=c23
LDLIBS := -lSDL3

##################################################

EXE         := Game
EXE_SOURCES := src/SDL.c     \
               src/SDL_gpu.c
EXE_OBJECTS := $(EXE_SOURCES:.c=.o)

DLL         := Game.so
DLL_SOURCES := src/game_png.c            \
               src/game_rectangle_pack.c \
               src/game_ttf.c            \
               src/game_wav.c            \
               src/game.c
DLL_OBJECTS := $(DLL_SOURCES:.c=.o)

SHADER_SOURCES := src/shaders/regular.frag src/shaders/regular.vert
SHADER_OBJECTS := $(SHADER_SOURCES:%=%.spv)

##################################################

.PHONY: all
all: $(EXE) $(DLL)

$(EXE_OBJECTS): $(SHADER_OBJECTS)
$(EXE): $(EXE_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

$(DLL): $(DLL_OBJECTS)
	$(CC) $(CFLAGS) -shared $^ -o $@

###

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

###

%.vert.spv: %.vert
	$(GLSLC) $< -o $@

%.frag.spv: %.frag
	$(GLSLC) $< -o $@

###

.PHONY: run
run: all
	./$(EXE)

.PHONY: clean
clean:
	rm -f $(EXE_OBJECTS) $(EXE)
	rm -f $(DLL_OBJECTS) $(DLL)
	rm -f $(SHADER_OBJECTS)
