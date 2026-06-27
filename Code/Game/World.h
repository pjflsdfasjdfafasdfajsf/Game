
#if !defined(WORLD_H)
#define WORLD_H

#include "State.h"

World WorldInit(State *State);
Void WorldUpdateAndRender(RenderBuf *RenderBuf, State *State);

#endif
