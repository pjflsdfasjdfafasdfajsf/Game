#if !defined(SDK_H)
#define SDK_H

#include "Math.h"
#include "Mem.h"
#include "Types.h"
#include "Ent.h"

#if defined(WASM)
#define Export(Name) __attribute__((export_name(Name)))
#define Import(Name) __attribute__((import_module("env"), import_name(Name)))
#else
#define Export(Name)
#define Import(Name)
#endif

#define ExtraMemSize Mb(2)

// NOTE: `State` is the game state and `ExtraMem` is memory for allocating your own state struct if you need it.
typedef Void UpdateAndRenderFunction(Void *ExtraMem);
#define UpdateAndRender(Name) Export("UpdateAndRender") Void Name(Void *ExtraMem)

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

// NOTE: It is highly recommend to minimize calls to this function. One of the
// ways you could do that is texture atlases, the whole Renderer API is pretty
// much built around it already. You can look in Example Mod for details.
//
// Supported image formats: PNG, JPEG, TGA, BMP, PSD, GIF, HDR, PIC.
// Import("AllocTexture") TexHandle AllocTexture(const Void *Mem, Uint32 Size);
// NOTE: If DstPtr or DstSize is 0 this returns the files uncompressed size.
// Otherwise it returns the number of bytes read.
Import("ReadFile") Uint32 ReadFile(const char *PathPtr, Uint32 PathLen, Void *DstPtr, Uint32 DstSize);
static inline Uint32 ReadFileCStr(const char *Path, Void *Buf, Uint32 BufSize)
{
    Assert(Path);

    if (!Path)
    {
        return 0;
    }

    return ReadFile(Path, CStrLen(Path), Buf, BufSize);
}
static inline Uint32 GetFileSize(const char *Path)
{
    return ReadFileCStr(Path, 0, 0);
}

//
// NOTE: ECS
//

Import("AddComp") CompTypeResult AddComp(Usize Size);
Import("AddEnt") EntResult AddEnt(Void);

Import("EntAddComp") Void EntAddComp(Uint32 EntID, Uint32 TypeID, const Void *Mem);
Import("EntAddTransform") Void EntAddTransform(Uint32 EntID, CompTransform Transform);
Import("EntAddRenderable") Void EntAddRenderable(Uint32 EntID, CompRenderable Renderable);

Import("EntGetComp") CompResult EntGetComp(Uint32 EntID, Uint32 TypeID);

#endif
