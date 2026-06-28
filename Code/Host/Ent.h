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
    // NOTE: Integer representation.
    Uint32 Value;
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

Int32 GetInternalTypeID(const World *World, Uint32 Hash);

CompTypeResult CompInit(World *World, Uint32 Hash, Uint32 Size);
EntResult EntInit(World *World);

Bool EntAddComp(World *World, Uint32 EntID, Uint32 TypeID, const Void *Mem);
CompResult EntGetComp(World *World, Uint32 EntID, Uint32 TypeID);

//
// NOTE: Res
//

// NOTE: If the requested resource does not exist it is registered.
ResID ResGetID(World *World, const char *NamePtr, Usize NameLen);

Uint32 ResGetVal(const World *World, ResID ResID);
Bool ResSetVal(World *World, Uint32 ResID, Uint32 Value);

#endif
