//
// NOTE: WASM host.
//

#include "Host.h"
#include "Render.h"
#include "SDL.h"
#include "Types.h"
#include "wasm3.h"

//
// NOTE: Host-provided funcs.
//

m3ApiRawFunction(Print)
{
    m3ApiGetArgMem(const char *, Ptr);
    m3ApiGetArg(Uint32, Len);

    m3ApiCheckMem(Ptr, Len);

    if (Ptr && Len > 0)
    {
        SDL_Log("%.*s\n", (Int32)Len, Ptr);
    }

    m3ApiSuccess();
}

//
// NOTE: Implementation.
//

Host HostInit(Void)
{
    Host Result = {0};

    Result.Env = m3_NewEnvironment();
    if (!Result.Env)
    {
        LogCritical("m3_NewEnvironment\n");

        return Result;
    }

    Result.Runtime = m3_NewRuntime(Result.Env, Kb(64), 0);
    if (!Result.Runtime)
    {
        LogCritical("m3_NewRuntime\n");

        return Result;
    }

    Result.Module = 0;
    Result.UpdateAndRender = 0;
    Result.Bytes = 0;

    Result.IsValid = True;
    return Result;
}

Bool HostLoadOne(Host *Host, const char *Path)
{
    Assert(Host);
    Assert(Path);
    Assert(Host->Env);

    Usize Size = 0;
    Host->Bytes = SDL_LoadFile(Path, &Size);
    if (!Host->Bytes)
    {
        LogCritical("%s\n", SDL_GetError());

        return False;
    }

    M3Result Result = m3_ParseModule(Host->Env, &Host->Module, (Uint8 *)Host->Bytes, (Uint32)Size);
    if (Result)
    {
        LogCritical("m3_ParseModule: %s\n", Result);

        return False;
    }

    Result = m3_LoadModule(Host->Runtime, Host->Module);
    if (Result)
    {
        LogCritical("m3_LoadModule: %s\n", Result);

        return False;
    }

    //
    // NOTE: Exports.
    //

    Result = m3_LinkRawFunction(Host->Module, "env", "Print", "v(ii)", &Print);
    // NOTE: functionLookupFailed here for some reason can happen if the module does not import it.
    if (Result && Result != m3Err_functionLookupFailed)
    {
        LogCritical("m3_LinkRawFunction  ('Print'): %s\n", Result);

        return False;
    }

    //
    // NOTE: Imports.
    //

#define Function(Name, Signature)                                \
    Result = m3_FindFunction(&Host->Name, Host->Runtime, #Name); \
    if (Result)                                                  \
    {                                                            \
        LogCritical("m3_FindFunction: %s\n"                      \
                    "NOTE: Host expects '%s' to be exported.\n", \
                    Result, #Signature);                         \
                                                                 \
        return False;                                            \
    }

    Function(UpdateAndRender, Void UpdateAndRender(*State, *RenderBuf));
    Function(GetState, State * GetState(Void));
    Function(GetExtraMem, Void * GetExtraMem(Void));
    Function(GetRenderBuf, RenderBuf * GetRenderBuf(Void));

#undef Function

    if (m3_CallV(Host->GetState) == m3Err_none)
    {
        m3_GetResultsV(Host->GetState, &Host->State);
    }
    if (m3_CallV(Host->GetRenderBuf) == m3Err_none)
    {
        m3_GetResultsV(Host->GetRenderBuf, &Host->RenderBuf);
    }
    if (m3_CallV(Host->GetExtraMem) == m3Err_none)
    {
        m3_GetResultsV(Host->GetExtraMem, &Host->ExtraMem);
    }

    return True;
}

Bool HostUpdate(Host *Host, State *State, RenderBuf *RenderBuf)
{
    Assert(Host);
    Assert(Host->UpdateAndRender);

    Uint32 TotalMemSize = 0;
    Uint8 *TotalMem = m3_GetMemory(Host->Runtime, &TotalMemSize, 0);
    if (!TotalMem)
    {
        LogCritical("m3_GetMemory\n");

        return False;
    }

    // NOTE: Coyp in.

    if (Host->State + sizeof(*State) <= TotalMemSize)
    {
        SDL_memcpy(TotalMem + Host->State, State, sizeof(*State));
    }

    if (Host->RenderBuf + 16 > TotalMemSize)
    {
        LogCritical("RenderBuf is out of bounds.\n");

        return False;
    }

    M3Result Result = m3_CallV(Host->UpdateAndRender, Host->State, Host->ExtraMem, Host->RenderBuf);
    if (Result)
    {
        M3ErrorInfo Info;
        m3_GetErrorInfo(Host->Runtime, &Info);

        if (Info.message && Info.message[0] != '\0')
        {
            LogCritical("m3_CallV  (runtime error): %s\n"
                        "Trace: %s\n",
                        Result,
                        Info.message);
        }
        else
        {
            LogCritical("m3_CallV  (runtime error): %s\n", Result);
        }
        return False;
    }

    // NOTE: Copy out.

    if (Host->State + sizeof(*State) <= TotalMemSize)
    {
        SDL_memcpy(State, TotalMem + Host->State, sizeof(*State));
    }

    // typedef struct
    // {
    //     Uint8 *Mem; 0-3
    //     Uint32 Size; 4-7
    //     Uint32 Cap; 8-11

    //     Bool IsValid; 12-15
    // } RenderBuf;
    Uint32 RenderBufMem = *(Uint32 *)(TotalMem + Host->RenderBuf + 0);
    Uint32 RenderBufSize = *(Uint32 *)(TotalMem + Host->RenderBuf + 4);

    if (RenderBufSize == 0)
    {
        return True;
    }

    if (RenderBufMem + RenderBufSize > TotalMemSize)
    {
        LogCritical("RenderBuf memory is out of bounds.\n");

        return False;
    }

    Void *Dest = RenderBufPush(RenderBuf, RenderBufSize);
    if (Dest)
    {
        SDL_memcpy(Dest, TotalMem + RenderBufMem, RenderBufSize);
        *(Uint32 *)(TotalMem + Host->RenderBuf + 4) = 0;
    }
    else
    {
        LogCritical("Out of memory.\n");

        return False;
    }

    return True;
}
