#include "Math.h"
#include "Types.h"
#include "Mem.h"

#define Epsilon 1.19209289e-7

//
// NOTE: Color
//

Color Color_MakeNoA(Uint8 R, Uint8 G, Uint8 B)
{
    return (Color){R, G, B, 255};
}

//
// NOTE: V2
//

V2 V2_Make(Float32 X, Float32 Y)
{
    return (V2){{{X, Y}}};
}

V2 V2S_plat(Float32 X)
{
    return V2_Make(X, X);
}

V2 V2_Div(V2 X, V2 Y)
{
    return V2_Make(X.X / Y.X, X.Y / Y.Y);
}

V2 V2_Add(V2 X, V2 Y)
{
    return V2_Make(X.X + Y.X, X.Y + Y.Y);
}

V2 V2_Sub(V2 X, V2 Y)
{
    return V2_Make(X.X - Y.X, X.Y - Y.Y);
}

V2 V2_Mul(V2 X, V2 Y)
{
    return V2_Make(X.X * Y.X, X.Y * Y.Y);
}

Float32 V2_Dot(V2 X, V2 Y)
{
    return X.X * Y.X + X.Y * Y.Y;
}

Float32 V2_LenSquared(V2 X)
{
    return X.X * X.X + X.Y * X.Y;
}

Float32 V2_Len(V2 X)
{
    return Sqrt(X.X * X.X + X.Y * X.Y);
}

V2 V2_Norm(V2 X)
{
    Float32 Len = V2_Len(X);
    if (Len < Epsilon)
    {
        return V2_Zero;
    }
    return V2_Unscale(X, Len);
}

Float32 V2_DistSquared(V2 X, V2 Y)
{
    return V2_LenSquared(V2_Sub(X, Y));
}

Float32 V2_Dist(V2 X, V2 Y)
{
    return Sqrt(V2_DistSquared(X, Y));
}

// NOTE: Scalar ops.

V2 V2_Scale(V2 X, Float32 Y)
{
    return V2_Make(X.X * Y, X.Y * Y);
}

V2 V2_Unscale(V2 X, Float32 Y)
{
    return V2_Make(X.X / Y, X.Y / Y);
}

// NOTE: V2I

V2I V2I_Make(Int32 X, Int32 Y)
{
    return (V2I){{{X, Y}}};
}

V2I V2I_Make2(V2 X)
{
    return V2I_Make((Int32)X.X, (Int32)X.Y);
}

// NOTE: Scalar ops.

V2I V2I_Scale(V2I X, Int32 Y)
{
    return V2I_Make(X.X * Y, X.Y * Y);
}

V2I V2I_Unscale(V2I X, Int32 Y)
{
    return V2I_Make(X.X / Y, X.Y / Y);
}

//
// NOTE: Rect
//

Rect Rect_Make(Float32 X, Float32 Y, Float32 W, Float32 H)
{
    return (Rect){V2_Make(X, Y), V2_Make(W, H)};
}

Rect Rect_Make2(V2 Pos, V2 Size)
{
    return Rect_Make(Pos.X, Pos.Y, Size.W, Size.H);
}

Bool Rect_ContainsV2(Rect Rect, V2 Point)
{
    Bool Result = (Point.X >= Rect.Pos.X && Point.X < Rect.Pos.X + Rect.Size.W &&
                   Point.Y >= Rect.Pos.Y && Point.Y < Rect.Pos.Y + Rect.Size.H);
    return Result;
}

Bool Rect_ContainsRect(Rect X, Rect Y)
{
    V2 TopLeft = Y.Pos;
    V2 TopRight = V2_Make(Y.Pos.X + Y.Size.W, Y.Pos.Y);
    V2 BottomLeft = V2_Make(Y.Pos.X, Y.Pos.Y + Y.Size.H);
    V2 BottomRight = V2_Add(Y.Pos, Y.Size);

    return Rect_ContainsV2(X, TopLeft) || Rect_ContainsV2(X, TopRight) || Rect_ContainsV2(X, BottomLeft) || Rect_ContainsV2(X, BottomRight);
}

V2 Rect_GetCenteredPos(Rect R, V2 Size)
{
    V2 Rect = V2_Make(R.Size.W, R.Size.H);
    return V2_Add(R.Pos, V2_Scale(V2_Sub(Rect, Size), 0.5f));
}

Rect Rect_GetCentered(V2 Pos, V2 Size)
{
    return Rect_Make2(V2_Make(Pos.X - (Size.W * 0.5f), Pos.Y - (Size.H * 0.5f)), Size);
}

//
// NOTE: Camera
//

V2 Camera_ToScreen(Camera Camera, V2 World)
{
    return V2_Sub(World, Camera.Pos);
}

//
// NOTE: Hashing
//

#define HashInitial 2166136261u
#define HashPrime 16777619u

Uint32 Hash_FNV1a_Str(const char *Str, Usize Len)
{
    if (Len > 0)
    {
        Assert(Str);
    }

    Uint32 Result = HashInitial;

    for (Usize I = 0; I < Len; ++I)
    {
        Result ^= (Uint32)Str[I];
        Result *= HashPrime;
    }

    return Result;
}

Uint32 Hash_FNV1a_CStr(const char *CStr)
{
    return Hash_FNV1a_Str(CStr, Mem_CStrLen(CStr));
}

Uint32 Hash_FNV1a_Combine(Uint32 Parent, Uint32 Child)
{
    Uint32 Result = Parent;

    Result ^= Child;
    Result *= HashPrime;

    return Result;
}

// NOTE: Generic Math
Float32 Math_Approach(Float32 Current, Float32 Target, Float32 MaximumDelta)
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
