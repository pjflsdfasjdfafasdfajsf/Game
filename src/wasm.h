#include "inputmap.h"
#if !defined(WASM_H)
#define WASM_H

#include "wasm3.h"

#include "SDK.h"

namespace wasm {

struct Metadata {
    char name[64];
    char version[16];
    char summary[256];

    bool ok;
};

struct Module {
    IM3Runtime runtime;
    IM3Module module;

    IM3Function initialize;
    IM3Function update;
    IM3Function get_state;
    IM3Function get_buffer;

    // NOTE: Cached memory offsets.
    Uint32 state;
    Uint32 buffer;

    void *bytecode;
    Metadata metadata;

    /// Here means whether a mod is active or not.
    bool ok;
};

struct Context {
    IM3Environment environment;
    Module modules[512];
    Int32 module_count;

    char custom_action_names[CUSTOM_INPUT_MAX_ACTIONS][64];
    Int32 custom_action_count;

    inputmap::Binding default_bindings[CUSTOM_INPUT_MAX_ACTIONS];
    Int32 default_binding_count;

    void Update(State &state, RenderCommandBuffer &buffer);
};

/// DIRECTORY is where this function is looking for mods.
bool Initialize(Context *context, const char *directory);

} // namespace wasm

#endif
