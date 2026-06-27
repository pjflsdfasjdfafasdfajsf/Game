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

Color ColorMakeNoA(Uint8 R, Uint8 G, Uint8 B);

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

V2 V2Make(Float32 X, Float32 Y);
// NOTE: Initializes a V2 with X and Y set to X.
V2 V2Splat(Float32 X);

#define V2Zero (V2Make(0.0f, 0.0f))

V2 V2Div(V2 X, V2 Y);
V2 V2Add(V2 X, V2 Y);
V2 V2Sub(V2 X, V2 Y);
Float32 V2Dot(V2 X, V2 Y);
Float32 V2LenSquared(V2 X);
Float32 V2Len(V2 X);
V2 V2Norm(V2 X);

Float32 V2DistSquared(V2 X, V2 Y);
Float32 V2Dist(V2 X, V2 Y);

// NOTE: Scalar ops.

V2 V2Scale(V2 X, Float32 Y);
// NOTE: Scalar division.
V2 V2Unscale(V2 X, Float32 Y);



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
V2I V2IMake(Int32 X, Int32 Y);
V2I V2IMake2(V2 X);

// NOTE: Scalar ops.

V2I V2IScale(V2I X, Int32 Y);
// NOTE: Scalar division.
V2I V2IUnscale(V2I X, Int32 Y);

typedef struct
{
    V2 Pos;
    V2 Size;
} Rect;

Rect RectMake(Float32 X, Float32 Y, Float32 W, Float32 H);
// NOTE: Makes a Rect out of V2s.
Rect RectMake2(V2 Pos, V2 Size);
// NOTE: Points exactly on the right (Pos.X + Size.W) or bottom (Pos.Y + Size.H)
// borders are considered outside!
Bool RectContainsV2(Rect Rect, V2 Point);
Bool RectContainsRect(Rect X, Rect Y);

// NOTE: DO NOT MESS UP THESE TWO!!
// NOTE: Returns V2 indicating where the top-left corner of the inner object should
//  be placed so that it is centered inside the container
V2 RectGetCenteredPos(Rect Rect, V2 Size);
// NOTE: Returns a Rect of that size, centered precisely on that coordinate
Rect RectGetCentered(V2 Pos, V2 Size);

#define RectZero RectMake(0.0f, 0.0f, 0.0f, 0.0f)

//
// NOTE: Hashing.
//

// NOTE: FNV-1a.
Uint32 HashCStr(const char *CStr);
Uint32 HashCombine(Uint32 Parent, Uint32 Child);

// NOTE: Generic Math
Float32 Approach(Float32 Current, Float32 Target, Float32 MaximumDelta);

#endif
