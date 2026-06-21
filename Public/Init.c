#include "SDK.h"

// NOTE: This is compiled into every single mod internally.
// TODO: (Well, not right now, but you know, once mods can be compiled through
// the game)

static State _State = {0};

static Uint8 _ExtraMem[Kb(64)];

static Uint8 _RenderMem[Kb(64)];
static RenderBuf _RenderBuf = {_RenderMem, 0, sizeof(_RenderMem), True};

Export("GetExtraMem") Void *GetExtraMem(Void)
{
    return _ExtraMem;
}

Export("GetState") State *GetState(Void)
{
    return &_State;
}

Export("GetRenderBuf") RenderBuf *GetRenderBuf(Void)
{
    return &_RenderBuf;
}
