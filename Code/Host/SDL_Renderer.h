#include "Mem.h"
#if !defined(SDL_RENDERER_H)
#define SDL_RENDERER_H

#include <SDL3/SDL.h>

#include "Render.h"
#include "Types.h"

// NOTE: Internal textures.
#define ReservedCircleTex 0

typedef struct
{
    SDL_Renderer *SDL;
    // NOTE: Some textures are reserved internally.
    // See Reserved* macros.
    SDL_Texture *Texs[512];
    Uint32 TexCount;

    RenderBuf Buf;

    Bool IsValid;
} Renderer;

Renderer RendererInit(SDL_Window *Window, MemAlloc Alloc);
Bool RendererDraw(Renderer *Renderer);

#endif
