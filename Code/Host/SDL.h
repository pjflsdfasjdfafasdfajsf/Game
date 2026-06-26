#if !defined(SDL_H)
#define SDL_H

#include <SDL3/SDL.h>

#include "Runtime.h"

typedef struct
{
    Runtime Rt;
    char Path[1024];
    Int64 LastWriteTime;
    Void *ExtraMem;
} Mod;

typedef struct
{
    SDL_Window *Window;
    SDL_Renderer *Renderer;
    // NOTE: The game loading logic is basically the same as mods except for
    // the file that is being loaded. If all regular mods are loaded from Mods
    // directory the game is just hardcoded to be loaded from Game.wasm. Since
    // game is considered a mod too, Mods[0] will be always occupied by the
    // game. So the structure of game is something like:
    // Game/
    //     Game.exe
    //     Game.wasm
    //     Mods/
    //         ExampleMod.wasm
    Mod Mods[512];
    Uint32 ModCount;

    MemAlloc MemAlloc;
    RenderBuf RenderBuf;

    // NOTE: Textures
    SDL_Texture *Texs[512];
    Uint32 TexCount;

    State State;
} SDL;

// NOTE: On any failures all of these function just traps the process.

SDL Init();

Bool Poll(SDL *App);

Void Update(SDL *App);

Void Render(SDL *App);

#define LogCritical(Message, ...) SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s:%d (%s): " Message, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#endif
