#if !defined(MATH_H)
#define MATH_H

#include "Types.h"

typedef struct Color
{
    Uint8 R;
    Uint8 G;
    Uint8 B;
    Uint8 A;
} Color;

Color Color_MakeNoA(Uint8 R, Uint8 G, Uint8 B);

// NOTE: Color definitions.

#define White ColorMakeNoA(255, 255, 255)
#define Black ColorMakeNoA(0, 0, 0)
#define Red ColorMakeNoA(255, 0, 0)
#define Green ColorMakeNoA(0, 255, 0)
#define Blue ColorMakeNoA(0, 0, 255)

typedef struct
{
    union
    {
        struct
        {

            Float32 X;
            Float32 Y;
        };

        struct
        {
            Float32 W;
            Float32 H;
        };
    };
} V2;

V2 V2_Make(Float32 X, Float32 Y);
// NOTE: Initializes a V2 with X and Y set to X.
V2 V2_Splat(Float32 X);

#define V2_Zero (V2_Make(0.0f, 0.0f))

V2 V2_Div(V2 X, V2 Y);
V2 V2_Add(V2 X, V2 Y);
V2 V2_Sub(V2 X, V2 Y);
V2 V2_Mul(V2 X, V2 Y);
Float32 V2_Dot(V2 X, V2 Y);
Float32 V2_LenSquared(V2 X);
Float32 V2_Len(V2 X);
V2 V2_Norm(V2 X);

Float32 V2_DistSquared(V2 X, V2 Y);
Float32 V2_Dist(V2 X, V2 Y);

// NOTE: Scalar ops.

V2 V2_Scale(V2 X, Float32 Y);
// NOTE: Scalar division.
V2 V2_Unscale(V2 X, Float32 Y);

typedef struct
{
    union
    {
        struct
        {

            Int32 X;
            Int32 Y;
        };

        struct
        {
            Int32 W;
            Int32 H;
        };
    };
} V2I;

// NOTE: Imagine NAMING your things in a SELF-DOCUMENTING way??
V2I V2I_Make(Int32 X, Int32 Y);
V2I V2I_Make2(V2 X);

// NOTE: Scalar ops.

V2I V2I_Scale(V2I X, Int32 Y);
// NOTE: Scalar division.
V2I V2I_Unscale(V2I X, Int32 Y);

typedef struct
{
    V2 Pos;
    V2 Size;
} Rect;

Rect Rect_Make(Float32 X, Float32 Y, Float32 W, Float32 H);
// NOTE: Makes a Rect out of V2s.
Rect Rect_Make2(V2 Pos, V2 Size);
// NOTE: Points exactly on the right (Pos.X + Size.W) or bottom (Pos.Y + Size.H)
// borders are considered outside!
Bool Rect_ContainsV2(Rect Rect, V2 Point);
Bool Rect_ContainsRect(Rect X, Rect Y);

// NOTE: DO NOT MESS UP THESE TWO!!

// NOTE: Returns V2 indicating where the top-left corner of the inner object should
// be placed so that it is centered inside the container
V2 Rect_GetCenteredPos(Rect Rect, V2 Size);
// NOTE: Returns a Rect of that size, centered precisely on that coordinate
Rect Rect_GetCentered(V2 Pos, V2 Size);

#define Rect_Zero RectMake(0.0f, 0.0f, 0.0f, 0.0f)

//
// NOTE: Camera.
//

typedef struct Camera
{
    V2 Pos;
    // NOTE: Usually is InternalRes.
    V2I Viewport;
} Camera;

// NOTE: Keep everything in world space and use this only when you need to pass
// coordinates to the draw call.
V2 Camera_ToScreen(Camera Camera, V2 World);

//
// NOTE: Hashing.
//

Uint32 Hash_FNV1a_Str(const char *Str, Usize Len);
Uint32 Hash_FNV1a_CStr(const char *CStr);

Uint32 Hash_FNV1a_Combine(Uint32 Parent, Uint32 Child);

// NOTE: Generic Math
Float32 Math_Approach(Float32 Current, Float32 Target, Float32 MaximumDelta);

#endif
