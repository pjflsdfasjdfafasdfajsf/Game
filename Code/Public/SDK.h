#if !defined(SDK_H)
#define SDK_H

#include "Mem.h"
#include "Render.h"
#include "Types.h"

#if defined(WASM)
#define Export(Name) __attribute__((export_name(Name)))
#define Import(Name) __attribute__((import_module("env"), import_name(Name)))
#else
#define Export(Name)
#define Import(Name)
#endif

typedef struct State
{
    MemAlloc PermanentAlloc;

    TexHandle SpriteAtlasTex;
    Atlas SpriteAtlas;

    Bool IsInitialized;
} State;

// NOTE: `State` is the game state and `ExtraMem` is memory for allocating your own state struct if you need it.
typedef Void UpdateAndRenderFunction(State *State, RenderBuf *RenderBuf, Void *ExtraMem);
#define UpdateAndRender(Name) Export("UpdateAndRender") Void Name(State *State, Void *ExtraMem, RenderBuf *RenderBuf)

//
// NOTE: Imports.
//

Import("PrintLine") Void PrintLine(const char *Ptr, Uint32 Len);
static inline Void PrintCStr(const char *Str)
{
    Assert(Str);

    if (!Str)
    {
        return;
    }

    PrintLine(Str, CStrLen(Str));
}

// TODO: This should just take in Memory, Size
//
// NOTE: It is highly recommend to minimize calls to this function. One of the
// ways you could do that is texture atlases, the whole Renderer API is pretty
// much built around it already. You can look in Example Mod for details.
//
// Supported image formats: PNG, JPEG, TGA, BMP, PSD, GIF, HDR, PIC.
Import("AllocTexture") TexHandle AllocTexture(const char *Path);

#endif
