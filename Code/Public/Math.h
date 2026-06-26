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

V2 V2Div(V2 X, V2 Y);
V2 V2Add(V2 X, V2 Y);
V2 V2Sub(V2 X, V2 Y);

// NOTE: Scalar ops.

V2 V2Scale(V2 X, Float32 Y);
// NOTE: Scalar division.
V2 V2Unscale(V2 X, Float32 Y);

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
V2 RectGetCenteredPos(Rect Rect, V2 Size);

#define RectZero RectMake(0.0f, 0.0f, 0.0f, 0.0f)

//
// NOTE: Hashing.
//

// NOTE: FNV-1a.
Uint32 HashCStr(const char *CStr);
Uint32 HashCombine(Uint32 Parent, Uint32 Child);

#endif
