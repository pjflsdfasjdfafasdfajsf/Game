//
// NOTE: Main game file.
//
#include <SDK.h>

#include "GameAtlas.h"

UpdateAndRender(UpdateAndRender)
{
    if (!State->IsInitialized)
    {
        State->PermanentAlloc = MemAllocInit(ExtraMem, Kb(16));

        State->SpriteAtlasTex = AllocTexture(GenGameAtlas, GenGameAtlasLen);
        State->SpriteAtlas = AtlasInit(&State->PermanentAlloc, State->SpriteAtlasTex, GenGameAtlasMeta, GenGameAtlasMetaLen);

        // ReadFileCStr("Hello.txt", 0, 0);

        PrintCStr("(Game): Initialized");
        State->IsInitialized = True;
   }

    RenderBufClear(RenderBuf, Black);
    RenderBufDrawCStr(RenderBuf, White, V2Make(10.0f, 10.0f), V2Make(2.0f, 2.0f), "Hello, World!\n");

    RenderBufDrawRect(RenderBuf, State->SpriteAtlasTex, RectMake(100.0f, 100.0f, 100.0f, 100.0f), AtlasGetRect(&State->SpriteAtlas, "ExcitedMan.png"), White);
}
