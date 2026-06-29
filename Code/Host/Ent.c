#include "Ent.h"
#include "Public/Ent.h"
#include "Public/Math.h"
#include "Public/Mem.h"
#include "SDL.h"

#include <stdarg.h>

//
// NOTE: Internal
//

Int32 GetOrCreateResID(World *World, Uint32 Hash)
{
    Int32 Existing = GetID(World, Lookup_Res, Hash);
    if (Existing >= 0)
    {
        return Existing;
    }

    if (World->ResCount >= MaxRes)
    {
        LogCritical("Maximum resource capacity (%u) reached.\n", MaxRes);
        return -1;
    }

    ResID Result = World->ResCount++;
    World->Res[Result] = (Res){0};
    World->Res[Result].Hash = Hash;

    SDL_snprintf(World->ResNames[Result], sizeof(World->ResNames[Result]), "Anon_0x%08X", Hash);

    return (Int32)Result;
}

//
// NOTE: Implementation
//

Int32 GetID(const World *World, LookupType Type, Uint32 Hash)
{
    Assert(World);

    const Uint8 *Base;
    Uint32 Count;
    Usize Stride;
    Usize Offset;

    if (Type == Lookup_Comp)
    {
        Base = (const Uint8 *)World->CompTypeHashes;
        Count = World->CompTypeCount;
        Stride = sizeof(Uint32);
        Offset = 0;
    }
    else // Lookup_Resource
    {
        Base = (const Uint8 *)World->Res;
        Count = World->ResCount;
        Stride = sizeof(Res);
        Offset = offsetof(Res, Hash);
    }

    for (Uint32 I = 0; I < Count; ++I)
    {
        const Uint32 *TargetHash = (const Uint32 *)(Base + (I * Stride) + Offset);
        if (*TargetHash == Hash)
        {
            return (Int32)I;
        }
    }

    return -1;
}

CompTypeResult CompInit(World *World, Uint32 Hash, Uint32 Size)
{
    Assert(World);

    CompTypeResult Result = {0};

    Int32 ExistingID = GetID(World, Lookup_Comp, Hash);
    if (ExistingID >= 0)
    {
        Result.ID = Hash;
        Result.IsValid = True;
        return Result;
    }

    if (World->CompTypeCount >= MaxCompTypes)
    {
        LogCritical("Maximum component types (%u) exceeded.\n", MaxCompTypes);
        Result.ID = 0;
        Result.IsValid = False;
        return Result;
    }

    if (Size > MaxCompSize)
    {
        LogCritical("Comp size %u exceeds MaxCompSize (%u).\n", Size, MaxCompSize);
        Result.ID = 0;
        Result.IsValid = False;
        return Result;
    }

    Uint32 Index = World->CompTypeCount;
    World->CompTypeHashes[Index] = Hash;
    World->CompSizes[Index] = Size;
    World->CompTypeCount++;

    Result.ID = Hash;
    Result.IsValid = True;
    return Result;
}

EntResult EntInit(World *World)
{
    Assert(World);

    EntResult Result = {0};

    for (Uint32 I = 0; I < MaxEnts; ++I)
    {
        if (!World->EntActive[I])
        {
            World->EntActive[I] = True;
            Result.ID = I;
            Result.IsValid = True;
            return Result;
        }
    }

    LogCritical("Maximum entity capacity (%u) reached.\n", MaxEnts);
    Result.ID = 0;
    Result.IsValid = False;
    return Result;
}

Bool EntAddComp(World *World, Uint32 EntID, Uint32 TypeID, const Void *Mem)
{
    Assert(World);
    Assert(Mem);

    if (EntID >= MaxEnts)
    {
        LogCritical("Invalid Entity ID %u.\n", EntID);
        return False;
    }

    Int32 InternalID = GetID(World, Lookup_Comp, TypeID);
    if (InternalID < 0)
    {
        LogCritical("Invalid Comp Type Hash %u.\n", TypeID);
        return False;
    }

    if (!World->EntActive[EntID])
    {
        LogCritical("Entity %u is inactive.\n", EntID);
        return False;
    }

    if (!Mem)
    {
        LogCritical("Invalid dynamic component memory source.\n");
        return False;
    }

    Usize Size = World->CompSizes[InternalID];
    SDL_memcpy(World->CompData[EntID][InternalID], Mem, Size);
    World->CompPresent[EntID][InternalID] = True;
    return True;
}

CompResult EntGetComp(World *World, Uint32 EntID, Uint32 TypeID)
{
    Assert(World);

    CompResult Result = {0};

    if (EntID >= MaxEnts)
    {
        return Result;
    }

    Int32 InternalID = GetID(World, Lookup_Comp, TypeID);
    if (InternalID < 0)
    {
        return Result;
    }

    if (World->EntActive[EntID] && World->CompPresent[EntID][InternalID])
    {
        Usize Size = World->CompSizes[InternalID];
        SDL_memcpy(Result.Mem, World->CompData[EntID][InternalID], Size);
        Result.IsValid = True;
    }

    return Result;
}

//
// NOTE: Res
//

ResID ResGetID(World *World, const char *NamePtr, Usize NameLen)
{
    Assert(World);

    if (!NamePtr || NameLen == 0 || NameLen >= MaxResNameLen)
    {
        LogCritical("Invalid resource registration attempt.\n");
        return ResID_Invalid;
    }

    Uint32 Hash = HashStr(NamePtr, NameLen);
    // TODO: Linear search bad :(
    for (Uint32 I = 0; I < World->ResCount; ++I)
    {
        if (World->Res[I].Hash == Hash)
        {
            return I;
        }
    }

    if (World->ResCount >= MaxRes)
    {
        LogCritical("Maximum res capacity (%u) reached", MaxRes);
        return ResID_Invalid;
    }

    ResID Result = World->ResCount++;
    World->Res[Result] = (Res){0};

    World->Res[Result].Hash = Hash;
    MemCopy(World->ResNames[Result], NamePtr, NameLen);
    MemNullTerminate(World->ResNames[Result], sizeof(World->ResNames[Result]), NameLen);

    return Result;
}

Uint32 ResGetUint(const World *World, Uint32 Hash)
{
    Assert(World);
    Int32 ID = GetID(World, Lookup_Res, Hash);

    if (ID < 0)
    {
        return 0;
    }

    return World->Res[ID].Value.Uint;
}

Bool ResSetUint(World *World, Uint32 Hash, Uint32 Value)
{
    Assert(World);
    Int32 ID = GetOrCreateResID(World, Hash);

    if (ID < 0)
    {
        return False;
    }

    World->Res[ID].Value.Uint = Value;
    return True;
}

Float32 ResGetFloat(const World *World, Uint32 Hash)
{
    Assert(World);
    Int32 ID = GetID(World, Lookup_Res, Hash);

    if (ID < 0)
    {
        return 0.0f;
    }

    return World->Res[ID].Value.Float;
}

Bool ResSetFloat(World *World, Uint32 Hash, Float32 Value)
{
    Assert(World);
    Int32 ID = GetOrCreateResID(World, Hash);

    if (ID < 0)
    {
        return False;
    }

    World->Res[ID].Value.Float = Value;
    return True;
}

//
// NOTE: Iter
//

Iter IterInit(World *World, const CompID *CompIDs, Uint32 CompCount)
{
    Assert(World);
    Assert(CompCount <= MaxIterComps);

    Iter Result = {0};
    Result.CompCount = CompCount;
    Result.CurEntID = 0;
    Result.IsValid = True;

    for (Uint32 I = 0; I < CompCount; ++I)
    {
        Int32 InternalID = GetID(World, Lookup_Comp, CompIDs[I]);
        Result.InternalCompIDs[I] = InternalID;

        if (InternalID < 0)
        {
            Result.IsValid = False;
        }
    }

    return Result;
}

Bool IterNextArray(World *World, Iter *It, EntID *OutEntID, Void ***OutPtrs)
{
    Assert(World);
    Assert(It);

    if (!It->IsValid)
    {
        return False;
    }

    while (It->CurEntID < MaxEnts)
    {
        EntID CurID = It->CurEntID++;

        if (!World->EntActive[CurID])
        {
            continue;
        }

        Bool AllPresent = True;
        for (Uint32 I = 0; I < It->CompCount; ++I)
        {
            Int32 InternalID = It->InternalCompIDs[I];
            Assert(InternalID >= 0 && InternalID < MaxCompTypes);

            if (!World->CompPresent[CurID][InternalID])
            {
                AllPresent = False;
                break;
            }
        }

        if (!AllPresent)
        {
            continue;
        }

        if (OutEntID)
        {
            *OutEntID = CurID;
        }

        if (OutPtrs)
        {
            for (Uint32 I = 0; I < It->CompCount; ++I)
            {
                Int32 InternalID = It->InternalCompIDs[I];
                Void **OutPtr = OutPtrs[I];
                if (OutPtr)
                {
                    *OutPtr = (Void *)World->CompData[CurID][InternalID];
                }
            }
        }

        return True;
    }

    return False;
}

Bool IterNext(World *World, Iter *It, EntID *OutEntID, ...)
{
    Void **Ptrs[MaxIterComps] = {0};
    va_list Args;
    va_start(Args, OutEntID);
    for (Uint32 I = 0; I < It->CompCount; ++I)
    {
        Ptrs[I] = va_arg(Args, Void **);
    }
    va_end(Args);

    return IterNextArray(World, It, OutEntID, Ptrs);
}
