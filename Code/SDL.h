#if !defined(SDL_H)
#define SDL_H

#include <SDL3/SDL.h>

#include "Host.h"
#include "Render.h"
#include "Shared.h"
#include "Types.h"

typedef struct
{
    SDL_SharedObject *Handle;
    UpdateAndRenderFunction *AppUpdateAndRender;
    Int64 LastWriteTime;
} Code;

typedef struct
{
    SDL_Window *Window;
    SDL_Renderer *Renderer;

    Host Host;
    MemAlloc MemAlloc;
    RenderBuf RenderBuf;

    // NOTE: The loaded application DLL.
    Code Code;
} SDL;

// NOTE: On any failures all of these function just traps the process.

SDL Init();

Bool Poll();

Void Update(SDL *SDL);

Void Render(SDL *SDL);

#define LogCritical(Message, ...) SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, Message, ##__VA_ARGS__)

#endif
