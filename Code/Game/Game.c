
#include "SDK.h"

#include "State.h"
#include "World.h"
#include "Menu.h"

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

        State->World = WorldInit(State);
        State->Menu = MenuInit(State);
        
        PrintCStr("(Game): Initialized");
        State->IsInitialized = True;
    }

    switch (State->GameState)
    {
    case GameState_Game:
    {
        WorldUpdateAndRender(RenderBuf, State);   
    } break;
    case GameState_Menu:
    {
        MenuUpdateAndRender(RenderBuf, State);   
    } break;
    }
}
