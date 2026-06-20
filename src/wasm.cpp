#include "wasm.h"
#include "SDK.h"
#include "SDL3/SDL_stdinc.h"
#include "m3_core.h"
#include "m3_env.h"
#include "wasm3.h"
#include <SDL3/SDL.h>

namespace wasm
{

//
// NOTE: Host functions.
//

static m3ApiRawFunction(HostLog)
{
    m3ApiGetArgMem(const char *, message);

    Module *module = (Module *)_ctx->userdata;
    const char *name = module->metadata.ok ? module->metadata.name : "unknown";

    if (message)
    {
        SDL_Log("(%s): %s\n", name, message);
    }

    m3ApiSuccess();
}

static m3ApiRawFunction(HostSetMetadata)
{
    m3ApiGetArgMem(const char *, name);
    m3ApiGetArgMem(const char *, version);
    m3ApiGetArgMem(const char *, summary);

    Module *module = (Module *)_ctx->userdata;

    if (name && version && summary)
    {
        SDL_strlcpy(module->metadata.name, name, sizeof(module->metadata.name));
        SDL_strlcpy(module->metadata.version, version, sizeof(module->metadata.version));
        SDL_strlcpy(module->metadata.summary, summary, sizeof(module->metadata.summary));
        module->metadata.ok = true;
    }

    m3ApiSuccess();
}

static m3ApiRawFunction(HostRegisterAction)
{
    m3ApiReturnType(Int32);
    m3ApiGetArgMem(const char *, name);

    Context *context = (Context *)_ctx->userdata;
    Assert(context);

    if (!name)
    {
        m3ApiReturn(-1);
    }

    for (Int32 i = 0; i < context->custom_action_count; ++i)
    {
        if (SDL_strcmp(context->custom_action_names[i], name) == 0)
        {
            m3ApiReturn(i);
        }
    }

    if (context->custom_action_count >= (Int32)SDL_arraysize(context->custom_action_names))
    {
        SDL_Log("too many actions requested (dropping '%s')\n", name);
        m3ApiReturn(-1);
    }

    Int32 index = context->custom_action_count;
    SDL_strlcpy(context->custom_action_names[index], name, sizeof(context->custom_action_names[index]));
    context->custom_action_count++;

    m3ApiReturn(index);
}

static m3ApiRawFunction(HostRegisterDefaultKey)
{
    m3ApiGetArgMem(const char *, action);
    m3ApiGetArgMem(const char *, key);

    Context *context = (Context *)_ctx->userdata;
    Assert(context);

    if (!action || !key)
    {
        m3ApiSuccess();
    }

    if (context->default_binding_count >= (Int32)SDL_arraysize(context->default_bindings))
    {
        SDL_Log("too many bindings requested (dropping '%s' -> '%s')\n", action, key);
        m3ApiSuccess();
    }

    Int32 index = context->default_binding_count;
    SDL_strlcpy(context->default_bindings[index].action, action, sizeof(context->default_bindings[index].action));
    SDL_strlcpy(context->default_bindings[index].key, key, sizeof(context->default_bindings[index].key));
    context->default_binding_count++;

    m3ApiSuccess();
}

//
// NOTE: Implementation.
//

static Bool LoadModule(Context *context, const char *file_path)
{
    if (context->module_count >= (Int32)SDL_arraysize(context->modules))
    {
        SDL_Log("maximum module capacity reached\n");
        return false;
    }

    Module *module = &context->modules[context->module_count];
    SDL_zero(*module);

    Usize file_size = 0;
    module->bytecode = SDL_LoadFile(file_path, &file_size);
    if (!module->bytecode)
    {
        SDL_Log("SDL_LoadFile  (for '%s'): %s\n", file_path, SDL_GetError());
        return false;
    }

    module->runtime = m3_NewRuntime(context->environment, Kilobytes(64), 0);
    if (!module->runtime)
    {
        SDL_Log("m3_NewRuntime (for '%s')\n", file_path);
        return false;
    }

    M3Result result = m3_ParseModule(context->environment, &module->module, (uint8_t *)module->bytecode, (uint32_t)file_size);
    if (result)
    {
        SDL_Log("m3_ParseModule  (for '%s'): %s\n", file_path, result);
        return false;
    }

    result = m3_LoadModule(module->runtime, module->module);
    if (result)
    {
        SDL_Log("m3_LoadModule  (for '%s'): %s\n", file_path, result);
        return false;
    }

    m3_LinkRawFunctionEx(module->module, "env", "host_log", "v(i)", &HostLog, module);
    m3_LinkRawFunctionEx(module->module, "env", "host_set_metadata", "v(iii)", &HostSetMetadata, module);
    m3_LinkRawFunctionEx(module->module, "env", "host_register_action", "i(i)", &HostRegisterAction, context);
    m3_LinkRawFunctionEx(module->module, "env", "host_register_default_key", "v(ii)", &HostRegisterDefaultKey, context);

    m3_FindFunction(&module->update, module->runtime, "guest_update");
    m3_FindFunction(&module->initialize, module->runtime, "guest_initialize");
    m3_FindFunction(&module->get_state, module->runtime, "guest_get_state");
    m3_FindFunction(&module->get_buffer, module->runtime, "guest_get_buffer");

    if (module->initialize)
    {
        result = m3_CallV(module->initialize);
        if (result)
        {
            SDL_Log("m3_CallV  (initialization for '%s'): %s\n", file_path, result);
            return false;
        }

        if (module->metadata.ok)
        {
            SDL_Log("initialized: %s (version %s)\n", module->metadata.name, module->metadata.version);
            SDL_Log(" %s\n\n", module->metadata.summary);
        }
        else
        {
            SDL_Log("initialized: %s which provided no metadata\n", file_path);
        }
    }

    if (module->get_state)
    {
        m3_CallV(module->get_state);
        m3_GetResultsV(module->get_state, &module->state);
    }

    if (module->get_buffer)
    {
        m3_CallV(module->get_buffer);
        m3_GetResultsV(module->get_buffer, &module->buffer);
    }

    module->ok = true;
    context->module_count++;

    return true;
}

static SDL_EnumerationResult SDLCALL EnumerateDirectoryCallback(void *userdata, const char *directory_name, const char *file_name)
{
    Context *context = (Context *)userdata;

    const char *extension = SDL_strrchr(file_name, '.');
    if (extension && SDL_strcmp(extension, ".wasm") == 0)
    {
        char buffer[1024];
        SDL_snprintf(buffer, sizeof(buffer), "%s/%s", directory_name, file_name);
        LoadModule(context, buffer);
    }

    return SDL_ENUM_CONTINUE;
}

Bool Initialize(Context *context, const char *directory)
{
    Assert(context != nullptr);

    context->environment = m3_NewEnvironment();
    if (!context->environment)
    {
        SDL_Log("m3_NewEnvironment\n");
        return false;
    }

    if (!SDL_CreateDirectory(directory))
    {
        SDL_Log("SDL_CreateDirectory: %s\n", SDL_GetError());
        return false;
    }

    if (!SDL_EnumerateDirectory(directory, EnumerateDirectoryCallback, context))
    {
        SDL_Log("SDL_EnumerateDirectory: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

void Context::Update(State &state, RenderCommandBuffer &buffer)
{
    for (Int32 i = 0; i < this->module_count; i++)
    {
        Module *module = &this->modules[i];

        if (!module->ok || !module->update)
        {
            continue;
        }

        Uint32 memory_size = 0;
        Uint8 *wasm_memory = (Uint8 *)m3_GetMemory(module->module, &memory_size, 0);

        if (!wasm_memory || memory_size == 0)
        {
            continue;
        }

        // NOTE: 0 is a valid memory offset in Wasm so it's better to check if the function exists.
        if (module->get_state)
        {
            Bool is_state_in_bounds = (module->state + sizeof(State)) <= memory_size;
            if (is_state_in_bounds)
            {
                SDL_memcpy(wasm_memory + module->state, &state, sizeof(State));
            }
            else
            {
                SDL_Log("module '%s' state pointer is out of bounds\n", module->metadata.name);
                module->ok = false;
                continue;
            }
        }

        M3Result result = m3_CallV(module->update);
        if (result)
        {
            const char *name = module->metadata.ok ? module->metadata.name : "unknown";
            SDL_Log("m3_CallV  (runtime error in '%s'): %s\n", name, result);
            module->ok = false;
            continue;
        }

        if (!module->get_buffer)
        {
            continue;
        }

        Bool is_buffer_struct_in_bounds = (module->buffer + sizeof(RenderCommandBuffer)) <= memory_size;
        if (!is_buffer_struct_in_bounds)
        {
            SDL_Log("module '%s' buffer pointer is out of bounds\n", module->metadata.name);
            module->ok = false;
            continue;
        }

        RenderCommandBuffer *local_buffer = (RenderCommandBuffer *)(wasm_memory + module->buffer);
        Assert(local_buffer != 0);

        if (local_buffer->size == 0)
        {
            continue;
        }

        Uint32 wasm_memory_offset = (Uint32)(Uintptr)local_buffer->memory;
        Bool is_buffer_memory_in_bounds = (wasm_memory_offset + local_buffer->size) <= memory_size;

        if (!is_buffer_memory_in_bounds)
        {
            SDL_Log("module '%s' render commands are out of bounds\n", module->metadata.name);
            module->ok = false;
            local_buffer->size = 0;

            continue;
        }

        void *host_destination = RenderCommandBuffer_Push(&buffer, local_buffer->size);

        if (!host_destination)
        {
            SDL_Log("host render buffer capacity exceeded by module '%s'\n", module->metadata.name);
            local_buffer->size = 0;

            continue;
        }

        SDL_memcpy(host_destination, wasm_memory + wasm_memory_offset, local_buffer->size);
        local_buffer->size = 0;
    }
}

} // namespace wasm