// TODO: Maybe this could be an actual proper interface
#if !defined(HOST_SDL_RENDERER_H)
#define HOST_SDL_RENDERER_H

#include <SDL3/SDL.h>

#include "Ent.h"
#include "Public/Types.h"

#define MaxTexs 512
// NOTE: Internal textures.
#define ReservedCircleTex MaxTexs - 1

typedef struct
{
    SDL_Renderer *SDL;
    // NOTE: Some textures are reserved internally.
    // See Reserved* macros.
    SDL_Texture *Texs[MaxTexs];
    Uint32 TexCount;

    Bool IsValid;
} Renderer;

// fuck
Renderer RendererInit(SDL_Window *Window);
Bool RendererDraw(World *World, Renderer *Renderer);

#endif
