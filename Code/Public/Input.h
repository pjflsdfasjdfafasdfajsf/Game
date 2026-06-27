
#if !defined(INPUT_H)
#define INPUT_H

#include "Math.h"
#include "Types.h"

typedef struct Action
{
    Bool IsDown;
    Bool Pressed;
    Bool Released;
} Action;

typedef struct Input
{
    V2 MousePos;
    Action LMB;
    Action RMB;
    Action MMB;

    Action Jump;
    Action Dash;
    Action Slam;
    Action Hook;
    Action Left;
    Action Right;
} Input;

#endif
