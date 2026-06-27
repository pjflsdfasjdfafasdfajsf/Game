#if !defined(SDL_H)
#define SDL_H

#include <SDL3/SDL.h>

#include "Runtime.h"
#include "SDL_Renderer.h"
#include "Zip.h"

enum
{
    MouseButtonLeft = SDL_SCANCODE_COUNT + 0,
    MouseButtonRight = SDL_SCANCODE_COUNT + 1,
    MouseButtonMiddle = SDL_SCANCODE_COUNT + 2,
    KeysCount = SDL_SCANCODE_COUNT + 3,
};

typedef struct
{
    Runtime Rt;

    char Path[1024];

    Int64 LastWriteTime;
    Void *ExtraMem;

    const Uint8 *Mem;
    Usize Size;
    ZipArchive Archive;
} Mod;

typedef struct
{
    SDL_Window *Window;

    Renderer Renderer;
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

    Action *Keys[KeysCount];
    State State;
} SDL;

// NOTE: On any failures all of these function just traps the process.

SDL Init();

Bool Poll(SDL *App);
void Render(SDL *App);
Void Update(SDL *App);

#define LogCritical(Message, ...) SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s:%d (%s): " Message, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#endif
