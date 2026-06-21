//
// NOTE: WASM host.
//

#include "Host.h"
#include "Render.h"
#include "SDL.h"
#include "Types.h"
#include "wasm3.h"

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
    // NOTE: Functions.
    //

    Result = m3_FindFunction(&Host->UpdateAndRender, Host->Runtime, "UpdateAndRender");
    if (Result)
    {
        LogCritical("m3_FindFunction: %s\n"
                    "NOTE: Host expects 'Void UpdateAndRender(*State, *RenderBuf)' to be exported.\n",
                    Result);

        return False;
    }

    Result = m3_FindFunction(&Host->GetState, Host->Runtime, "GetState");
    if (Result)
    {
        LogCritical("m3_FindFunction: %s\n"
                    "NOTE: Host expects 'State *GetState(Void)' to be exported.\n",
                    Result);

        return False;
    }

    Result = m3_FindFunction(&Host->GetRenderBuf, Host->Runtime, "GetRenderBuf");
    if (Result)
    {
        LogCritical("m3_FindFunction: %s\n"
                    "NOTE: Host expects 'RenderBuf *GetRenderBuf(Void)' to be exported.\n",
                    Result);

        return False;
    }

    if (m3_CallV(Host->GetState) == m3Err_none)
    {
        m3_GetResultsV(Host->GetState, &Host->State);
    }
    if (m3_CallV(Host->GetRenderBuf) == m3Err_none)
    {
        m3_GetResultsV(Host->GetRenderBuf, &Host->RenderBuf);
    }

    return True;
}

Bool HostUpdate(Host *Host, RenderBuf *RenderBuf)
{
    Assert(Host);
    Assert(Host->UpdateAndRender);

    M3Result Result = m3_CallV(Host->UpdateAndRender, Host->State, Host->RenderBuf);
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

    Uint32 TotalMemSize = 0;
    Uint8 *TotalMem = m3_GetMemory(Host->Runtime, &TotalMemSize, 0);
    if (!TotalMem)
    {
        LogCritical("m3_GetMemory\n");

        return False;
    }

    if (Host->RenderBuf + 16 > TotalMemSize)
    {
        LogCritical("RenderBuf is out of bounds.\n");

        return False;
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
    }
    else
    {
        LogCritical("Out of memory.\n");

        return False;
    }

    return True;
}
