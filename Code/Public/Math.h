#include "Types.h"

typedef struct
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

typedef struct
{
    V2 Pos;
    V2 Size;
} Rect;

Rect RectMake(Float32 X, Float32 Y, Float32 W, Float32 H);
