#include "SDK.h"

// You can store the state that you need to be persistent across frames.
typedef struct
{
    // Don't forget this! If you have any state you also need this boolean to actually initialize your state as the engine
    // does not search for any special 'initialize' functions as we're good programmers and keep the code simple, and so should you.
    Bool IsInitialized;
} ExampleModState;

// This UpdateAndRender functions gets called by the engine every frame.
// It does not matter what you put in the parenthesis as the underlying macro anyways exports the function under the name of 'UpdateAndRender'.
// UpdateAndRender(FooBar) would still work perfectly fine.
// 
// ExtraMem is, as you can say from the name, just an extra block of memory kindly provided to you by the engine.
// You can do whatever you want with it, but your use of it probably won't go past getting the mod state you defined above.  
UpdateAndRender(UpdateAndRender)
{
    ExampleModState *ExampleMod = (ExampleModState *)ExtraMem;
    // This works since the provided ExtraMem is zeroed out, so you can have your own sort of initialization function without actually exporting one.
    // Don't forget to set IsInitialized to true though!
    if (!ExampleMod->IsInitialized)
    {
        // The PrintStr function does not prefix your logs with anything, and that's the exact reason why you should prepend some prefix to your logs.
        // You can go even further and prepend the logging level:
        // PrintStr("(Example Mod): INFO: Initialized")
        // You don't need to include the newline, it's already appended for you.
        PrintStr("(Example Mod): Initialized");
        ExampleMod->IsInitialized = True;
    }
    // The Renderer API should be pretty self-explanatory. If you're wondering how it works you can look at Render.h SDK header.
    // Everything else that isn't there (where 'everything else' is just command execution) is hidden in our engine.
    RenderBufClear(RenderBuf, Color3(255, 255, 0));
}
