#if !defined(APP_H)
#define APP_H

#define INTERNAL

#include <SDL3/SDL.h>

#include "wasm.h"

#include "SDK.h"

struct App {
    SDL_Window *window;
    SDL_Renderer *renderer;
    RenderCommandBuffer buffer;

    wasm::Context *wasm;
    State state;
    /// Was this structure initialized succesfully.
    bool ok;
    /// Maps action -> scancode.
    Action *keys[SDL_SCANCODE_COUNT];

    static App Initialize();

    bool PollEvents();
    void Update();
    void Draw();
    void ClearInputEdges();
};

#endif
