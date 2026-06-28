#if !defined(ENT_H)
#define ENT_H

#include "Types.h"
#include "Math.h"

#define MaxEnts 32
#define MaxCompTypes 8
#define MaxCompSize 32

typedef struct CompTypeResult
{
    Uint32 ID;

    Bool IsValid;
} CompTypeResult;

typedef struct EntResult
{
    Uint32 ID;

    Bool IsValid;
} EntResult;

typedef struct CompResult
{
    Uint8 Mem[MaxCompSize];

    Bool IsValid;
} CompResult;

//
// NOTE: Built-in components.
// 

typedef struct CompTransform {
    V2 Pos;
    V2 Size; 
} CompTransform;

typedef struct CompRenderable {} CompRenderable;

#endif
