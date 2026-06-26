//
// NOTE: Main game file.
//
#include "Mem.h"
#include <SDK.h>

// TODO: Put this in SDK

#define UIID Uint32

typedef struct
{
    // NOTE: Widget currently under the mouse
    UIID Hot;
    // NOTE: Widget that the user is currently interacting with
    UIID Active;
    // NOTE: Feedback count
    Uint32 Count;

    UIID Stack[32];
    Uint32 Depth;
} UIContext;

static inline UIID UIGetCurrentID(UIContext *UI, UIID BaseID)
{
    UIID Result = BaseID;
    for (Uint32 i = 0; i < UI->Depth; i++)
    {
        Result = HashCombine(Result, UI->Stack[i]);
    }
    return Result;
}

typedef struct
{
    V2 Pos;
    V2 Size;
    Float32 Spacing;
    // NOTE: Tracks item count across frames
    Uint32 *Track;
} UILayout;

static inline UILayout UILayoutBegin(V2 Start, V2 ItemSize, Float32 Spacing)
{
    UILayout Result = {0};

    Result.Pos = Start;
    Result.Size = ItemSize;
    Result.Spacing = Spacing;

    return Result;
}

static inline UILayout UILayoutBeginCenteredVertical(UIContext *UI, V2 Center, V2 ItemSize, Float32 Spacing)
{
    Uint32 Count = (UI->Count == 0) ? 1 : UI->Count;

    Float32 Height = ((Float32)Count * ItemSize.Y) + ((Float32)(Count - 1) * Spacing);
    V2 Start = V2Make(Center.X - (ItemSize.X / 2.0f), Center.Y - (Height / 2.0f));

    UILayout Layout = UILayoutBegin(Start, ItemSize, Spacing);
    Layout.Track = &UI->Count;
    *Layout.Track = 0;

    return Layout;
}
static inline Rect UILayoutNext(UILayout *Layout)
{
    Rect Result = RectMake2(Layout->Pos, Layout->Size);
    Layout->Pos.Y += Layout->Size.Y + Layout->Spacing;

    if (Layout->Track)
    {
        (*Layout->Track)++;
    }

    return Result;
}

static inline Bool UIButton(RenderBuf *RenderBuf, UIContext *UI, State *State, Rect Bounds, const char *CStr)
{
    UIID BaseID = HashCStr(CStr);
    UIID ID = UIGetCurrentID(UI, BaseID);

    Bool Clicked = False;
    Bool Hovered = RectContainsV2(Bounds, State->Input.MousePos);

    if (Hovered)
    {
        UI->Hot = ID;
    }
    else if (UI->Hot == ID)
    {
        UI->Hot = 0;
    }

    if (UI->Active == ID)
    {
        if (!State->Input.MouseDown)
        {
            if (UI->Hot == ID)
            {
                Clicked = True;
            }
            UI->Active = 0;
        }
    }
    else if (UI->Hot == ID && State->Input.MouseClicked)
    {
        UI->Active = ID;
    }

    V2 Pos = RectGetCenteredPos(Bounds, V2Make((Float32)CStrLen(CStr) * 16.0f, 16.0f));
    RenderBufDrawCStr(RenderBuf, White, Pos, V2Splat(2.0f), CStr);

    return Clicked;
}

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
    if (UIButton(RenderBuf, &UI, State, UILayoutNext(&Layout), "Play"))
    {
        PrintCStr("Hi :)");
    }
    if (UIButton(RenderBuf, &UI, State, UILayoutNext(&Layout), "Mods"))
    {
        PrintCStr("Hi :)");
    }
    if (UIButton(RenderBuf, &UI, State, UILayoutNext(&Layout), "Quit"))
    {
        PrintCStr("Hi :)");
    }
}
