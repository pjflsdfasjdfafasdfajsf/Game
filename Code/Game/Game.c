//
// NOTE: Main game file.
//
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

    // TODO: Put in state
    static UIContext UI = {0};

    UILayout Layout = UILayoutBeginCenteredVertical(&UI, ScreenCenter, V2Make(180.0f, 32.0f), 10.0f);
    if (UIButton(RenderBuf, &UI, &State->Input, UILayoutNext(&Layout), "Play"))
    {
        PrintCStr("Hi :)");
    }
    if (UIButton(RenderBuf, &UI, &State->Input, UILayoutNext(&Layout), "Mods"))
    {
        PrintCStr("Hi :)");
    }
    if (UIButton(RenderBuf, &UI, &State->Input, UILayoutNext(&Layout), "Quit"))
    {
        PrintCStr("Hi :)");
    }
}
