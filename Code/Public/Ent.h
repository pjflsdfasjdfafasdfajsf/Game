#if !defined(ENT_H)
#define ENT_H

#include "Math.h"
#include "Types.h"

#define MaxEnts 32
#define MaxCompTypes 8
#define MaxCompSize Kb(1)

typedef Uint32 CompID;
#define CompID_Invalid 0xFFFFFFFF

typedef Uint32 EntID;
#define EntID_Invalid 0xFFFFFFFF

typedef Uint32 ResID;
#define ResID_Invalid 0xFFFFFFFF

#define MaxIterComps 8

typedef struct CompTypeResult
{
    CompID ID;

    Bool IsValid;
} CompTypeResult;

typedef struct EntResult
{
    EntID ID;

    Bool IsValid;
} EntResult;

typedef struct CompResult
{
    Uint8 Mem[MaxCompSize];

    Bool IsValid;
} CompResult;

typedef struct IterResult
{
    Int32 InternalCompIDs[MaxIterComps];
    Uint32 CompCount;
    EntID CurEntID;

    Bool IsValid;
} Iter;

//
// NOTE: Built-in components.
//

typedef struct CompTransform
{
    V2 Pos;
    V2 Size;
} CompTransform;

#define CompTransformHash HashCStr("CompTransform")

typedef Uint32 TexHandle;
#define TexHandle_Invalid 0

typedef enum RenderableType
{
    RenderableType_None,
    RenderableType_Rect,
    RenderableType_Line,
    RenderableType_Circle,
    RenderableType_DebugText,
} RenderableType;

typedef struct CompRenderable
{
    RenderableType Type;

    union
    {
        struct
        {
            // NOTE: This can be TexHandleInvalid for a regular untextured rectangle.
            TexHandle Tex;

            // NOTE: Which part of the texture draw. If Src.Pos is 0,0, then the entire
            // image is drawn.
            // This is fully ignored if Tex is TexHandleInvalid.
            Rect Src;

            // NOTE: If Tex is TexHandleInvalid then it's the color of the rectangle.
            // Otherwise acts as a tint. For unmodified keep it as White.
            Color Color;

            // NOTE: This is fully ignored if Tex is NOT TexHandleInvalid and just renders a filled rectangle with a texture.
            Bool Filled;
        } Rect;

        // TODO: Texture?
        struct
        {
            Float32 Radius;
            Color Color;
            // TODO: Add filled circles
            Bool Filled;
        } Circle;

        struct
        {
            V2 End;
            Color Color;
        } Line;

        // NOTE: There's a reason why this command is called 'draw DEBUG' text and
        // that reason is that the implementation of it just uses SDL built-in
        // SDL_RenderDebugText function, which provides just simple bitmap
        // font rendering intended for, as you probably already guessed,
        // debugging.
#define DebugTextCharSize 8
        struct
        {
            Color Color;
            // NOTE: Scale multiplier.
            V2 Scale;

            // NOTE: The final newline is not appended!
            char Str[64];
        } DebugText;
    };
} CompRenderable;

#define CompRenderableHash HashCStr("CompRenderable")

#endif
