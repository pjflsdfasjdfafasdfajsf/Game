#include "Render.h"
#include "Shared.h"

// TODO: Abstract
// TODO: can we get rid of this? global state bad.
static State _State = {0};
static Uint8 _Mem[Kb(64)];
static RenderBuf _RenderBuf = {_Mem, 0, sizeof(_Mem)};

Export("GetState") State *GetState(Void)
{
    return &_State;
}

Export("GetRenderBuf") RenderBuf *GetRenderBuf(Void)
{
    return &_RenderBuf;
}

UpdateAndRender(UpdateAndRender)
{
    RenderBufClear(RenderBuf, Color3(255, 255, 0));
}
