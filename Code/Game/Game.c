//
// NOTE: Main game file.
//
#include <SDK.h>

UpdateAndRender(UpdateAndRender)
{
    // TODO: Fix segfault
    if (!State->IsInitialized)
    {
        State->PermanentAlloc = MemAllocInit(ExtraMem, Kb(32));

        State->SpriteAtlasTex = AllocTexture("GameAtlas.png");

        // TODO: No abs path :(
        const char *Meta = "/home/me/Workspace/Game/Code/Host/../../Assets/Images/ExcitedMan.png 0 0 275 183\n";
        Uint32 MetaLen = CStrLen(Meta);

        State->SpriteAtlas = AtlasInit(&State->PermanentAlloc, State->SpriteAtlasTex, Meta, MetaLen);

        PrintCStr("(Game): Initialized");
        State->IsInitialized = True;
    }

    RenderBufClear(RenderBuf, Black);
    RenderBufDrawCStr(RenderBuf, White, V2Make(10.0f, 10.0f), V2Make(2.0f, 2.0f), "Hello, World!\n");

    RenderBufDrawRect(RenderBuf, State->SpriteAtlasTex, RectMake(100.0f, 100.0f, 100.0f, 100.0f), AtlasGetRect(&State->SpriteAtlas, "ExcitedMan.png"), White);
}
