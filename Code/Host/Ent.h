//
// NOTE: Host-side ECS implementation.
//
#if !defined(HOST_ENT_H)
#define HOST_ENT_H

#include "Public/Ent.h"
#include "Public/Types.h"

#define MaxRes 1024
#define MaxResNameLen 64

typedef struct
{
    Uint32 Hash;
    struct
    {
        Uint32 Uint;

        Int32 Int;

        Float32 Float;
    } Value;
    // TODO: Generic ptr.
} Res;

typedef struct
{
    // NOTE: Components
    Uint32 CompTypeHashes[MaxCompTypes];
    Usize CompSizes[MaxCompTypes];
    Uint32 CompTypeCount;

    Uint8 CompData[MaxEnts][MaxCompTypes][MaxCompSize];
    Bool CompPresent[MaxEnts][MaxCompTypes];

    // NOTE: Resources
    Res Res[MaxRes];
    char ResNames[MaxRes][MaxResNameLen];
    Uint32 ResCount;

    Bool EntActive[MaxEnts];
} World;

typedef enum
{
    Lookup_Comp,
    Lookup_Res
} LookupType;

Int32 GetID(const World *World, LookupType Type, Uint32 Hash);

CompTypeResult CompInit(World *World, Uint32 Hash, Uint32 Size);
EntResult EntInit(World *World);

Bool EntAddComp(World *World, Uint32 EntID, Uint32 TypeID, const Void *Mem);
CompResult EntGetComp(World *World, Uint32 EntID, Uint32 TypeID);

//
// NOTE: Res
//

Uint32 ResGetUint(const World *World, Uint32 Hash);
Bool ResSetUint(World *World, Uint32 Hash, Uint32 Value);

Float32 ResGetFloat(const World *World, Uint32 Hash);
Bool ResSetFloat(World *World, Uint32 Hash, Float32 Value);

//
// NOTE: Iter
//

Iter IterInit(World *World, const CompID *CompIDs, Uint32 CompCount);
Bool IterNext(World *World, Iter *It, EntID *OutEntID, ...);
Bool IterNextArray(World *World, Iter *It, EntID *OutEntID, Void ***OutPtrs);

#endif
