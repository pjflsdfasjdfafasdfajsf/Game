#include "Ent.h"
#include "SDL.h"

CompTypeResult AddComp(World *World, Uint32 Size)
{
    CompTypeResult Result = {0};

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

    Uint32 TypeID = World->CompTypeCount;
    World->CompSizes[TypeID] = Size;
    World->CompTypeCount++;

    Result.ID = TypeID;
    Result.IsValid = True;
    return Result;
}

EntResult AddEnt(World *World)
{
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
    if (EntID >= MaxEnts)
    {
        LogCritical("Invalid Entity ID %u.\n", EntID);
        return False;
    }

    if (TypeID >= World->CompTypeCount)
    {
        LogCritical("Invalid Comp Type ID %u.\n", TypeID);
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

    Usize Size = World->CompSizes[TypeID];
    SDL_memcpy(World->CompData[EntID][TypeID], Mem, Size);
    World->CompPresent[EntID][TypeID] = True;
    return True;
}

CompResult EntGetComp(World *World, Uint32 EntID, Uint32 TypeID)
{
    CompResult Result = {0};
    Result.IsValid = False;

    if (EntID >= MaxEnts || TypeID >= World->CompTypeCount)
    {
        return Result;
    }

    if (World->EntActive[EntID] && World->CompPresent[EntID][TypeID])
    {
        Usize Size = World->CompSizes[TypeID];
        SDL_memcpy(Result.Mem, World->CompData[EntID][TypeID], Size);
        Result.IsValid = True;
    }

    return Result;
}

Bool EntAddTransform(World *World, Uint32 EntID, const CompTransform *Transform)
{
    if (EntID >= MaxEnts)
    {
        LogCritical("Invalid Entity ID %u.\n", EntID);
        return False;
    }

    if (!World->EntActive[EntID])
    {
        LogCritical("Entity %u is inactive.\n", EntID);
        return False;
    }

    CompID TransformTypeID = MaxCompTypes;
    for (Uint32 I = 0; I < World->CompTypeCount; ++I)
    {
        // TODO: HACK
        if (World->CompSizes[I] == sizeof(CompTransform))
        {
            TransformTypeID = I;
            break;
        }
    }

    if (TransformTypeID >= MaxCompTypes)
    {
        LogCritical("CompTransform component type is not registered.\n");
        return False;
    }

    if (!Transform)
    {
        LogCritical("Invalid component memory source.\n");
        return False;
    }

    Usize Size = World->CompSizes[TransformTypeID];
    SDL_memcpy(World->CompData[EntID][TransformTypeID], Transform, Size);
    World->CompPresent[EntID][TransformTypeID] = True;
    return True;
}

Bool EntAddRenderable(World *World, Uint32 EntID, const CompRenderable *Renderable)
{
    if (EntID >= MaxEnts)
    {
        LogCritical("Invalid Entity ID %u.\n", EntID);
        return False;
    }

    if (!World->EntActive[EntID])
    {
        LogCritical("Entity %u is inactive.\n", EntID);
        return False;
    }

    CompID RenderableTypeID = CompIDInvalid;
    for (Uint32 I = 0; I < World->CompTypeCount; ++I)
    {
        // TODO: HACK
        if (World->CompSizes[I] > sizeof(CompTransform))
        {
            RenderableTypeID = I;
            break;
        }
    }

    if (RenderableTypeID >= MaxCompTypes)
    {
        LogCritical("CompRenderable component type is not registered.\n");
        return False;
    }

    if (!Renderable)
    {
        LogCritical("Invalid component memory source.\n");
        return False;
    }

    Usize Size = World->CompSizes[RenderableTypeID];
    SDL_memcpy(World->CompData[EntID][RenderableTypeID], Renderable, Size);
    World->CompPresent[EntID][RenderableTypeID] = True;
    return True;
}
