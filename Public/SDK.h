#if !defined(SDK_H)
#define SDK_H

#include "Mem.h"
#include "Render.h"
#include "Types.h"

#if defined(WASM)
#define Export(Name) __attribute__((export_name(Name)))
#define Import(Name) __attribute__((import_module("env"), import_name("Print")))
#else
#define Export(Name)
#define Import(Name)
#endif

typedef struct State
{
    Bool IsInitialized;
} State;

// NOTE: `State` is the game state and `ExtraMem` is memory for allocating your own state struct if you need it.
typedef Void UpdateAndRenderFunction(State *State, Void *ExtraMem, RenderBuf *RenderBuf);
#define UpdateAndRender(Name) Export("UpdateAndRender") Void Name(State *State, Void *ExtraMem, RenderBuf *RenderBuf)

//
// NOTE: Function imports.
//

#if defined(WASM)

Import("Print") Void Print(const char *Ptr, Uint32 Len);
static inline Void PrintStr(const char *Str)
{
    Assert(Str);

    if (!Str)
    {
        return;
    }

    Print(Str, StrLen(Str));
}

#endif

#endif
