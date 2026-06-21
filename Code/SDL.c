#include "SDL.h"
#include "Mem.h"
#include "Render.h"
#include "SDL3/SDL_filesystem.h"
#include "Shared.h"

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

//
// NOTE: Intenral
//

// TODO: Maybe we could reduce code duplication if internal functions use proper error handling?

static inline Int64 GetFileModTime(const char *Path)
{
    SDL_PathInfo Info;
    if (SDL_GetPathInfo(Path, &Info))
    {
        return Info.modify_time;
    }

    return 0;
}

// NOTE: Usually I don't free anything but it makes sense to do it here as this is called on every hot reload.
static inline Bool CopyFile(const char *From, const char *To)
{
    SDL_RemovePath(To);

    Usize Size = 0;
    Void *Data = SDL_LoadFile(From, &Size);
    if (!Data)
    {
        // NOTE: Probably compiler being funny.
        return False;
    }

    SDL_IOStream *Out = SDL_IOFromFile(To, "wb");
    if (!Out)
    {
        SDL_free(Data);

        return False;
    }

    Usize Written = SDL_WriteIO(Out, Data, Size);
    SDL_CloseIO(Out);

    SDL_free(Data);

    return Written == Size;
}

// NOTE: This is needed so it does not matter where you launch the game from and
// it still finds the lib.
static inline Bool GetLibAbsPath(char *Buf, Usize Size, const char *Name)
{
    Assert(Buf);
    Assert(Size > 0);

    const char *BasePath = SDL_GetBasePath();
    if (!BasePath)
    {
        return False;
    }

    Int32 PathLen = SDL_snprintf(Buf, Size, "%s%s%s", BasePath, Name, DynamicLibrarySuffix);
    if (PathLen < 0 || (Usize)PathLen >= Size)
    {
        return False;
    }

    return True;
}

// NOTE: Code

static inline Void CodeUnload(Code *Code)
{
    if (Code->Handle)
    {
        SDL_UnloadObject(Code->Handle);
        Code->Handle = 0;
        Code->AppUpdateAndRender = 0;
    }
}

static inline Code CodeLoad(const char *Path, const char *TempPath)
{
    Code Result = {0};

    CheckReturnBool(CopyFile(Path, TempPath));

    CheckReturnPtr(Result.Handle = SDL_LoadObject(TempPath));
    CheckReturnPtr(Result.AppUpdateAndRender = (UpdateAndRenderFunction *)SDL_LoadFunction(Result.Handle, "UpdateAndRender"));

    Result.LastWriteTime = GetFileModTime(Path);

    return Result;
}

static inline Void CodeReload(Code *Code, const char *Path, const char *TempPath)
{
    Int64 CurrentTime = GetFileModTime(Path);

    if (CurrentTime > Code->LastWriteTime && CurrentTime)
    {
        CodeUnload(Code);
        // NOTE: Just in case.
        SDL_Delay(50);

        if (CopyFile(Path, TempPath))
        {
            Code->Handle = SDL_LoadObject(TempPath);
            if (Code->Handle)
            {
                Code->AppUpdateAndRender = (UpdateAndRenderFunction *)SDL_LoadFunction(Code->Handle, "UpdateAndRender");
                Code->LastWriteTime = CurrentTime;
            }
            else
            {
                SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
            }
        }
    }
}

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

            CheckReturnBool(SDL_SetRenderDrawColor(SDL->Renderer, Clear->Col.R, Clear->Col.G, Clear->Col.B, Clear->Col.A));
            CheckReturnBool(SDL_RenderClear(SDL->Renderer));
        }
        break;

        case RenderCommand_None:
        default:
        {
            SDL_LogCritical(SDL_LOG_CATEGORY_RENDER, "Unrecognized or not implemented render command (%d)", Cmd->Type);
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

    char Path[1024];
    char TempPath[1024];

    if (GetLibAbsPath(Path, sizeof(Path), DynamicLibraryName) && GetLibAbsPath(TempPath, sizeof(TempPath), DynamicLibraryName DynamicLibraryTempSuffix))
    {
        CodeReload(&SDL->Code, Path, TempPath);
    }

    if (SDL->Code.AppUpdateAndRender)
    {
        SDL->Code.AppUpdateAndRender(0, &SDL->RenderBuf);
    }
}

SDL Init()
{
    SDL Result = {0};

    CheckReturnBool(SDL_Init(SDL_INIT_VIDEO));
    CheckReturnBool(SDL_CreateWindowAndRenderer("Game", 1280, 720, 0, &Result.Window, &Result.Renderer));

    CheckReturnBool(SDL_SetRenderLogicalPresentation(Result.Renderer, 1280, 720, SDL_LOGICAL_PRESENTATION_LETTERBOX));

    Void *Mem = SDL_malloc(Kb(64));
    if (!Mem)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Out of memory\n");
        Assert(0);
    }

    Result.MemAlloc = MemAllocInit(Mem, Kb(64));

    Result.RenderBuf = RenderBufInit(&Result.MemAlloc, Kb(32));
    if (!Result.RenderBuf.IsValid)
    {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Could not initialize Render Buffer\n");
        Assert(0);
    }

    char Path[1024];
    char TempPath[1024];

    CheckReturnBool(GetLibAbsPath(Path, sizeof(Path), DynamicLibraryName));
    CheckReturnBool(GetLibAbsPath(TempPath, sizeof(TempPath), DynamicLibraryName DynamicLibraryTempSuffix));

    SDL_Log("Dynamic library path is: %s (Temp: %s)\n", Path, TempPath);

    Result.Code = CodeLoad(Path, TempPath);

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
