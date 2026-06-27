#include "Math.h"
#include "Types.h"

#define EPSILON 1.19209289e-7

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

Float32 V2Dot(V2 X, V2 Y)
{
    return X.X * Y.X + X.Y * Y.Y;
}

Float32 V2LenSquared(V2 X)
{
    return X.X * X.X + X.Y * X.Y;
}

Float32 V2Len(V2 X)
{
    return Sqrt(X.X * X.X + X.Y * X.Y);
}

V2 V2Norm(V2 X)
{
    Float32 Len = V2Len(X);
    if (Len < EPSILON)
    {
        return V2Zero;
    }
    return V2Unscale(X, Len);
}

Float32 V2DistSquared(V2 X, V2 Y)
{
    return V2LenSquared(V2Sub(X, Y));
}

Float32 V2Dist(V2 X, V2 Y)
{
    return Sqrt(V2DistSquared(X, Y));
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

// NOTE: V2I

V2I V2IMake(Int32 X, Int32 Y)
{
    return (V2I){{{X, Y}}};
}

V2I V2IMake2(V2 X)
{
    return V2IMake((Int32)X.X, (Int32)X.Y);
}

// NOTE: Scalar ops.

V2I V2IScale(V2I X, Int32 Y)
{
    return V2IMake(X.X * Y, X.Y * Y);
}

V2I V2IUnscale(V2I X, Int32 Y)
{
    return V2IMake(X.X / Y, X.Y / Y);
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

Bool RectContainsRect(Rect X, Rect Y)
{
    V2 TopLeft = Y.Pos;
    V2 TopRight = V2Make(Y.Pos.X + Y.Size.W, Y.Pos.Y);
    V2 BottomLeft = V2Make(Y.Pos.X, Y.Pos.Y + Y.Size.H);
    V2 BottomRight = V2Add(Y.Pos, Y.Size);

    return RectContainsV2(X, TopLeft) || RectContainsV2(X, TopRight) || RectContainsV2(X, BottomLeft) || RectContainsV2(X, BottomRight);
}

V2 RectGetCenteredPos(Rect R, V2 Size)
{
    V2 Rect = V2Make(R.Size.W, R.Size.H);
    return V2Add(R.Pos, V2Scale(V2Sub(Rect, Size), 0.5f));
}

Rect RectGetCentered(V2 Pos, V2 Size)
{
    return RectMake2(V2Make(Pos.X - (Size.W * 0.5f), Pos.Y - (Size.H * 0.5f)), Size);
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

// NOTE: Generic Math
Float32 Approach(Float32 Current, Float32 Target, Float32 MaximumDelta)
{
    if (Current < Target)
    {
        return Min(Current + MaximumDelta, Target);
    }
    else
    {
        return Max(Current - MaximumDelta, Target);
    }
}
