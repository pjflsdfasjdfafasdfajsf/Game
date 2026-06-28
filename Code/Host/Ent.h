//
// NOTE: Host-side ECS implementation.
// 
#if !defined(HOST_ENT_H)
#define HOST_ENT_H

#include "Public/Ent.h"
#include "Public/Types.h"

typedef struct World
{
    Usize CompSizes[MaxCompTypes];
    Uint32 CompTypeCount;

    Uint8 CompData[MaxEnts][MaxCompTypes][MaxCompSize];
    Bool CompPresent[MaxEnts][MaxCompTypes];

    Bool EntActive[MaxEnts];
} World;

CompTypeResult AddComp(World *World, Uint32 Size);
EntResult AddEnt(World *World);

Bool EntAddComp(World *World, Uint32 EntID, Uint32 TypeID, const Void *Mem);
Bool EntAddTransform(World *World, Uint32 EntID, const CompTransform *Transform);
Bool EntAddRenderable(World *World, Uint32 EntID, const CompRenderable *Renderable);

CompResult EntGetComp(World *World, Uint32 EntID, Uint32 TypeID);

#endif
