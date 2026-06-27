#include <SDK.h>

Menu MenuInit(State *State)
{
    Menu Result = {0};

    return Result;
}

Void MenuUpdateAndRender(RenderBuf *RenderBuf, State *State)
{
    Menu *Menu = &State->Menu;

    UILayout Layout = UILayoutBeginCenteredVertical(&Menu->UI, ScreenCenter, V2Make(180.0f, 32.0f), 10.0f);
    if (UIButton(RenderBuf, &Menu->UI, &State->Input, UILayoutNext(&Layout), "Play"))
    {
        PrintCStr("Hi :)");
    }
    if (UIButton(RenderBuf, &Menu->UI, &State->Input, UILayoutNext(&Layout), "Mods"))
    {
        PrintCStr("Hi :)");
    }
    if (UIButton(RenderBuf, &Menu->UI, &State->Input, UILayoutNext(&Layout), "Quit"))
    {
        PrintCStr("Hi :)");
    }

    if (State->Input.RMB.Pressed)
    {
        State->GameState = GameState_Game;
    }
}
