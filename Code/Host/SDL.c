#include "SDL.h"
#include "Host/Ent.h"
#include "KeyValue.h"
#include "Public/Ent.h"
#include "Runtime.h"
#include "SDL_Renderer.h"

#define GameZip "Game.zip"

//
// NOTE: Host-provided funcs.
//

static Void HostPrintLine(wasm_exec_env_t ExecEnv, Uint32 PtrOffset,
                          Uint32 Len)
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

// TODO: move this out to SDL_Renderer.c somehow.
// static TexHandle HostAllocTexture(wasm_exec_env_t ExecEnv, Uint32 PtrOffset,
// Uint32 Size)
// {
//     wasm_module_inst_t ModuleInst = wasm_runtime_get_module_inst(ExecEnv);

//     if (!wasm_runtime_validate_app_addr(ModuleInst, PtrOffset, Size))
//     {
//         LogCritical("Memory bounds violation.\n");

//         return TexHandleInvalid;
//     }

//     const Uint8 *Mem = (const Uint8
//     *)wasm_runtime_addr_app_to_native(ModuleInst, PtrOffset); if (!Mem ||
//     Size == 0)
//     {
//         LogCritical("Invalid pointer or size.\n");

//         return TexHandleInvalid;
//     }

//     SDL_Log("Texture allocation request for %u bytes.\n", Size);

//     SDL *App = (SDL *)wasm_runtime_get_custom_data(ModuleInst);
//     Assert(App);

//     if (App->Renderer.TexCount >= ArrayCount(App->Renderer.Texs))
//     {
//         LogCritical("Too many textures.\n");

//         return TexHandleInvalid;
//     }

//     Int32 Width = 0;
//     Int32 Height = 0;
//     Int32 Channels = 0;

//     Uint8 *Pixels = stbi_load_from_memory(Mem, Size, &Width, &Height,
//     &Channels, 4); if (!Pixels)
//     {
//         LogCritical("%s\n", stbi_failure_reason());

//         return TexHandleInvalid;
//     }

//     SDL_Texture *Tex = SDL_CreateTexture(App->Renderer.SDL,
//     SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, Width, Height); if
//     (!Tex)
//     {
//         LogCritical("%s\n", SDL_GetError());

//         return TexHandleInvalid;
//     }

//     if (!SDL_UpdateTexture(Tex, 0, Pixels, Width * 4))
//     {
//         LogCritical("%s\n", SDL_GetError());

//         return TexHandleInvalid;
//     }

//     stbi_image_free(Pixels);

//     TexHandle Handle = App->Renderer.TexCount;
//     App->Renderer.Texs[Handle] = Tex;
//     App->Renderer.TexCount++;

//     return Handle;
// }

static Uint32 HostReadFile(wasm_exec_env_t ExecEnv, Uint32 PathPtrOffset,
                           Uint32 PathLen, Uint32 DstPtrOffset,
                           Uint32 DstSize)
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
    MemNullTerminate(PathBuf, sizeof(PathBuf), PathLen);

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

    if (!Mod->ZipArchive.IsValid)
    {
        LogCritical("Calling mod is not a valid ZIP archive.\n");
        return 0;
    }

    SDL_Log("'%s' requested %s\n", Mod->Path, PathBuf);

    ZipEntry Ent = ZipGetEntByName(&Mod->ZipArchive, PathBuf);
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

    if (!ZipReadEnt(&Mod->ZipArchive, &Ent, Dst, DstSize))
    {
        LogCritical("Failed to read decompressed data from ZIP: %s\n", PathBuf);
        return 0;
    }

    return Ent.UncompressedSize;
}

//
// NOTE: ECS
//

// NOTE: Res

static ResID HostResGetID(wasm_exec_env_t ExecEnv, Uint32 NamePtrOffset,
                          Uint32 NameLen)
{
    wasm_module_inst_t ModuleInst = wasm_runtime_get_module_inst(ExecEnv);
    if (!wasm_runtime_validate_app_str_addr(ModuleInst, NamePtrOffset))
    {
        LogCritical("Resource name memory bounds violation.\n");
        return ResID_Invalid;
    }

    const char *Name = (const char *)wasm_runtime_addr_app_to_native(ModuleInst, NamePtrOffset);
    if (!Name || NameLen == 0)
    {
        LogCritical("Invalid resource name pointer or length.\n");
        return ResID_Invalid;
    }

    SDL *App = (SDL *)wasm_runtime_get_custom_data(ModuleInst);
    Assert(App);

    return ResGetID(&App->World, Name, NameLen);
}

static Uint32 HostResGetVal(wasm_exec_env_t ExecEnv, Uint32 ResID)
{
    wasm_module_inst_t ModuleInst = wasm_runtime_get_module_inst(ExecEnv);
    SDL *App = (SDL *)wasm_runtime_get_custom_data(ModuleInst);
    Assert(App);

    return ResGetVal(&App->World, ResID);
}

static Uint32 HostResSetVal(wasm_exec_env_t ExecEnv, Uint32 ResID, Uint32 Val)
{
    wasm_module_inst_t ModuleInst = wasm_runtime_get_module_inst(ExecEnv);
    SDL *App = (SDL *)wasm_runtime_get_custom_data(ModuleInst);
    Assert(App);

    return ResSetVal(&App->World, ResID, Val);
}

// NOTE: Comp

static Void HostCompInit(wasm_exec_env_t ExecEnv, Uint32 OutPtrOffset,
                         Uint32 Hash, Uint32 Size)
{
    wasm_module_inst_t ModuleInst = wasm_runtime_get_module_inst(ExecEnv);
    SDL *App = (SDL *)wasm_runtime_get_custom_data(ModuleInst);
    Assert(App);

    if (!wasm_runtime_validate_app_addr(ModuleInst, OutPtrOffset,
                                        sizeof(CompTypeResult)))
    {
        LogCritical("Output memory bounds violation.\n");
        return;
    }

    CompTypeResult *Result = (CompTypeResult *)wasm_runtime_addr_app_to_native(
        ModuleInst, OutPtrOffset);
    if (!Result)
    {
        LogCritical("Invalid output pointer.\n");
        return;
    }

    *Result = CompInit(&App->World, Hash, Size);
}

// NOTE: Ent

static Void HostEntInit(wasm_exec_env_t ExecEnv, Uint32 OutPtrOffset)
{
    wasm_module_inst_t ModuleInst = wasm_runtime_get_module_inst(ExecEnv);

    SDL *App = (SDL *)wasm_runtime_get_custom_data(ModuleInst);
    Assert(App);

    if (!wasm_runtime_validate_app_addr(ModuleInst, OutPtrOffset,
                                        sizeof(EntResult)))
    {
        LogCritical("Output memory bounds violation.\n");
        return;
    }

    EntResult *Result =
        (EntResult *)wasm_runtime_addr_app_to_native(ModuleInst, OutPtrOffset);
    if (!Result)
    {
        LogCritical("Invalid output pointer.\n");
        return;
    }

    *Result = EntInit(&App->World);
}

static Bool HostEntAddComp(wasm_exec_env_t ExecEnv, Uint32 EntID, Uint32 TypeID,
                           Uint32 MemOffset)
{
    wasm_module_inst_t ModuleInst = wasm_runtime_get_module_inst(ExecEnv);
    SDL *App = (SDL *)wasm_runtime_get_custom_data(ModuleInst);
    Assert(App);

    World *World = &App->World;

    Int32 InternalID = GetInternalTypeID(World, TypeID);
    if (InternalID < 0)
    {
        LogCritical("Invalid Comp Type Hash %u.\n", TypeID);
        return False;
    }

    Usize Size = World->CompSizes[InternalID];
    if (!wasm_runtime_validate_app_addr(ModuleInst, MemOffset, Size))
    {
        LogCritical("Comp input memory bounds violation.\n");
        return False;
    }

    const Void *Mem = wasm_runtime_addr_app_to_native(ModuleInst, MemOffset);
    return EntAddComp(World, EntID, TypeID, Mem);
}

static Void HostEntGetComp(wasm_exec_env_t ExecEnv, Uint32 OutPtrOffset,
                           Uint32 EntID, Uint32 TypeID)
{
    wasm_module_inst_t ModuleInst = wasm_runtime_get_module_inst(ExecEnv);
    SDL *App = (SDL *)wasm_runtime_get_custom_data(ModuleInst);
    Assert(App);

    if (!wasm_runtime_validate_app_addr(ModuleInst, OutPtrOffset,
                                        sizeof(CompResult)))
    {
        LogCritical("Output memory bounds violation.\n");
        return;
    }

    CompResult *Result =
        (CompResult *)wasm_runtime_addr_app_to_native(ModuleInst, OutPtrOffset);
    if (!Result)
    {
        LogCritical("Invalid output pointer.\n");
        return;
    }

    *Result = EntGetComp(&App->World, EntID, TypeID);
}

// NOTE: Iter

static Void HostIterInit(wasm_exec_env_t ExecEnv, Uint32 OutIterOffset, Uint32 CompIDsOffset, Uint32 CompCount)
{
    wasm_module_inst_t ModuleInst = wasm_runtime_get_module_inst(ExecEnv);
    SDL *App = (SDL *)wasm_runtime_get_custom_data(ModuleInst);
    Assert(App);

    if (!wasm_runtime_validate_app_addr(ModuleInst, OutIterOffset, sizeof(Iter)) ||
        !wasm_runtime_validate_app_addr(ModuleInst, CompIDsOffset, CompCount * sizeof(CompID)))
    {
        LogCritical("Memory bounds violation.\n");
        return;
    }

    Iter *Result = (Iter *)wasm_runtime_addr_app_to_native(ModuleInst, OutIterOffset);
    const CompID *CompIDs = (const CompID *)wasm_runtime_addr_app_to_native(ModuleInst, CompIDsOffset);

    if (!Result || !CompIDs)
    {
        return;
    }

    *Result = IterInit(&App->World, CompIDs, CompCount);
}

static Bool HostIterNext(wasm_exec_env_t ExecEnv, Uint32 IterOffset, Uint32 OutEntIDOffset, Uint32 VarargsBufferOffset)
{
    wasm_module_inst_t ModuleInst = wasm_runtime_get_module_inst(ExecEnv);
    SDL *App = (SDL *)wasm_runtime_get_custom_data(ModuleInst);
    Assert(App);

    if (!wasm_runtime_validate_app_addr(ModuleInst, IterOffset, sizeof(Iter)))
    {
        LogCritical("IterNext: Iterator memory bounds violation.\n");
        return False;
    }

    if (OutEntIDOffset != 0 && !wasm_runtime_validate_app_addr(ModuleInst, OutEntIDOffset, sizeof(EntID)))
    {
        LogCritical("OutEntID memory bounds violation.\n");
        return False;
    }

    Iter *It = (Iter *)wasm_runtime_addr_app_to_native(ModuleInst, IterOffset);
    if (!It || !It->IsValid)
    {
        return False;
    }

    if (It->CompCount > 0 && VarargsBufferOffset != 0)
    {
        if (!wasm_runtime_validate_app_addr(ModuleInst, VarargsBufferOffset, It->CompCount * sizeof(Uint32)))
        {
            LogCritical("Varargs buffer memory bounds violation.\n");
            return False;
        }
    }

    const Uint32 *Varargs = (It->CompCount > 0) ? (const Uint32 *)wasm_runtime_addr_app_to_native(ModuleInst, VarargsBufferOffset) : 0;

    Void *CompPtrs[MaxIterComps] = {0};
    Void **CompPtrPtrs[MaxIterComps];
    for (Uint32 I = 0; I < It->CompCount; ++I)
    {
        CompPtrPtrs[I] = &CompPtrs[I];
    }

    EntID Ent = 0;

    if (!IterNextArray(&App->World, It, &Ent, CompPtrPtrs))
    {
        return False;
    }
    if (OutEntIDOffset != 0)
    {
        EntID *OutEntID = (EntID *)wasm_runtime_addr_app_to_native(ModuleInst, OutEntIDOffset);
        if (OutEntID)
        {
            *OutEntID = Ent;
        }
    }

    if (!Varargs)
    {
        return True;
    }

    for (Uint32 I = 0; I < It->CompCount; ++I)
    {
        Uint32 DstOffset = Varargs[I];
        if (DstOffset == 0 || CompPtrs[I] == 0)
        {
            continue;
        }

        Usize Size = App->World.CompSizes[It->InternalCompIDs[I]];
        if (!wasm_runtime_validate_app_addr(ModuleInst, DstOffset, Size))
        {
            LogCritical("Component %u destination memory bounds violation.\n", I);
            return False;
        }

        Void *Dst = wasm_runtime_addr_app_to_native(ModuleInst, DstOffset);
        if (Dst)
        {
            SDL_memcpy(Dst, CompPtrs[I], Size);
        }
    }

    return True;
}

//
// NOTE: Input
//

static Void HostGetInputState(wasm_exec_env_t ExecEnv, Uint32 OutPtrOffset, Input Input)
{
    wasm_module_inst_t ModuleInst = wasm_runtime_get_module_inst(ExecEnv);
    SDL *App = (SDL *)wasm_runtime_get_custom_data(ModuleInst);
    Assert(App);

    InputState *Result =
        (InputState *)wasm_runtime_addr_app_to_native(ModuleInst, OutPtrOffset);
    if (!Result)
    {
        LogCritical("Invalid output pointer.\n");
        return;
    }

    if (Input < 0 || Input > Key_Count)
    {
        LogCritical("Invalid key.\n");
        *Result = (InputState){0};
        return;
    }

    *Result = App->Inputs[Input];
}

//
// NOTE: Internal
//

// static inline Float32 GetDeltaSeconds()
// {
//     static Uint64 LastFrameTicks = 0;
//     static Bool IsInitialized = True;

//     if (IsInitialized)
//     {
//         IsInitialized = False;
//         LastFrameTicks = SDL_GetTicks();
//     }

//     Uint64 ThisFrameTicks = SDL_GetTicks();
//     Float32 DeltaSeconds = (ThisFrameTicks - LastFrameTicks) / 1000.0f;
//     LastFrameTicks = ThisFrameTicks;

//     return DeltaSeconds;
// }

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

static Bool ReadManifest(ZipArchive *Archive, const char *Path)
{
    Assert(Archive);
    Assert(Path);

    ZipEntry Manifest = ZipGetEntByName(Archive, "Manifest.txt");
    if (!Manifest.IsValid)
    {
        LogCritical("%s is missing Manifest.txt.\n", Path);
        return False;
    }

    char *Buf = (char *)SDL_malloc(Manifest.UncompressedSize);
    if (!Buf)
    {
        LogCritical("Failed to allocate memory for manifest buffer.\n");
        return False;
    }

    if (!ZipReadEnt(Archive, &Manifest, (Uint8 *)Buf,
                    Manifest.UncompressedSize))
    {
        LogCritical("Failed to read Manifest.txt from archive.\n");
        return False;
    }

    Uint8 Mem[1024];
    MemAlloc Alloc = MemAllocInit(Mem, sizeof(Mem));

    KeyValueList List = KeyValueParse(&Alloc, Buf, Manifest.UncompressedSize);
    if (List.HasError)
    {
        LogCritical("Failed to parse Manifest.txt in %s.\n", Path);
        return False;
    }

#define ReqField(Field)                                                        \
    KeyValuePair Field##Pair =                                                 \
        KeyValueListFind(List, #Field, sizeof(#Field) - 1);                    \
    if (Field##Pair.KeyLen == 0)                                               \
    {                                                                          \
        LogCritical("Invalid manifest in %s. Missing required field: '%s'.\n", \
                    Path, #Field);                                             \
        return False;                                                          \
    }

#define OptField(Field)        \
    KeyValuePair Field##Pair = \
        KeyValueListFind(List, #Field, sizeof(#Field) - 1);

    ReqField(Name);
    ReqField(Version);

    OptField(Author);
    OptField(Summary);

#undef ReqField
#undef OptField

    if (AuthorPair.KeyLen > 0)
    {
        SDL_Log("%s: %.*s %.*s by %.*s\n", Path, (Int32)NamePair.ValueLen,
                NamePair.Value, (Int32)VersionPair.ValueLen, VersionPair.Value,
                (Int32)AuthorPair.ValueLen, AuthorPair.Value);
    }
    else
    {
        SDL_Log("%s: %.*s %.*s\n", Path, (Int32)NamePair.ValueLen,
                NamePair.Value, (Int32)VersionPair.ValueLen, VersionPair.Value);
    }

    if (SummaryPair.KeyLen > 0)
    {
        SDL_Log("%.*s\n", (Int32)SummaryPair.ValueLen, SummaryPair.Value);
    }

    SDL_free(Buf);
    return True;
}

static Bool LoadOneMod(SDL *App, Mod *Mod, const char *ZipPath)
{
    Assert(Mod);
    Assert(ZipPath);

    if (Mod->ArchiveMem)
    {
        SDL_free((void *)Mod->ArchiveMem);
        Mod->ArchiveMem = 0;
        Mod->ArchiveSize = 0;
        SDL_memset(&Mod->ZipArchive, 0, sizeof(Mod->ZipArchive));
    }

    size_t Size = 0;
    void *File = SDL_LoadFile(ZipPath, &Size);
    if (!File)
    {
        LogCritical("Failed to load ZIP file: %s\n", ZipPath);
        return False;
    }

    Mod->ArchiveMem = (const Uint8 *)File;
    Mod->ArchiveSize = Size;

    Mod->ZipArchive = ZipOpen(Mod->ArchiveMem, Mod->ArchiveSize);
    if (!Mod->ZipArchive.IsValid)
    {
        LogCritical("Failed to parse ZIP archive: %s\n", ZipPath);
        return False;
    }

    if (!ReadManifest(&Mod->ZipArchive, ZipPath))
    {
        LogCritical("Failed to read manifest.\n");
        return False;
    }

    ZipEntry Wasm = {0};
    for (Uint32 I = 0; I < Mod->ZipArchive.Count; ++I)
    {
        ZipEntry Ent = ZipGetEntByIndex(&Mod->ZipArchive, I);
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

    if (!ZipReadEnt(&Mod->ZipArchive, &Wasm, WasmBuf, Wasm.UncompressedSize))
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
        MemNullTerminate(PathBuf, sizeof(PathBuf), PathLen - 4);
    }
    else
    {
        SDL_snprintf(PathBuf, sizeof(PathBuf), "%s", ZipPath);
    }
    SDL_strlcat(PathBuf, "_Extracted.wasm", sizeof(PathBuf));

    if (!SDL_SaveFile(PathBuf, WasmBuf, Wasm.UncompressedSize))
    {
        LogCritical("Failed to write extracted wasm to %s: %s\n", PathBuf,
                    SDL_GetError());
        SDL_free(WasmBuf);
        return False;
    }

    SDL_free(WasmBuf);

    if (!RtLoadOne(&Mod->Rt, PathBuf))
    {
        LogCritical("RtLoadOne failed on extracted wasm: %s\n", PathBuf);
        return False;
    }

    if (Mod->Rt.ModuleInst)
    {
        wasm_runtime_set_custom_data(Mod->Rt.ModuleInst, App);
    }

    // NOTE: if this is 0 that means it is the very first time we are
    // loading this module
    if (Mod->LastWriteTime == 0)
    {
        if (!RtCallInit(&Mod->Rt))
        {
            LogCritical("RtCallInit failed on %s\n", ZipPath);
            return False;
        }
    }

    return True;
}

// NOTE: UserData -- *SDL
static SDL_EnumerationResult SDLCALL EnumerateDirectoryCallback(
    Void *UserData, const char *DirName, const char *FileName)
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
        if (Mod->Rt.IsValid && LoadOneMod(App, Mod, Mod->Path))
        {
            Mod->LastWriteTime = GetFileModTime(Mod->Path);
            App->ModCount++;
        }
    }

    return SDL_ENUM_CONTINUE;
}

//
// NOTE: Implementation
//

Void Update(SDL *App)
{
    Assert(App);

    // App->State.Time.Delta = GetDeltaSeconds();

    for (Uint32 I = 0; I < App->ModCount; ++I)
    {
        Mod *Mod = &App->Mods[I];

        Int64 CurrentTime = GetFileModTime(Mod->Path);
        if (CurrentTime > Mod->LastWriteTime && CurrentTime)
        {
            SDL_Delay(50);
            SDL_Log("Reloading: %s\n", Mod->Path);

            RtDeinit(&Mod->Rt);
            Mod->Rt = RtInit();

            if (LoadOneMod(App, Mod, Mod->Path))
            {
                Mod->LastWriteTime = CurrentTime;
            }
        }
        // NOTE: This handles the case where you got an error on compilation
        if (Mod->Rt.IsValid)
        {
            if (!RtCallUpdate(&Mod->Rt))
            {
                Assert(0);
            }
        }
    }
}

// static inline Void ApplyInputBindings(SDL *App)
// {
//     /* TODO: later read them from a input file. Right now they are hardcoded
//     */ Input *Input = &App->State.Input;

//     App->Keys[SDL_SCANCODE_D] = &Input->Right;
//     App->Keys[SDL_SCANCODE_A] = &Input->Left;
//     App->Keys[SDL_SCANCODE_SPACE] = &Input->Jump;
//     App->Keys[SDL_SCANCODE_LSHIFT] = &Input->Dash;
//     App->Keys[SDL_SCANCODE_E] = &Input->Hook;
//     App->Keys[SDL_SCANCODE_S] = &Input->Slam;

//     App->Keys[MouseButtonLeft] = &Input->LMB;
//     App->Keys[MouseButtonRight] = &Input->RMB;
//     App->Keys[MouseButtonMiddle] = &Input->MMB;
// }

SDL Init()
{
    SDL Result = {0};

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        LogCritical("%s", SDL_GetError());
        Assert(0);
    }

    Result.Window = SDL_CreateWindow("Game", 1280, 720, SDL_WINDOW_RESIZABLE);
    if (!Result.Window)
    {
        LogCritical("%s", SDL_GetError());
        Assert(0);
    }

    Result.Renderer = RendererInit(Result.Window);
    if (!Result.Renderer.IsValid)
    {
        LogCritical("%s", SDL_GetError());
        Assert(0);
    }

    //
    // NOTE: Built-in components.
    //

    CompInit(&Result.World, CompTransformHash, sizeof(CompTransform));
    CompInit(&Result.World, CompRenderableHash, sizeof(CompRenderable));

    // ApplyInputBindings(&Result);

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
        // NOTE: I/O
        {"PrintLine", (void *)HostPrintLine, "(ii)", 0},
        // {"AllocTexture", (void *)HostAllocTexture, "(ii)i", 0},
        {"ReadFile", (void *)HostReadFile, "(iiii)i", 0},
        // NOTE: ECS
        {"CompInit", (void *)HostCompInit, "(iii)", 0},

        {"EntInit", (void *)HostEntInit, "(i)", 0},
        {"EntAddComp", (void *)HostEntAddComp, "(iii)i", 0},
        {"EntGetComp", (void *)HostEntGetComp, "(iii)", 0},

        {"ResGetID", (void *)HostResGetID, "(ii)i", 0},
        {"ResGetVal", (void *)HostResGetVal, "(i)i", 0},
        {"ResSetVal", (void *)HostResSetVal, "(ii)", 0},

        {"IterInit", (void *)HostIterInit, "(iii)", 0},
        {"IterNext", (void *)HostIterNext, "(iii)i", 0},

        // NOTE: Input
        {"GetInputState", (void *)HostGetInputState, "(ii)", 0}};
    if (!wasm_runtime_register_natives("env", Natives, ArrayCount(Natives)))
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
    if (!Game->Rt.IsValid || !LoadOneMod(&Result, Game, Game->Path))
    {
        Assert(0);
    }

    Game->LastWriteTime = GetFileModTime(Game->Path);
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

static inline Int32 GetMouseIndex(Uint8 Code)
{
    switch (Code)
    {
    case SDL_BUTTON_LEFT:
        return Button_Left;
    case SDL_BUTTON_RIGHT:
        return Button_Right;
    default:
        return -1;
    }
}

static inline Int32 GetKeyIndex(SDL_Scancode Code)
{
    switch (Code)
    {
    case SDL_SCANCODE_W:
        return Key_W;
    case SDL_SCANCODE_A:
        return Key_A;
    case SDL_SCANCODE_S:
        return Key_S;
    case SDL_SCANCODE_D:
        return Key_D;
    default:
        return -1;
    }
}

Void Render(SDL *App)
{
    Assert(App);

    if (!RendererDraw(&App->World, &App->Renderer))
    {
        LogCritical("%s", SDL_GetError());
        Assert(0);
    }
}

Bool Poll(SDL *App)
{
    Assert(App);

    for (Usize I = 0; I < Key_Count; I++)
    {
        InputState *InputState = &App->Inputs[I];

        if (InputState)
        {
            InputState->Pressed = False;
            InputState->Released = False;
        }
    }

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

            // case SDL_EVENT_MOUSE_MOTION:
            // {
            //     // TODO: Can we not do that to not do that
            //     SDL_ConvertEventToRenderCoordinates(App->Renderer.SDL, &Ev);
            //     App->State.Input.MousePos.X = Ev.motion.x;
            //     App->State.Input.MousePos.Y = Ev.motion.y;
            // }
            // break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            Int32 Index = GetMouseIndex(Ev.button.button);

            if (Index != -1)
            {
                InputState *InputState = &App->Inputs[Index];
                Assert(InputState);

                InputState->Down = True;
                InputState->Pressed = True;
            }
        }
        break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
        {
            Int32 Index = GetMouseIndex(Ev.button.button);

            if (Index != -1)
            {
                InputState *InputState = &App->Inputs[Index];
                Assert(InputState);

                InputState->Down = False;
                InputState->Released = True;
            }
        }
        break;

        case SDL_EVENT_KEY_DOWN:
        {
            SDL_Scancode Scancode = Ev.key.scancode;
            if (Scancode > 0 && Scancode < SDL_SCANCODE_COUNT)
            {
                Int32 Index = GetKeyIndex(Scancode);

                if (Index != -1)
                {
                    InputState *InputState = &App->Inputs[Index];
                    Assert(InputState);

                    InputState->Down = True;
                    InputState->Pressed = True;
                }
            }
        }
        break;

        case SDL_EVENT_KEY_UP:
        {
            SDL_Scancode Scancode = Ev.key.scancode;
            if (Scancode > 0 && Scancode < SDL_SCANCODE_COUNT)
            {
                Int32 Index = GetKeyIndex(Scancode);

                if (Index != -1)
                {
                    InputState *InputState = &App->Inputs[Index];
                    Assert(InputState);

                    InputState->Down = False;
                    InputState->Released = True;
                }
            }
        }
        break;

        default:
        {
            break;
        }
        }
    }

    return True;
}
