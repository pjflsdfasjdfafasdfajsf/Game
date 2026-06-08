CC     := clang
GLSLC  := glslc


##################################################

EXE         := Game
EXE_SOURCES := src/SDL.c     \
               src/SDL_gpu.c
EXE_OBJECTS := $(EXE_SOURCES:.c=.o)

EXE_CFLAGS := -Wall -Wextra -Wpedantic -std=c23
EXE_LDLIBS := -lSDL3 -lm

DLL         := Game.so
DLL_SOURCES := src/game_png.c            \
               src/game_rectangle_pack.c \
               src/game_ttf.c            \
               src/game_wav.c            \
               src/game.c
DLL_OBJECTS := $(DLL_SOURCES:.c=.o)

DLL_CFLAGS := -Wall -Wextra -Wpedantic -fPIC -nostdlib -std=c23
DLL_LDLIBS := 

SHADER_SOURCES := src/shaders/regular.frag src/shaders/regular.vert
SHADER_OBJECTS := $(SHADER_SOURCES:%=%.spv)

##################################################

.PHONY: all
all: $(EXE) $(DLL)

$(EXE_OBJECTS): $(SHADER_OBJECTS)
$(EXE): $(EXE_OBJECTS)
	$(CC) $(EXE_CFLAGS) $^ -o $@ $(EXE_LDLIBS)

$(DLL): $(DLL_OBJECTS)
	$(CC) $(DLL_CFLAGS) -shared $^ -o $@ $(DLL_LDLIBS)

###

$(EXE_OBJECTS): CFLAGS = $(EXE_CFLAGS)
$(DLL_OBJECTS): CFLAGS = $(DLL_CFLAGS)

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