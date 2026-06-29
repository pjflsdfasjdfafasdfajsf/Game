
#if !defined(INPUT_H)
#define INPUT_H

#include "Public/Types.h"

typedef struct
{
    Bool Down;
    Bool Pressed;
    Bool Released;
} InputState;

typedef enum Input
{
    Key_W,
    Key_A,
    Key_S,
    Key_D,

    Button_Left,
    Button_Right,

    Key_Count,
} Input;

#endif
