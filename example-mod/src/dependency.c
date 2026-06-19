#include "SDK.h"

Initialize {
    SetMetadata(
        "dependency",
        "1.0.0",
        "this is dependency for super-jump mod");
    Log("dependency is here too!");
}

Update {
    RenderCommandBuffer_DrawRectangle(buffer, Rectangle_FromFloats(130.0f, 130.0f, 100.0f, 100.0f), Color3(255, 255, 0));
}
