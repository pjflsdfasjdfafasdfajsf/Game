//
// NOTE: Main game file.
//
#include <SDK.h>

UpdateAndRender(UpdateAndRender)
{
    if (!State->IsInitialized)
    {
        State->PermanentAlloc = MemAllocInit(ExtraMem, Mb(2));

        Uint32 Size = ReadFileCStr("GameAtlas.png", 0, 0);
        Void *Buf = MemAllocPush(&State->PermanentAlloc, Size);
        ReadFileCStr("GameAtlas.png", Buf, Size);
        State->SpriteAtlasTex = AllocTexture(Buf, Size);

        Size = ReadFileCStr("GameAtlas.txt", 0, 0);
        Buf = MemAllocPush(&State->PermanentAlloc, Size + 1);
        ReadFileCStr("GameAtlas.txt", Buf, Size);
        State->SpriteAtlas = AtlasInit(&State->PermanentAlloc, State->SpriteAtlasTex, Buf, Size);

        PrintCStr("(Game): Initialized");
        State->IsInitialized = True;
    }

    RenderBufClear(RenderBuf, Black);

    Float32 X = State->Input.MousePos.X;
    Float32 Y = State->Input.MousePos.Y;

    // TODO: IsPointInRect
    Bool Hover = (X >= 200.0f && X <= 300.0f && Y >= 200.0f && Y <= 300.0f);

    Color Col = White;
    if (Hover)
    {
        Col = State->Input.MouseDown ? Red : Blue;
    }

    RenderBufDrawRect(RenderBuf, TexHandleInvalid, RectMake(200.0f, 200.0f, 100.0f, 100.0f), RectZero, Col);

    // RenderBufClear(RenderBuf, Black);
    // RenderBufDrawCStr(RenderBuf, White, V2Make(InternalWidth / 2.0f, InternalHeight / 2.0f), V2Make(2.0f, 2.0f), "Hello, World!\n");

    // Rect ExcitedMan = AtlasGetRect(&State->SpriteAtlas, "ExcitedMan.png");
    // RenderBufDrawRect(RenderBuf, State->SpriteAtlasTex, RectMake(100.0f, 100.0f, 100.0f, 100.0f), ExcitedMan, White);
}
