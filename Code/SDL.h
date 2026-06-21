#if !defined(SDL_H)
#define SDL_H

#include <SDL3/SDL.h>

#include "Render.h"
#include "Shared.h"
#include "Types.h"

#define CheckReturnBool(Function)                                            \
    if (!(Function))                                                         \
    {                                                                        \
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError()); \
        Assert(0)                                                            \
    }

#define CheckReturnPtr(Ptr)                                                  \
    if (!(Ptr))                                                              \
    {                                                                        \
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError()); \
        Assert(0)                                                            \
    }

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

#endif
