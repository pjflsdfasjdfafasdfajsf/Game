#if !defined(UI_H)
#define UI_H

#include "Types.h"
#include "Math.h"
#include "Render.h"
#include "State.h"

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

UIID UIGetCurrentID(UIContext *UI, UIID BaseID);

typedef struct
{
    V2 Pos;
    V2 Size;
    Float32 Spacing;
    // NOTE: Tracks item count across frames
    Uint32 *Track;
} UILayout;

UILayout UILayoutBegin(V2 Start, V2 ItemSize, Float32 Spacing);
UILayout UILayoutBeginCenteredVertical(UIContext *UI, V2 Center, V2 ItemSize, Float32 Spacing);

Rect UILayoutNext(UILayout *Layout);

Bool UIButton(RenderBuf *RenderBuf, UIContext *UI, State *State, Rect Bounds, const char *CStr);

#endif
