//
// NOTE: Game state.
//
#if !defined(STATE_H)
#define STATE_H

#include "Math.h"
#include "Mem.h"
#include "Render.h"
#include "Types.h"

typedef struct Input
{
    V2 MousePos;

    // TODO: Struct Button
    Bool MouseDown;
    // NOTE: True for exactly one frame on press
    Bool MouseClicked;
} Input;

// NOTE: ANYTHING THAT IS MODIFIED BY HOST MUST BE THE AT THE VERY TOP OF THIS
// STRUCT!!!

typedef struct State
{
    Input Input;

    MemAlloc PermanentAlloc;

    TexHandle SpriteAtlasTex;
    Atlas SpriteAtlas;

    Bool IsInitialized;
} State;

#endif
