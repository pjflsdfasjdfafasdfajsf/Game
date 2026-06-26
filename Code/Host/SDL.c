#include "SDL.h"
#include "Render.h"
#include "STB.h"
#include "wasm_export.h"

#define GAME_WASM_FILE "Game.wasm"

//
// NOTE: Host-provided funcs.
//

static Void HostPrintLine(wasm_exec_env_t ExecEnv, Uint32 PtrOffset, Uint32 Len)
{
    wasm_module_inst_t ModuleInst = wasm_runtime_get_module_inst(ExecEnv);
    if (!wasm_runtime_validate_app_str_addr(ModuleInst, PtrOffset))
    {
        LogCritical("Memory bounds violation.\n");
        return;
    }

    const char *Ptr = (const char *)wasm_runtime_addr_app_to_native(ModuleInst, PtrOffset);
    if (Ptr && Len > 0)
    {
        SDL_Log("%.*s\n", (Int32)Len, Ptr);
    }
}

static TexHandle HostAllocTexture(wasm_exec_env_t ExecEnv, Uint32 PtrOffset, Uint32 Size)
{
    wasm_module_inst_t ModuleInst = wasm_runtime_get_module_inst(ExecEnv);

    if (!wasm_runtime_validate_app_str_addr(ModuleInst, PtrOffset))
    {
        LogCritical("Memory bounds violation.\n");

        return TexHandleInvalid;
    }

    const Uint8 *Mem = (const Uint8 *)wasm_runtime_addr_app_to_native(ModuleInst, PtrOffset);
    if (!Mem || Size == 0)
    {
        LogCritical("Invalid pointer or size.\n");

        return TexHandleInvalid;
    }

    SDL_Log("Texture allocation request for %u bytes.\n", Size);

    SDL *App = (SDL *)wasm_runtime_get_custom_data(ModuleInst);
    Assert(App);

    if (App->TexCount >= ArrayCount(App->Texs))
    {
        LogCritical("Too many textures.\n");

        return TexHandleInvalid;
    }

    Int32 Width = 0;
    Int32 Height = 0;
    Int32 Channels = 0;

    Uint8 *Pixels = stbi_load_from_memory(Mem, Size, &Width, &Height, &Channels, 4);
    if (!Pixels)
    {
        LogCritical("%s\n", stbi_failure_reason());

        return TexHandleInvalid;
    }

    SDL_Texture *Tex = SDL_CreateTexture(App->Renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, Width, Height);
    if (!Tex)
    {
        LogCritical("%s\n", SDL_GetError());

        return TexHandleInvalid;
    }

    if (!SDL_UpdateTexture(Tex, 0, Pixels, Width * 4))
    {
        LogCritical("%s\n", SDL_GetError());

        return TexHandleInvalid;
    }

    stbi_image_free(Pixels);

    TexHandle Handle = App->TexCount;
    App->Texs[Handle] = Tex;
    App->TexCount++;

    return Handle;
}

//
// NOTE: Internal
//

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

            if (!SDL_SetRenderDrawColor(App->Renderer, Clear->Color.R, Clear->Color.G, Clear->Color.B, Clear->Color.A))
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

        case RenderCommand_DrawDebugText:
        {
            RenderDrawDebugText *DrawDebugText = (RenderDrawDebugText *)Cmd;

            if (!SDL_SetRenderDrawColor(App->Renderer, DrawDebugText->Color.R, DrawDebugText->Color.G, DrawDebugText->Color.B, DrawDebugText->Color.A))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }
            if (!SDL_SetRenderScale(App->Renderer, DrawDebugText->Scale.X, DrawDebugText->Scale.Y))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }

            if (!SDL_RenderDebugText(App->Renderer, DrawDebugText->Pos.X, DrawDebugText->Pos.Y, DrawDebugText->Str))
            {
                LogCritical("%s", SDL_GetError());
                Assert(0);
            }
        }
        break;

        case RenderCommand_DrawRect:
        {
            RenderDrawRect *DrawRect = (RenderDrawRect *)Cmd;

            SDL_FRect DstRect = {
                DrawRect->Dst.Pos.X,
                DrawRect->Dst.Pos.Y,
                DrawRect->Dst.Size.W,
                DrawRect->Dst.Size.H,
            };

            if (DrawRect->Tex == TexHandleInvalid)
            {
                // NOTE: Untextured Rectangle
                if (!SDL_SetRenderDrawColor(App->Renderer, DrawRect->Color.R, DrawRect->Color.G, DrawRect->Color.B, DrawRect->Color.A))
                {
                    LogCritical("%s", SDL_GetError());
                    Assert(0);
                }

                if (!SDL_RenderFillRect(App->Renderer, &DstRect))
                {
                    LogCritical("%s", SDL_GetError());
                    Assert(0);
                }
            }
            else
            {
                // NOTE: Textured Rectangle
                if (DrawRect->Tex >= App->TexCount)
                {
                    LogCritical("Invalid texture handle: %d", DrawRect->Tex);
                    Assert(0);
                    break;
                }

                SDL_Texture *Tex = App->Texs[DrawRect->Tex];
                if (!Tex)
                {
                    LogCritical("Attempted to draw null texture at handle: %d", DrawRect->Tex);
                    Assert(0);
                    break;
                }

                SDL_SetTextureColorMod(Tex, DrawRect->Color.R, DrawRect->Color.G, DrawRect->Color.B);
                SDL_SetTextureAlphaMod(Tex, DrawRect->Color.A);

                SDL_FRect SrcRect = {
                    DrawRect->Src.Pos.X,
                    DrawRect->Src.Pos.Y,
                    DrawRect->Src.Size.W,
                    DrawRect->Src.Size.H,
                };

                SDL_FRect *SrcPtr = &SrcRect;
                if (DrawRect->Src.Pos.X == 0 && DrawRect->Src.Pos.Y == 0 && DrawRect->Src.Size.W == 0)
                {
                    SrcPtr = 0;
                }

                if (!SDL_RenderTexture(App->Renderer, Tex, SrcPtr, &DstRect))
                {
                    LogCritical("%s", SDL_GetError());
                    Assert(0);
                }
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
        if (Mod->Rt.ModuleInst)
        {
            wasm_runtime_set_custom_data(Mod->Rt.ModuleInst, App);
        }

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

    if (!RtGlobalInit())
    {
        Assert(0);
    }

    //
    // NOTE: Natives.
    //

    NativeSymbol Natives[] = {
        {"PrintLine", (void *)HostPrintLine, "(ii)", 0},
        {"AllocTexture", (void *)HostAllocTexture, "(ii)i", 0}};
    if (!wasm_runtime_register_natives("env", Natives, sizeof(Natives) / sizeof(Natives[0])))
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
    Game->ExtraMem = SDL_calloc(1, Mb(2));
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

Bool Poll(SDL *SDL)
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
        case SDL_EVENT_KEY_DOWN:
        {
            if (Ev.key.repeat)
            {
                break;
            }

            SDL_Scancode Scancode = Ev.key.scancode;
        } break;
        }
    }

    return True;
}
