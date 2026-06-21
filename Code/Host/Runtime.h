#if !defined(HOST_H)
#define HOST_H

#include "wasm_export.h"

#include "SDK.h"

typedef struct
{
    wasm_module_t Module;
    wasm_module_inst_t ModuleInst;
    wasm_exec_env_t ExecEnv;

    wasm_function_inst_t UpdateAndRender;

    wasm_function_inst_t GetState;
    wasm_function_inst_t GetRenderBuf;
    wasm_function_inst_t GetExtraMem;

    // NOTE: NOT REAL HOST POINTERS!!!!
    Uint32 State;
    Uint32 RenderBuf;
    Uint32 ExtraMem;

    void *Bytes;

    Bool IsValid;
} Runtime;

Runtime RtInit(Void);
Void RtDeinit(Runtime *Rt);

Bool RtLoadOne(Runtime *Rt, const char *File);

Bool RtUpdate(Runtime *Rt, State *RuntimeState, RenderBuf *RuntimeRenderBuf);

#endif
