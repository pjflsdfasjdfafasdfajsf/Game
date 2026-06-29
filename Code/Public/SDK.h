#if !defined(SDK_H)
#define SDK_H

#include "Ent.h"
#include "Input.h"
#include "Math.h"
#include "Mem.h"
#include "Types.h"

#if defined(WASM)
#define Export(Name) __attribute__((export_name(Name)))
#define Import(Name) __attribute__((import_module("env"), import_name(Name)))
#else
#define Export(Name)
#define Import(Name)
#endif

// NOTE: Initialization can be easily implemented with a single UpdateAndRender
// function but it would require exporting a resource such as GameInitialized
// which is rather ugly.
typedef Void InitFunction(Void);
#define Init(Name) Export("Init") Void Init(Void)

typedef Void UpdateAndRenderFunction(Void);
#define UpdateAndRender(Name) Export("UpdateAndRender") Void Name(Void)

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
// TODO: This could have been as part of SDK, but it is instead is implemented
// on host and imported here since there's one world for every module. It CAN
// be part of SDK but it requires a lot of fuckery.
//

Import("CompInit") CompTypeResult CompInit(Uint32 Hash, Usize Size);
Import("EntInit") EntResult EntInit(Void);

Import("EntAddComp") Bool EntAddComp(EntID EntID, CompID TypeID, const Void *Mem);
Import("EntGetComp") CompResult EntGetComp(EntID EntID, CompID TypeID);

Import("ResGetID") ResID ResGetID(const char *NamePtr, Usize NameLen);
Import("ResGetVal") Uint32 ResGetVal(ResID ResID);
Import("ResSetVal") Void ResSetVal(ResID ResID, Uint32 Val);

static inline ResID ResGetIDCStr(const char *Name)
{
    Assert(Name);

    if (!Name)
    {
        return ResID_Invalid;
    }

    return ResGetID(Name, CStrLen(Name));
}

//
// NOTE: Input
//

Import("GetInputState") InputState GetInputState(Input Input);

#endif
