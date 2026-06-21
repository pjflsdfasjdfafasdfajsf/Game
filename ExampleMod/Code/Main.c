#include "SDK.h"

// TODO: @ttche Document this.

typedef struct
{
    Bool IsInitialized;
} ExampleModState;

UpdateAndRender(UpdateAndRender)
{
    ExampleModState *ExampleMod = (ExampleModState *)ExtraMem;
    if (!ExampleMod->IsInitialized)
    {
        PrintStr("(Example Mod): Initialized");
        ExampleMod->IsInitialized = True;
    }

    RenderBufClear(RenderBuf, Color3(255, 255, 0));
}
