#include "Shared.h"
#include "Render.h"

// NOTE: This is compiled into every single mod internally.
// TODO: (Well, not right now, but you know, once mods can be compiled through
// the game)

static State _State = {0};
static Uint8 _Mem[Kb(64)];
static RenderBuf _RenderBuf = {_Mem, 0, sizeof(_Mem), True};

Export("GetState") State *GetState(Void)
{
    return &_State;
}

Export("GetRenderBuf") RenderBuf *GetRenderBuf(Void)
{
    return &_RenderBuf;
}

