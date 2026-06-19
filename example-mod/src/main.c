#include "SDK.h"

static Int32 miguel = -1;

Initialize {
    SetMetadata(
        "super jump", 
        "1.0.0", 
        "increases the player's jump height by 50%"
    );
    Log("guest says hello!");

    miguel = RegisterAction("miguel");
    RegisterDefaultKey("miguel", "f");
}

Update {
    if (state->input.custom[miguel].pressed) {
        Log("MIGUEL");
    }
    RenderCommandBuffer_DrawRectangle(buffer, Rectangle_FromFloats(20.0f, 20.0f, 100.0f, 100.0f), Color3(255, 255, 0));
}
