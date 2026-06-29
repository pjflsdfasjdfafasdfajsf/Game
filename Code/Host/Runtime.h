#if !defined(HOST_RUNTIME_H)
#define HOST_RUNTIME_H

#include <WAMR/wasm_export.h>

#include "Public/Types.h"

typedef struct
{
    wasm_module_t Module;
    wasm_module_inst_t ModuleInst;
    wasm_exec_env_t ExecEnv;

    wasm_function_inst_t Init;
    wasm_function_inst_t UpdateAndRender;

    void *Bytes;

    Bool IsValid;
} Runtime;

Bool RtGlobalInit(Void);
// NOTE: RtGlobalInit must have been called before.
Runtime RtInit(Void);
Void RtDeinit(Runtime *Rt);

Bool RtLoadOne(Runtime *Rt, const char *Path);

Bool RtCallUpdate(Runtime *Rt);
Bool RtCallInit(Runtime *Rt);

#endif
