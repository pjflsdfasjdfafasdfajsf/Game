#include "Math.h"
#include "Types.h"

Color ColorMakeNoA(Uint8 R, Uint8 G, Uint8 B)
{
    return (Color){R, G, B, 255};
}

V2 V2Make(Float32 X, Float32 Y)
{
    return (V2){{{X, Y}}};
}

Rect RectMake(Float32 X, Float32 Y, Float32 W, Float32 H)
{
    return (Rect){V2Make(X, Y), V2Make(W, H)};
}
