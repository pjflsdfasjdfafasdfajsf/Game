#include "SDL.h"
#include "Runtime.h"
#include "Types.h"

//
// NOTE: Internal
//

#define GAME_WASM_FILE "Game.wasm"

static inline Int64 GetFileModTime(const char *Path)
{
    SDL_PathInfo Info;
    if (SDL_GetPathInfo(Path, &Info))
    {
        return Info.modify_time;
    }

    return 0;
}

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

// NOTE: UserData -- *SDL
static SDL_EnumerationResult SDLCALL EnumerateDirectoryCallback(Void *UserData, const char *DirName, const char *FileName)
{
    SDL *App = (SDL *)UserData;

    if (App->ModCount >= ArrayCount(App->Mods))
    {
        return SDL_ENUM_FAILURE;
    }

    Usize Len = SDL_strlen(FileName);
    if (Len > 5 && SDL_strcmp(FileName + Len - 5, ".wasm") == 0)
    {
        Mod *Mod = &App->Mods[App->ModCount];

        SDL_snprintf(Mod->Path, sizeof(Mod->Path), "%s%s", DirName, FileName);

        Mod->Rt = RtInit();
        if (Mod->Rt.IsValid && RtLoadOne(&Mod->Rt, Mod->Path))
        {
            Mod->LastWriteTime = GetFileModTime(Mod->Path);
            Mod->ExtraMem = SDL_calloc(1, Kb(16));

            if (Mod->ExtraMem)
            {
                App->ModCount++;
                SDL_Log("Loaded: %s\n", Mod->Path);
            }
            else
            {
                LogCritical("Out of memory!\n");
            }
        }
    }

    return SDL_ENUM_CONTINUE;
}

//
// NOTE: SDL
//

Void Render(SDL *App)
{
    Assert(App);

    RenderBufIterate(&App->RenderBuf, Cmd)
    {
        switch (Cmd->Type)
        {
        case RenderCommand_Clear:
        {
            RenderClear *Clear = (RenderClear *)Cmd;

            if (!SDL_SetRenderDrawColor(App->Renderer, Clear->Col.R, Clear->Col.G, Clear->Col.B, Clear->Col.A))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }
            if (!SDL_RenderClear(App->Renderer))
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

    SDL_RenderPresent(App->Renderer);
    RenderBufReset(&App->RenderBuf);
}

Void Update(SDL *App)
{
    Assert(App);

    for (Uint32 I = 0; I < App->ModCount; ++I)
    {
        Mod *Mod = &App->Mods[I];
        Int64 CurrentTime = GetFileModTime(Mod->Path);

        if (CurrentTime > Mod->LastWriteTime && CurrentTime)
        {
            SDL_Delay(50);
            SDL_Log("Reloading: %s\n", Mod->Path);

            // NOTE: Need to save ExtraMem since that's where state is stored
            if (Mod->Rt.ModuleInst && Mod->Rt.ExtraMem)
            {
                Void *NativeExtraMem = wasm_runtime_addr_app_to_native(Mod->Rt.ModuleInst, Mod->Rt.ExtraMem);
                if (NativeExtraMem)
                {
                    SDL_memcpy(Mod->ExtraMem, NativeExtraMem, Kb(16));
                }
            }

            RtDeinit(&Mod->Rt);
            Mod->Rt = RtInit();

            if (RtLoadOne(&Mod->Rt, Mod->Path))
            {
                Mod->LastWriteTime = CurrentTime;

                // NOTE: And restore the saved ExtraMem
                if (Mod->Rt.ModuleInst && Mod->Rt.ExtraMem)
                {
                    Void *NativeExtraMem = wasm_runtime_addr_app_to_native(Mod->Rt.ModuleInst, Mod->Rt.ExtraMem);
                    if (NativeExtraMem)
                    {
                        SDL_memcpy(NativeExtraMem, Mod->ExtraMem, Kb(16));
                    }
                }
            }
        }
        // NOTE: This handles the case where you got an error on compilation
        if (Mod->Rt.IsValid)
        {
            if (!RtUpdate(&Mod->Rt, &App->State, &App->RenderBuf))
            {
                Assert(0);
            }
        }
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

    //
    // NOTE: Game loading.
    //
    Mod *Game = &Result.Mods[0];
    if (!GetAbsPath(Game->Path, sizeof(Game->Path), GAME_WASM_FILE))
    {
        Assert(0);
    }

    Game->Rt = RtInit();
    if (!Game->Rt.IsValid || !RtLoadOne(&Game->Rt, Game->Path))
    {
        Assert(0);
    }

    Game->LastWriteTime = GetFileModTime(Game->Path);
    Game->ExtraMem = SDL_calloc(1, Kb(16));
    if (!Game->ExtraMem)
    {
        Assert(0);
    }
    Result.ModCount = 1;

    //
    // NOTE: Mod loading.
    //

    char ModsDir[1024];
    if (GetAbsPath(ModsDir, sizeof(ModsDir), "Mods/"))
    {
        SDL_CreateDirectory(ModsDir);
        if (!SDL_EnumerateDirectory(ModsDir, EnumerateDirectoryCallback, &Result))
        {
            LogCritical("%s", SDL_GetError());
        }
    }
    else
    {
        Assert(0);
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
