//
// NOTE: WASM Runtime implementation.
//
#include "Runtime.h"
#include "SDK.h"
#include "SDL.h"

#include "wasm_export.h"

Bool RtGlobalInit(Void)
{
    // NOTE: This is needed since wasm_runtime_full_init only must be called once.
    static Bool IsInitialized = False;
    if (IsInitialized)
    {
        return True;
    }

    RuntimeInitArgs InitArgs = {0};
    InitArgs.mem_alloc_type = Alloc_With_Allocator;
    InitArgs.mem_alloc_option.allocator.malloc_func = SDL_malloc;
    InitArgs.mem_alloc_option.allocator.realloc_func = SDL_realloc;
    InitArgs.mem_alloc_option.allocator.free_func = SDL_free;

    if (!wasm_runtime_full_init(&InitArgs))
    {
        LogCritical("wasm_runtime_full_init\n");
        return False;
    }

    IsInitialized = True;
    return True;
}

Runtime RtInit(Void)
{
    Runtime Result = {0};

    Result.IsValid = True;
    return Result;
}

Void RtDeinit(Runtime *Rt)
{
    Assert(Rt);

    if (Rt->ExecEnv)
    {
        wasm_runtime_destroy_exec_env(Rt->ExecEnv);
        Rt->ExecEnv = 0;
    }
    if (Rt->ModuleInst)
    {
        wasm_runtime_deinstantiate(Rt->ModuleInst);
        Rt->ModuleInst = 0;
    }
    if (Rt->Module)
    {
        wasm_runtime_unload(Rt->Module);
        Rt->Module = 0;
    }
    if (Rt->Bytes)
    {
        SDL_free(Rt->Bytes);
        Rt->Bytes = 0;
    }

    Rt->IsValid = False;
}

Bool RtLoadOne(Runtime *Rt, const char *Path)
{
    Assert(Rt);
    Assert(Path);

    Usize Size = 0;
    Rt->Bytes = SDL_LoadFile(Path, &Size);
    if (!Rt->Bytes)
    {
        LogCritical("%s\n", SDL_GetError());
        return False;
    }

    char ErrorBuf[128];

    Rt->Module = wasm_runtime_load((Uint8 *)Rt->Bytes, (Uint32)Size, ErrorBuf, sizeof(ErrorBuf));
    if (!Rt->Module)
    {
        LogCritical("wasm_runtime_load: %s\n", ErrorBuf);
        return False;
    }

    Rt->ModuleInst = wasm_runtime_instantiate(Rt->Module, Kb(64), Kb(64), ErrorBuf, sizeof(ErrorBuf));
    if (!Rt->ModuleInst)
    {
        LogCritical("wasm_runtime_instantiate: %s\n", ErrorBuf);
        return False;
    }

    Rt->ExecEnv = wasm_runtime_create_exec_env(Rt->ModuleInst, Kb(16));
    if (!Rt->ExecEnv)
    {
        LogCritical("wasm_runtime_create_exec_env\n");
        return False;
    }

    //
    // NOTE: Imports.
    //

#define Function(Name, Signature)                                   \
    Rt->Name = wasm_runtime_lookup_function(Rt->ModuleInst, #Name); \
    if (!Rt->Name)                                                  \
    {                                                               \
        LogCritical("wasm_runtime_lookup_function  ('%s')\n"        \
                    "NOTE: Runtime expects '%s' to be exported.\n", \
                    #Name, #Signature);                             \
        return False;                                               \
    }

    Function(UpdateAndRender, Void UpdateAndRender(*State, *ExtraMem, *RenderBuf));

    Function(GetState, State * GetState(Void));
    Function(GetRenderBuf, RenderBuf * GetRenderBuf(Void));
    Function(GetExtraMem, Void * GetExtraMem(Void));

#undef Function

#define Pointer(Name)                                                       \
    {                                                                       \
        Uint32 Results[1] = {0};                                            \
        if (wasm_runtime_call_wasm(Rt->ExecEnv, Rt->Get##Name, 0, Results)) \
        {                                                                   \
            Rt->Name = Results[0];                                          \
        }                                                                   \
    }

    Pointer(State);
    Pointer(RenderBuf);
    Pointer(ExtraMem);

#undef Pointer

    Rt->IsValid = True;
    return True;
}

Bool RtUpdate(Runtime *Rt, State *HostState, RenderBuf *HostRenderBuf)
{
    Assert(Rt);
    Assert(Rt->UpdateAndRender);
    Assert(Rt->ModuleInst);

    // NOTE: State
    if (!wasm_runtime_validate_app_addr(Rt->ModuleInst, Rt->State, sizeof(State)))
    {
        LogCritical("State memory is out of bounds.\n");
        return False;
    }
    State *NativeState = (State *)wasm_runtime_addr_app_to_native(Rt->ModuleInst, Rt->State);

    // NOTE: RenderBuf
    if (!wasm_runtime_validate_app_addr(Rt->ModuleInst, Rt->RenderBuf, 16))
    {
        LogCritical("RenderBuf header is out of bounds.\n");
        return False;
    }
    Uint32 *NativeRenderBuf = (Uint32 *)wasm_runtime_addr_app_to_native(Rt->ModuleInst, Rt->RenderBuf);

    // NOTE: Copy in.
    SDL_memcpy(NativeState, HostState, sizeof(State));

    Uint32 Args[3] = {Rt->State, Rt->ExtraMem, Rt->RenderBuf};
    if (!wasm_runtime_call_wasm(Rt->ExecEnv, Rt->UpdateAndRender, 3, Args))
    {
        LogCritical("wasm_runtime_call_wasm  (runtime error): %s\n", wasm_runtime_get_exception(Rt->ModuleInst));
        return False;
    }

    // NOTE: Copy out.
    SDL_memcpy(HostState, NativeState, sizeof(State));

    // typedef struct
    // {
    //     Uint8 *Mem; 0-3
    //     Uint32 Size; 4-7
    //     Uint32 Cap; 8-11

    //     Bool IsValid; 12-15
    // } RenderBuf;

    Uint32 RenderBufMemOffset = NativeRenderBuf[0];
    Uint32 RenderBufSizeOffset = NativeRenderBuf[1];

    if (RenderBufSizeOffset == 0)
    {
        return True;
    }

    if (!wasm_runtime_validate_app_addr(Rt->ModuleInst, RenderBufMemOffset, RenderBufSizeOffset))
    {
        LogCritical("RenderBuf memory is out of bounds.\n");
        return False;
    }

    Void *RenderBufMem = (Void *)wasm_runtime_addr_app_to_native(Rt->ModuleInst, RenderBufMemOffset);

    Void *Dst = RenderBufPush(HostRenderBuf, RenderBufSizeOffset);
    if (Dst)
    {
        SDL_memcpy(Dst, RenderBufMem, RenderBufSizeOffset);
        NativeRenderBuf[1] = 0;
    }
    else
    {
        LogCritical("Out of memory.\n");
        return False;
    }

    return True;
}
