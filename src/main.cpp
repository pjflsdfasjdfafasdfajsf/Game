#include "app.h"
#include "inputmap.h"
#include "wasm.h"

#include <stdlib.h>

#include "SDK.h"

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

#define DEFAULT_MODS_DIRECTORY "mods"

static const char default_input_txt[] =
    "###########################################################################################\n"
    "#                                                                                         #\n"
    "# Default input bindings.                                                                 #\n"
    "# As you can see comments are supported via #.                                            #\n"
    "# Input is declared through (action) = (key) pairs, where                                 #\n"
    "# (action) can be:                                                                        #\n"
    "#    jump, left, slam, right                                                              #\n"
    "#                                                                                         #\n"
    "# (key) can be:                                                                           #\n"
    "#    w, a, s, d, space, f                                                                 #\n"
    "#                                                                                         #\n"
    "# You can declare multiple keys for one action by putting a comma inbetween the keys:     #\n"
    "#    action = w, a, s, d                                                                  #\n"
    "#                                                                                         #\n"
    "# On any key conflicts the action that was declared the latest wins over that key:        #\n"
    "#    actionX = w                                                                          #\n"
    "#    actionY = w                                                                          #\n"
    "# Key W in this case will be bound to actionY.                                            #\n"
    "#                                                                                         #\n"
    "# Any extra spaces on both sides of '=' and ',' are ignored:                              #\n"
    "#    actionX   = keyX  , keyY                                                             #\n"
    "#    actionXYZ = keyFoo, keyBar                                                           #\n"
    "# IMPORTANT: Trailing commas are not allowed.                                             #\n"
    "#                                                                                         #\n"
    "# Examples:                                                                               #\n"
    "#    jump = space, w                                                                      #\n"
    "#    left = a                                                                             #\n"
    "#    right = d                                                                            #\n"
    "#    slam = s                                                                             #\n"
    "#                                                                                         #\n"
    "# IMPORTANT: Spaces are NOT supported for action names. Use snake_case for your actions,  #\n"
    "# the _ are later replaced with spaces when they need to be displayed.                    #\n"
    "#                                                                                         #\n"
    "###########################################################################################\n"
    "\n"
    "jump  = w, space\n"
    "left  = a\n"
    "slam  = s\n"
    "right = d\n"
    "hook  = e\n"
    "dash  = left shift\n";

const char *GetAbsolutePath(const char *file)
{
    static char buffer[1024];
    const char *base = SDL_GetBasePath();

    if (!base)
    {
        SDL_Log("SDL_GetBasePath failed: %s\n", SDL_GetError());
        SDL_snprintf(buffer, sizeof(buffer), "%s", file);

        return buffer;
    }

    SDL_snprintf(buffer, sizeof(buffer), "%s%s", base, file);

    return buffer;
}

void WriteDefaultInputConfig(void)
{
    Bool exists = SDL_IOFromFile(GetAbsolutePath("input.txt"), "r");
    if (exists)
    {
        SDL_Log("input.txt already exists\n");
        return;
    }

    SDL_IOStream *io = SDL_IOFromFile(GetAbsolutePath("input.txt"), "w");
    if (!io)
    {
        return;
    }

    size_t length = sizeof(default_input_txt) - 1;
    size_t written = SDL_WriteIO(io, default_input_txt, length);

    Assert(written == length);

    SDL_CloseIO(io);
}

float GetDeltaSeconds()
{
    static Uint64 last_frame_ticks = SDL_GetTicks();

    Uint64 this_frame_ticks = SDL_GetTicks();
    float delta_seconds = (this_frame_ticks - last_frame_ticks) / 1000.0f;
    last_frame_ticks = this_frame_ticks;

    return delta_seconds;
}

static Action *GetActionByName(State &state, wasm::Context &wasm, const char *name)
{
    Assert(name);

    if (SDL_strcmp(name, "jump") == 0)
    {
        return &state.input.jump;
    }
    if (SDL_strcmp(name, "dash") == 0)
    {
        return &state.input.dash;
    }
    if (SDL_strcmp(name, "slam") == 0)
    {
        return &state.input.slam;
    }
    if (SDL_strcmp(name, "hook") == 0)
    {
        return &state.input.hook;
    }
    if (SDL_strcmp(name, "left") == 0)
    {
        return &state.input.left;
    }
    if (SDL_strcmp(name, "right") == 0)
    {
        return &state.input.right;
    }

    for (Int32 i = 0; i < wasm.custom_action_count; ++i)
    {
        if (SDL_strcmp(wasm.custom_action_names[i], name) == 0)
        {
            return &state.input.custom[i];
        }
    }

    return 0;
}

static void ApplyBindings(App &app, inputmap::Binding *bindings, Uint32 count, const char *source)
{
    if (count == 0)
    {
        return;
    }

    Assert(bindings);

    for (Uint32 i = 0; i < count; i++)
    {
        inputmap::Binding *binding = &bindings[i];
        SDL_Scancode scancode = SDL_GetScancodeFromName(binding->key);

        if (scancode == SDL_SCANCODE_UNKNOWN)
        {
            SDL_Log("unknown key '%s' (from %s)\n", binding->key, source);
            continue;
        }

        Action *action = GetActionByName(app.state, *app.wasm, binding->action);
        if (!action)
        {
            SDL_Log("unknown action '%s' (from %s)\n", binding->action, source);
            continue;
        }

        SDL_Log("%s -> %s", binding->key, binding->action);

        app.keys[scancode] = action;
    }
}

static Bool IsActionBound(inputmap::Bindings *bindings, const char *action)
{
    for (Uint32 i = 0; i < bindings->count; i++)
    {
        if (SDL_strcmp(bindings->items[i].action, action) == 0)
        {
            return true;
        }
    }
    return false;
}

static Bool AppendMissingBinding(SDL_IOStream **stream, inputmap::Bindings *user_bindings, const char *action, const char *key)
{
    if (IsActionBound(user_bindings, action))
    {
        return false;
    }

    if (!*stream)
    {
        *stream = SDL_IOFromFile(GetAbsolutePath("input.txt"), "a");

        if (!*stream)
        {
            SDL_Log("SDL_IOFromFile: %s\n", SDL_GetError());
            return false;
        }
    }

    char line[256];
    Int32 length = SDL_snprintf(line, sizeof(line), "%s = %s\n", action, key);
    Usize written = SDL_WriteIO(*stream, line, length);
    Assert(written == (Usize)length);

    return true;
}

// Platform Api Functions

Bool FileSave(const char *file, const void *data, Uint32 size)
{
    return SDL_SaveFile(file, data, size);
}

void *FileLoad(const char* file, Uint32 *out_size)
{
    Usize size = 0;
    void *data = SDL_LoadFile(file, &size);
    return data;    
}

void FileFree(void *data)
{
    if (data)
    {
        SDL_free(data);
    }
}

/////////////////////////////

App App::Initialize()
{
    App app = {};

    SDL_Log("base path: %s\n", SDL_GetBasePath());
    SDL_Log("looking for mods in: %s\n", GetAbsolutePath(DEFAULT_MODS_DIRECTORY));

    SDL_Log("state struct size: %lu", sizeof(State));

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init: %s\n", SDL_GetError());
        return app;
    }

    if (!SDL_CreateWindowAndRenderer("tiramisu", DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &app.window, &app.renderer))
    {
        SDL_Log("SDL_CreateWindowAndRenderer: %s\n", SDL_GetError());
        return app;
    }

    app.wasm = (wasm::Context *)SDL_calloc(1, sizeof(wasm::Context));
    if (!wasm::Initialize(app.wasm, GetAbsolutePath(DEFAULT_MODS_DIRECTORY)))
    {
        return app;
    }

    SDL_SetRenderLogicalPresentation(app.renderer, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);

    app.buffer.capacity = Kilobytes(64);
    app.buffer.memory = (Uint8 *)SDL_malloc(app.buffer.capacity);

    if (!app.buffer.memory)
    {
        return app;
    }

    app.platform.GetAbsoultePath = GetAbsolutePath;
    app.platform.FileSave = FileSave;
    app.platform.FileLoad = FileLoad;
    app.platform.FileFree = FileFree;

    app.state = State::Initialize(app.platform);
    if (!app.state.ok)
    {
        return app;
    }

    // NOTE: I don't want to segfault on unmapped keys :(
    for (Int32 i = 0; i < SDL_SCANCODE_COUNT; i++)
    {
        app.keys[i] = NULL;
    }

    inputmap::Bindings bindings = inputmap::Bindings::Initialize(128);
    WriteDefaultInputConfig();

    ApplyBindings(app, app.wasm->default_bindings, (Uint32)app.wasm->default_binding_count, "RegisterDefaultKey call");

    Usize file_size = 0;
    char *file_content = (char *)SDL_LoadFile(GetAbsolutePath("input.txt"), &file_size);
    // NOTE: Since we have written the default input config it should not fail.
    Assert(file_content);

    bindings.Read(file_content);

    ApplyBindings(app, bindings.items, bindings.count, "input.txt");

    SDL_IOStream *stream = 0;

    char *default_input_txt_copy = SDL_strdup(default_input_txt);
    if (default_input_txt_copy)
    {
        inputmap::Bindings defaults = inputmap::Bindings::Initialize(128);
        defaults.Read(default_input_txt_copy);

        for (Uint32 i = 0; i < defaults.count; i++)
        {
            inputmap::Binding *binding = &defaults.items[i];

            if (AppendMissingBinding(&stream, &bindings, binding->action, binding->key))
            {
                ApplyBindings(app, binding, 1, "Appended Engine Default");
            }
        }
        SDL_free(default_input_txt_copy);
    }

    for (Int32 i = 0; i < app.wasm->default_binding_count; i++)
    {
        inputmap::Binding *binding = &app.wasm->default_bindings[i];
        AppendMissingBinding(&stream, &bindings, binding->action, binding->key);
    }

    if (stream)
    {
        SDL_CloseIO(stream);
        SDL_Log("updated your config\n");
    }

    app.ok = true;
    return app;
}

void AssertDidNotReadPastEnd(Uint8 *current, Uint8 *end, Uint64 size)
{
    if (current + size > end)
    {
        Assert(0);
    }
}

void App::Draw()
{
    Uint8 *current = buffer.memory;
    Uint8 *end = buffer.memory + buffer.size;

    while (current < end)
    {
        RenderCommandType type = *(RenderCommandType *)current;

        switch (type)
        {
        case RenderCommandType_ClearScreen:
        {
            AssertDidNotReadPastEnd(current, end, sizeof(RenderCommand_ClearScreen));

            RenderCommand_ClearScreen *command = (RenderCommand_ClearScreen *)current;
            SDL_SetRenderDrawColor(renderer, command->color.red, command->color.green, command->color.blue, command->color.alpha);
            SDL_RenderClear(renderer);

            current += sizeof(RenderCommand_ClearScreen);
        }
        break;

        case RenderCommandType_DrawRectangle:
        {
            AssertDidNotReadPastEnd(current, end, sizeof(RenderCommand_DrawRectangle));

            RenderCommand_DrawRectangle *command = (RenderCommand_DrawRectangle *)current;
            SDL_FRect rectangle = {
                command->rectangle.position.x,
                command->rectangle.position.y,
                command->rectangle.size.width,
                command->rectangle.size.height};

            SDL_SetRenderDrawColor(renderer, command->color.red, command->color.green, command->color.blue, command->color.alpha);
            SDL_RenderFillRect(renderer, &rectangle);

            current += sizeof(RenderCommand_DrawRectangle);
        }
        break;

        case RenderCommandType_DrawLine:
        {
            AssertDidNotReadPastEnd(current, end, sizeof(RenderCommand_DrawLine));

            RenderCommand_DrawLine *command = (RenderCommand_DrawLine *)current;
            SDL_SetRenderDrawColor(renderer, command->color.red, command->color.green, command->color.blue, command->color.alpha);
            SDL_RenderLine(renderer, command->start.x, command->start.y, command->end.x, command->end.y);

            current += sizeof(RenderCommand_DrawLine);
        }
        break;

        case RenderCommandType_None:
        default:
        {
            SDL_Log("CORRUPTED RENDER BUFFER OR A NOT IMPLEMENTED COMMAND (%d)\n", type);
            Assert(0 && "Invalid render command");
            current = end;
        }
        break;
        }
    }

    SDL_RenderPresent(renderer);
    buffer.size = 0;
}

void App::ClearInputEdges()
{
    this->state.input.jump.pressed = false;
    this->state.input.jump.released = false;

    this->state.input.dash.pressed = false;
    this->state.input.dash.released = false;

    this->state.input.slam.pressed = false;
    this->state.input.slam.released = false;

    this->state.input.hook.pressed = false;
    this->state.input.hook.released = false;

    this->state.input.left.pressed = false;
    this->state.input.left.released = false;

    this->state.input.right.pressed = false;
    this->state.input.right.released = false;

    for (Int32 i = 0; i < this->wasm->custom_action_count; i++)
    {
        this->state.input.custom[i].pressed = false;
        this->state.input.custom[i].released = false;
    }
}

Bool App::PollEvents()
{
    this->ClearInputEdges();

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_EVENT_QUIT:
        {
            return false;
        }
        break;

        case SDL_EVENT_KEY_DOWN:
        {
            if (event.key.repeat)
            {
                break;
            }
            SDL_Scancode scancode = event.key.scancode;
            if (scancode >= 0 && scancode < SDL_SCANCODE_COUNT)
            {
                Action *action = this->keys[scancode];

                if (action)
                {
                    action->is_down = true;
                    action->pressed = true;
                }
            }
        }
        break;

        case SDL_EVENT_KEY_UP:
        {
            SDL_Scancode scancode = event.key.scancode;
            if (scancode >= 0 && scancode < SDL_SCANCODE_COUNT)
            {
                Action *action = this->keys[scancode];

                if (action)
                {
                    action->is_down = false;
                    action->released = true;
                }
            }
        }
        break;

        default:
        {
        }
        break;
        }
    }
    return true;
}

void App::Update()
{
    this->state.time.delta = GetDeltaSeconds();

    int width, height;
    SDL_GetRenderOutputSize(this->renderer, &width, &height);
    this->state.camera.viewport = V2i(width, height);
}

int main()
{
    App app = App::Initialize();
    if (!app.ok)
    {
        return EXIT_FAILURE;
    }

    while (app.PollEvents())
    {
        app.Update();
        app.state.Update(app.platform);
        app.Draw();

        app.state.Draw(app.buffer, app.platform);
        app.wasm->Update(app.state, app.buffer);
    }

    return EXIT_SUCCESS;
}
