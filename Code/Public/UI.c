#include "UI.h"

//
// NOTE: UIContext
// 

UIID UIGetCurrentID(UIContext *UI, UIID BaseID)
{
    UIID Result = BaseID;
    for (Uint32 i = 0; i < UI->Depth; i++)
    {
        Result = HashCombine(Result, UI->Stack[i]);
    }
    return Result;
}

//
// NOTE: UILayout
// 

UILayout UILayoutBegin(V2 Start, V2 ItemSize, Float32 Spacing)
{
    UILayout Result = {0};

    Result.Pos = Start;
    Result.Size = ItemSize;
    Result.Spacing = Spacing;

    return Result;
}

UILayout UILayoutBeginCenteredVertical(UIContext *UI, V2 Center, V2 ItemSize, Float32 Spacing)
{
    Uint32 Count = (UI->Count == 0) ? 1 : UI->Count;

    Float32 Height = ((Float32)Count * ItemSize.Y) + ((Float32)(Count - 1) * Spacing);
    V2 Start = V2Make(Center.X - (ItemSize.X / 2.0f), Center.Y - (Height / 2.0f));

    UILayout Layout = UILayoutBegin(Start, ItemSize, Spacing);
    Layout.Track = &UI->Count;
    *Layout.Track = 0;

    return Layout;
}

Rect UILayoutNext(UILayout *Layout)
{
    Rect Result = RectMake2(Layout->Pos, Layout->Size);
    Layout->Pos.Y += Layout->Size.Y + Layout->Spacing;

    if (Layout->Track)
    {
        (*Layout->Track)++;
    }

    return Result;
}

Bool UIButton(RenderBuf *RenderBuf, UIContext *UI, Input *Input, Rect Bounds, const char *CStr)
{
    UIID BaseID = HashCStr(CStr);
    UIID ID = UIGetCurrentID(UI, BaseID);

    Bool Clicked = False;
    Bool Hovered = RectContainsV2(Bounds, Input->MousePos);

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
        if (!Input->MouseDown)
        {
            if (UI->Hot == ID)
            {
                Clicked = True;
            }
            UI->Active = 0;
        }
    }
    else if (UI->Hot == ID && Input->MouseClicked)
    {
        UI->Active = ID;
    }

    V2 Pos = RectGetCenteredPos(Bounds, V2Make((Float32)CStrLen(CStr) * 16.0f, 16.0f));
    RenderBufDrawCStr(RenderBuf, White, Pos, V2Splat(2.0f), CStr);

    return Clicked;
}

