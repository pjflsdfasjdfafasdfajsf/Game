#include "SDL.h"
#include "SDK.h"

#if defined(_WIN32)
#define DynamicLibrarySuffix ".dll"
#elif defined(__APPLE__)
#define DynamicLibrarySuffix ".dylib"
#elif defined(__linux__)
#define DynamicLibrarySuffix ".so"
#else
#error "Unsupported platform for compilation"
#endif
#define DynamicLibraryName "Game"
#define DynamicLibraryTempSuffix "_Temp"

#define DynamicLibraryFullName DynamicLibraryName DynamicLibrarySuffix
#define DynamicLibraryFullTempName DynamicLibraryName DynamicLibraryTempSuffix DynamicLibrarySuffix

//
// NOTE: Intenral
//

// static inline Int64 GetFileModTime(const char *Path)
// {
//     SDL_PathInfo Info;
//     if (SDL_GetPathInfo(Path, &Info))
//     {
//         return Info.modify_time;
//     }

//     return 0;
// }

// NOTE: This is needed so it does not matter where you launch the game from and
// it still finds the lib.
static inline Bool GetAbsPath(char *Buf, Usize Size, const char *Name)
{
    Assert(Buf);
    Assert(Name);
    Assert(Size > 0);

    const char *BasePath = SDL_GetBasePath();
    if (!BasePath)
    {
        return False;
    }

    Int32 PathLen = SDL_snprintf(Buf, Size, "%s%s", BasePath, Name);
    if (PathLen < 0 || (Usize)PathLen >= Size)
    {
        return False;
    }

    return True;
}

// // NOTE: Code

// static inline Void CodeUnload(Code *Code)
// {
//     if (Code->Handle)
//     {
//         SDL_UnloadObject(Code->Handle);
//         Code->Handle = 0;
//         Code->AppUpdateAndRender = 0;
//     }
// }

// static inline Code CodeLoad(const char *Path, const char *TempPath)
// {
//     Code Result = {0};

//     if (!SDL_CopyFile(Path, TempPath))
//     {
//         LogCritical("%s", SDL_GetError());
//         return Result;
//     }

//     Result.Handle = SDL_LoadObject(TempPath);
//     if (!Result.Handle)
//     {
//         LogCritical("%s", SDL_GetError());
//         return Result;
//     }

//     Result.AppUpdateAndRender = (UpdateAndRenderFunction *)SDL_LoadFunction(Result.Handle, "UpdateAndRender");
//     if (!Result.AppUpdateAndRender)
//     {
//         LogCritical("%s", SDL_GetError());
//         return Result;
//     }

//     Result.LastWriteTime = GetFileModTime(Path);
//     Result.IsValid = True;

//     return Result;
// }

// static inline Void CodeReload(Code *OldCode, const char *Path, const char *TempPath)
// {
//     Int64 CurrentTime = GetFileModTime(Path);

//     if (CurrentTime > OldCode->LastWriteTime && CurrentTime)
//     {
//         CodeUnload(OldCode);
//         // NOTE: Just in case.
//         SDL_Delay(50);

//         Code NewCode = CodeLoad(Path, TempPath);
//         if (!NewCode.IsValid)
//         {
//             return;
//         }

//         *OldCode = NewCode;
//     }
// }

//
// NOTE: SDL
//

Void Render(SDL *SDL)
{
    Assert(SDL);

    RenderBufIterate(&SDL->RenderBuf, Cmd)
    {
        switch (Cmd->Type)
        {
        case RenderCommand_Clear:
        {
            RenderClear *Clear = (RenderClear *)Cmd;

            if (!SDL_SetRenderDrawColor(SDL->Renderer, Clear->Col.R, Clear->Col.G, Clear->Col.B, Clear->Col.A))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }
            if (!SDL_RenderClear(SDL->Renderer))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }
        }
        break;

        case RenderCommand_None:
        default:
        {
            LogCritical("Unrecognized or not implemented render command (%d)", Cmd->Type);
            Assert(0);
        }
        break;
        }
    }

    SDL_RenderPresent(SDL->Renderer);
    RenderBufReset(&SDL->RenderBuf);
}

Void Update(SDL *SDL)
{
    Assert(SDL);

    // {
    //     char Path[1024];
    //     char TempPath[1024];

    //     if (GetAbsPath(Path, sizeof(Path), DynamicLibraryFullName) &&
    //         GetAbsPath(TempPath, sizeof(TempPath), DynamicLibraryFullTempName))
    //     {
    //         CodeReload(&SDL->Code, Path, TempPath);
    //     }
    // }

    // if (SDL->Code.AppUpdateAndRender)
    // {
    //     SDL->Code.AppUpdateAndRender(&SDL->State, &SDL->RenderBuf, SDL->ExtraMem);
    // }

    if (!HostUpdate(&SDL->Host, &SDL->State, &SDL->RenderBuf))
    {
        Assert(0);
    }
}

SDL Init()
{
    SDL Result = {0};

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        LogCritical("%s", SDL_GetError());
        Assert(0);
    }
    if (!SDL_CreateWindowAndRenderer("Game", 1280, 720, 0, &Result.Window, &Result.Renderer))
    {
        LogCritical("%s", SDL_GetError());
        Assert(0);
    }

    if (!SDL_SetRenderLogicalPresentation(Result.Renderer, 1280, 720, SDL_LOGICAL_PRESENTATION_LETTERBOX))
    {
        LogCritical("%s", SDL_GetError());
        Assert(0);
    }

    Void *Mem = SDL_malloc(Kb(64));
    if (!Mem)
    {
        Assert(0);
    }
    Result.MemAlloc = MemAllocInit(Mem, Kb(64));

    Result.RenderBuf = RenderBufInit(&Result.MemAlloc, Kb(32));
    if (!Result.RenderBuf.IsValid)
    {
        Assert(0);
    }

    Result.ExtraMem = SDL_calloc(1, Kb(16));
    if (!Result.ExtraMem)
    {
        Assert(0);
    }

    Result.Host = HostInit();
    if (!Result.Host.IsValid)
    {
        Assert(0);
    }

    {
        char Path[1024];

        if (!GetAbsPath(Path, sizeof(Path), "Game.wasm"))
        {
            LogCritical("%s", SDL_GetError());
            Assert(0);
        }

        if (!HostLoadOne(&Result.Host, Path))
        {
            Assert(0);
        }
    }

    return Result;
}

Bool Poll()
{
    SDL_Event Ev;
    while (SDL_PollEvent(&Ev))
    {
        switch (Ev.type)
        {
        case SDL_EVENT_QUIT:
        {
            return False;
        }
        break;
        }
    }

    return True;
}
