#include "SDL.h"

// 
// NOTE: Implementation
//

Void Update(SDL *App)
{
    Assert(App);
}

SDL Init()
{
    SDL Result = {0};

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        LogCritical("%s", SDL_GetError());
        Assert(0);
    }

    Result.Window = SDL_CreateWindow("Game", 1280, 720, SDL_WINDOW_RESIZABLE);
    if (!Result.Window)
    {
        LogCritical("%s", SDL_GetError());
        Assert(0);
    }

    return Result;
}

Void Render(SDL *App)
{
    Assert(App);
}

Bool Poll(SDL *App)
{
    Assert(App);

    // for (Usize I = 0; I < KeysCount; I++)
    // {
    //     Action *Action = App->Keys[I];

    //     if (Action)
    //     {
    //         Action->Pressed = False;
    //     }
    // }

    SDL_Event Ev;
    while (SDL_PollEvent(&Ev))
    {
        switch (Ev.type)
        {
        case SDL_EVENT_QUIT:
        {
            return False;
        }
        break;

            // case SDL_EVENT_MOUSE_MOTION:
            // {
            //     // TODO: Can we not do that to not do that
            //     SDL_ConvertEventToRenderCoordinates(App->Renderer.SDL, &Ev);
            //     App->State.Input.MousePos.X = Ev.motion.x;
            //     App->State.Input.MousePos.Y = Ev.motion.y;
            // }
            // break;

            // case SDL_EVENT_MOUSE_BUTTON_DOWN:
            // {
            //     Action *Action = GetMouseAction(App, Ev.button.button);

            //     if (Action)
            //     {
            //         Action->IsDown = True;
            //         Action->Pressed = True;
            //     }
            // }
            // break;

            // case SDL_EVENT_MOUSE_BUTTON_UP:
            // {
            //     Action *Action = GetMouseAction(App, Ev.button.button);

            //     if (Action)
            //     {
            //         Action->IsDown = False;
            //         Action->Released = True;
            //     }
            // }
            // break;

            // case SDL_EVENT_KEY_DOWN:
            // {
            //     SDL_Scancode Scancode = Ev.key.scancode;
            //     if (Scancode > 0 && Scancode < SDL_SCANCODE_COUNT)
            //     {
            //         Action *Action = App->Keys[Scancode];

            //         if (Action)
            //         {
            //             Action->IsDown = True;
            //             Action->Pressed = True;
            //         }
            //     }
            // }
            // break;

            // case SDL_EVENT_KEY_UP:
            // {
            //     SDL_Scancode Scancode = Ev.key.scancode;
            //     if (Scancode > 0 && Scancode < SDL_SCANCODE_COUNT)
            //     {
            //         Action *Action = App->Keys[Scancode];

            //         if (Action)
            //         {
            //             Action->IsDown = False;
            //             Action->Released = True;
            //         }
            //     }
            // }
            // break;

        default:
        {
            break;
        }
        }
    }

    return True;
}
