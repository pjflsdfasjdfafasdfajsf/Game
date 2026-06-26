#include "Math.h"
#include "Types.h"

//
// NOTE: Color
//

Color ColorMakeNoA(Uint8 R, Uint8 G, Uint8 B)
{
    return (Color){R, G, B, 255};
}

//
// NOTE: V2
//

V2 V2Make(Float32 X, Float32 Y)
{
    return (V2){{{X, Y}}};
}

V2 V2Splat(Float32 X)
{
    return V2Make(X, X);
}

V2 V2Div(V2 X, V2 Y)
{
    return V2Make(X.X / Y.X, X.Y / Y.Y);
}

V2 V2Add(V2 X, V2 Y)
{
    return V2Make(X.X + Y.X, X.Y + Y.Y);
}

V2 V2Sub(V2 X, V2 Y)
{
    return V2Make(X.X - Y.X, X.Y - Y.Y);
}

// NOTE: Scalar ops.

V2 V2Scale(V2 X, Float32 Y)
{
    return V2Make(X.X * Y, X.Y * Y);
}

V2 V2Unscale(V2 X, Float32 Y)
{
    return V2Make(X.X / Y, X.Y / Y);
}

//
// NOTE: Rect
//

Rect RectMake(Float32 X, Float32 Y, Float32 W, Float32 H)
{
    return (Rect){V2Make(X, Y), V2Make(W, H)};
}

Rect RectMake2(V2 Pos, V2 Size)
{
    return RectMake(Pos.X, Pos.Y, Size.W, Size.H);
}

Bool RectContainsV2(Rect Rect, V2 Point)
{
    Bool Result = (Point.X >= Rect.Pos.X && Point.X < Rect.Pos.X + Rect.Size.W &&
                   Point.Y >= Rect.Pos.Y && Point.Y < Rect.Pos.Y + Rect.Size.H);
    return Result;
}

V2 RectGetCenteredPos(Rect R, V2 Size)
{
    V2 Rect = V2Make(R.Size.W, R.Size.H);
    return V2Add(R.Pos, V2Scale(V2Sub(Rect, Size), 0.5f));
}

//
// NOTE: Hashing
//

#define HashInitial 2166136261u
#define HashPrime 16777619u

Uint32 HashCStr(const char *CStr)
{
    Uint32 Result = HashInitial;

    while (*CStr)
    {
        Result ^= (Uint32)(*CStr++);
        Result *= HashPrime;
    }

    return Result;
}

Uint32 HashCombine(Uint32 Parent, Uint32 Child)
{
    Uint32 Result = Parent;

    Result ^= Child;
    Result *= HashPrime;

    return Result;
}
