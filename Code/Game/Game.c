//
// NOTE: Main game file.
//
#include "Math.h"
#include <SDK.h>

UpdateAndRender(UpdateAndRender)
{
    if (!State->IsInitialized)
    {
        State->PermanentAlloc = MemAllocInit(ExtraMem, Mb(2));

        Uint32 Size = GetFileSize("GameAtlas.png");
        Void *Buf = MemAllocPush(&State->PermanentAlloc, Size);
        ReadFileCStr("GameAtlas.png", Buf, Size);
        State->SpriteAtlasTex = AllocTexture(Buf, Size);

        Size = GetFileSize("GameAtlas.txt");
        Buf = MemAllocPush(&State->PermanentAlloc, Size + 1);
        ReadFileCStr("GameAtlas.txt", Buf, Size);
        State->SpriteAtlas = AtlasInit(&State->PermanentAlloc, State->SpriteAtlasTex, Buf, Size);

        PrintCStr("(Game): Initialized");
        State->IsInitialized = True;
    }

    RenderBufClear(RenderBuf, Black);

    Rect Rect = RectMake(200.0f, 200.0f, 100.0f, 100.0f);

    Color Col = White;
    if (RectContainsV2(Rect, State->Input.MousePos))
    {
        Col = State->Input.MouseDown ? Red : Blue;
    }

    RenderBufDrawRect(RenderBuf, TexHandleInvalid, Rect, RectZero, Col);

    // RenderBufClear(RenderBuf, Black);
    // RenderBufDrawCStr(RenderBuf, White, V2Make(InternalWidth / 2.0f, InternalHeight / 2.0f), V2Make(2.0f, 2.0f), "Hello, World!\n");

    // Rect ExcitedMan = AtlasGetRect(&State->SpriteAtlas, "ExcitedMan.png");
    // RenderBufDrawRect(RenderBuf, State->SpriteAtlasTex, RectMake(100.0f, 100.0f, 100.0f, 100.0f), ExcitedMan, White);
}
