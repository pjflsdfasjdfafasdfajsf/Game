#include "SDL.h"
#include "Render.h"
#include "STB.h"
#include "wasm_export.h"

#define GameZip "Game.zip"

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

    if (!wasm_runtime_validate_app_addr(ModuleInst, PtrOffset, Size))
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

static Uint32 HostReadFile(wasm_exec_env_t ExecEnv, Uint32 PathPtrOffset, Uint32 PathLen, Uint32 DstPtrOffset, Uint32 DstSize)
{
    wasm_module_inst_t ModuleInst = wasm_runtime_get_module_inst(ExecEnv);

    if (!wasm_runtime_validate_app_str_addr(ModuleInst, PathPtrOffset))
    {
        LogCritical("Path memory bounds violation.\n");
        return 0;
    }

    Bool QuerySizeOnly = (DstPtrOffset == 0 || DstSize == 0);
    if (!QuerySizeOnly)
    {
        if (!wasm_runtime_validate_app_addr(ModuleInst, DstPtrOffset, DstSize))
        {
            LogCritical("Buffer memory bounds violation.\n");
            return 0;
        }
    }

    const char *Path = (const char *)wasm_runtime_addr_app_to_native(ModuleInst, PathPtrOffset);
    Uint8 *Dst = QuerySizeOnly ? 0 : (Uint8 *)wasm_runtime_addr_app_to_native(ModuleInst, DstPtrOffset);

    if (!Path || PathLen == 0)
    {
        LogCritical("Invalid path pointer or length.\n");
        return 0;
    }

    char PathBuf[256];
    if (PathLen >= sizeof(PathBuf))
    {
        LogCritical("Path too long.\n");
        return 0;
    }
    for (Uint32 I = 0; I < PathLen; ++I)
    {
        PathBuf[I] = (Path[I] == '\\') ? '/' : Path[I];
    }
    PathBuf[PathLen] = '\0';

    SDL *App = (SDL *)wasm_runtime_get_custom_data(ModuleInst);
    Assert(App);

    Mod *Mod = 0;
    for (Uint32 I = 0; I < App->ModCount; ++I)
    {
        if (App->Mods[I].Rt.ModuleInst == ModuleInst)
        {
            Mod = &App->Mods[I];
            break;
        }
    }

    if (!Mod)
    {
        LogCritical("Could not identify calling mod.\n");
        return 0;
    }

    if (!Mod->Archive.IsValid)
    {
        LogCritical("Calling mod is not a valid ZIP archive.\n");
        return 0;
    }

    SDL_Log("'%s' requested %s\n", Mod->Path, PathBuf);

    ZipEntry Ent = ZipGetEntByName(&Mod->Archive, PathBuf);
    if (!Ent.IsValid)
    {
        LogCritical("'%s' not found.\n", PathBuf);
        return 0;
    }

    if (QuerySizeOnly)
    {
        return Ent.UncompressedSize;
    }

    if (DstSize < Ent.UncompressedSize)
    {
        LogCritical("Buffer too small: target needs %u bytes, got %u.\n", Ent.UncompressedSize, DstSize);
        return 0;
    }

    if (!ZipReadEnt(&Mod->Archive, &Ent, Dst, DstSize))
    {
        LogCritical("Failed to read decompressed data from ZIP: %s\n", PathBuf);
        return 0;
    }

    return Ent.UncompressedSize;
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

static Bool LoadOneMod(Mod *Mod, const char *ZipPath)
{
    Assert(Mod);
    Assert(ZipPath);

    if (Mod->Mem)
    {
        SDL_free((void *)Mod->Mem);
        Mod->Mem = 0;
        Mod->Size = 0;
        SDL_memset(&Mod->Archive, 0, sizeof(Mod->Archive));
    }

    size_t Size = 0;
    void *File = SDL_LoadFile(ZipPath, &Size);
    if (!File)
    {
        LogCritical("Failed to load ZIP file: %s\n", ZipPath);
        return False;
    }

    Mod->Mem = (const Uint8 *)File;
    Mod->Size = Size;

    Mod->Archive = ZipOpen(Mod->Mem, Mod->Size);
    if (!Mod->Archive.IsValid)
    {
        LogCritical("Failed to parse ZIP archive: %s\n", ZipPath);
        return False;
    }

    ZipEntry Wasm = {0};
    for (Uint32 I = 0; I < Mod->Archive.Count; ++I)
    {
        ZipEntry Ent = ZipGetEntByIndex(&Mod->Archive, I);
        if (Ent.IsValid && ZipEntEndsWith(&Ent, ".wasm"))
        {
            Wasm = Ent;
            break;
        }
    }

    if (!Wasm.IsValid)
    {
        LogCritical("No .wasm file found in zip archive %s\n", ZipPath);
        return False;
    }

    Uint8 *WasmBuf = (Uint8 *)SDL_malloc(Wasm.UncompressedSize);
    if (!WasmBuf)
    {
        LogCritical("Failed to allocate memory for decompressed wasm.\n");
        return False;
    }

    if (!ZipReadEnt(&Mod->Archive, &Wasm, WasmBuf, Wasm.UncompressedSize))
    {
        LogCritical("Failed to decompress wasm from zip %s\n", ZipPath);
        SDL_free(WasmBuf);
        return False;
    }

    char PathBuf[1024];
    Usize PathLen = SDL_strlen(ZipPath);
    if (PathLen > 4 && SDL_strcmp(ZipPath + PathLen - 4, ".zip") == 0)
    {
        SDL_memcpy(PathBuf, ZipPath, PathLen - 4);
        PathBuf[PathLen - 4] = '\0';
    }
    else
    {
        SDL_snprintf(PathBuf, sizeof(PathBuf), "%s", ZipPath);
    }
    SDL_strlcat(PathBuf, "_extracted.wasm", sizeof(PathBuf));

    if (!SDL_SaveFile(PathBuf, WasmBuf, Wasm.UncompressedSize))
    {
        LogCritical("Failed to write extracted wasm to %s: %s\n", PathBuf, SDL_GetError());
        SDL_free(WasmBuf);
        return False;
    }

    SDL_free(WasmBuf);

    if (!RtLoadOne(&Mod->Rt, PathBuf))
    {
        LogCritical("RtLoadOne failed on extracted wasm: %s\n", PathBuf);
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
    if (Len > 4 && SDL_strcmp(FileName + Len - 4, ".zip") == 0)
    {
        Mod *Mod = &App->Mods[App->ModCount];

        SDL_snprintf(Mod->Path, sizeof(Mod->Path), "%s%s", DirName, FileName);

        Mod->Rt = RtInit();
        if (Mod->Rt.IsValid && LoadOneMod(Mod, Mod->Path))
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

            if (LoadOneMod(Mod, Mod->Path))
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

    // NOTE: I never wish to experience this to anyone, but this MUST be static.
    // You don't realize through how many hours of debugging I went through not
    // understading why on reloads the game would just hang, and all because
    // THIS fucking array was not static. This was just IMPOSSIBLE to fucking
    // debug as all it was just a HANG.
    static NativeSymbol Natives[] = {
        {"PrintLine", (void *)HostPrintLine, "(ii)", 0},
        {"AllocTexture", (void *)HostAllocTexture, "(ii)i", 0},
        {"ReadFile", (void *)HostReadFile, "(iiii)i", 0}};
    if (!wasm_runtime_register_natives("env", Natives, sizeof(Natives) / sizeof(Natives[0])))
    {
        Assert(0);
    }

    //
    // NOTE: Game loading.
    //
    Mod *Game = &Result.Mods[0];
    if (!GetAbsPath(Game->Path, sizeof(Game->Path), GameZip))
    {
        Assert(0);
    }

    Game->Rt = RtInit();
    if (!Game->Rt.IsValid || !LoadOneMod(Game, Game->Path))
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
