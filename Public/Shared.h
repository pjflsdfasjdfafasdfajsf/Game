//
// NOTE: Shared definitions between SDL and the app.
//
#if !defined(SHARED_H)
#define SHARED_H

#include "Render.h"
#include "Types.h"

#if defined(WASM)
#define Export(Name) __attribute__((export_name(Name)))
#else
#define Export(Name)
#endif

typedef struct State
{
    Bool IsInitialized;
} State;

typedef Void UpdateAndRenderFunction(State *State, RenderBuf *RenderBuf);
#define UpdateAndRender(Name) Export("UpdateAndRender") Void Name(State *State, RenderBuf *RenderBuf)

#endif
