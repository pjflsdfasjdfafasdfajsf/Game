#if !defined(HOST_RUNTIME_H)
#define HOST_RUNTIME_H

#include <wasm_export.h>

#include "Public/Types.h"

typedef struct
{
    wasm_module_t Module;
    wasm_module_inst_t ModuleInst;
    wasm_exec_env_t ExecEnv;

    wasm_function_inst_t UpdateAndRender;

    wasm_function_inst_t GetState;
    wasm_function_inst_t GetRenderBuf;
    wasm_function_inst_t GetExtraMem;

    void *Bytes;

    Bool IsValid;
} Runtime;

Bool RtGlobalInit(Void);
// NOTE: RtGlobalInit must have been called before.
Runtime RtInit(Void);
Void RtDeinit(Runtime *Rt);

Bool RtLoadOne(Runtime *Rt, const char *Path);

Bool RtUpdate(Runtime *Rt);

#endif
