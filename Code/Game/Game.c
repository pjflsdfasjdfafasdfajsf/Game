//
// NOTE: Main game file.
//
#include <SDK.h>

#include "GameAtlas.h"
#include "Render.h"

#define MapTileSize 28

enum
{
    TileEmpty,  
    TileSolid,
    TileHook,  
};

static inline Map MapInitialize(Void)
{
    Map Result = {
        {
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1},
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        },
    };
    
    return Result;
}

static inline Void MapDraw(Map *Map, RenderBuf *RenderBuf)
{
    Assert(Map); Assert(RenderBuf);
    
    for (Int32 Y = 0; Y < MapHeight; Y++)
    {
        for (Int32 X = 0; X < MapWidth; X++)
        {
            Int32 Tile = Map->Grid[Y][X];

            if (Tile == TileEmpty)
            {
                /* NOTE: dont draw empty Tiles */
                continue;
            }

            Rect Rect = RectMake(X * MapTileSize, Y * MapTileSize, MapTileSize, MapTileSize);
            Color color = Black;

            switch (Tile)
            {
            case TileSolid:
            {
                color = White;   
            } break;
            }

            RenderBufDrawRect(RenderBuf, TexHandleInvalid, Rect, Rect, color);
        }
    }
}

//
// NOTE: player
//

#define PlayerSize 40.0f
#define PlayerHookRadius 600.0f
#define PlayerHookForce 1000.0f
// in pixels/sec
#define PlayerMaxSpeed 600.0f
#define PlayerDashSpeed 1500.0f
#define PlayerJumpSpeed 1000.0f
#define PlayerSlamSpeed 2500.0f
#define PlayerAcceleration 3000.0f
#define PlayerFriction 2500.0f
#define PlayerGravity 2000.0f
// how close to 0 velocity.y needs to be to trigger the apex
#define PlayerApexThreshold 150.0f
#define PlayerApexGravityMultiplier 0.5f
#define PlayerApexControlMultiplier 2.0f
// in seconds
#define PlayerDashDuration 0.15f
#define PlayerDashCooldown 1.0f
#define PlayerJumpBufferDuration 0.15f

static inline Player PlayerInitialize(Void)
{
    Player Result = {0};

    Result.State = PlayerState_Normal;

    return Result;
}

static inline Void PlayerUpdate(State *State)
{
    Player *Player = &State->Player;

    switch (Player->State)
    {
    case PlayerState_Normal:
    {
        
    } break;
    }
} 

UpdateAndRender(UpdateAndRender)
{
    if (!State->IsInitialized)
    {
        State->PermanentAlloc = MemAllocInit(ExtraMem, Kb(32));

        State->SpriteAtlasTex = AllocTexture(GenGameAtlas, GenGameAtlasLen);
        State->SpriteAtlas = AtlasInit(&State->PermanentAlloc, State->SpriteAtlasTex, GenGameAtlasMeta, GenGameAtlasMetaLen);

        State->Map = MapInitialize();

        PrintCStr("(Game): Initialized");
        State->IsInitialized = True;
    }

    RenderBufClear(RenderBuf, Black);
    MapDraw(&State->Map, RenderBuf);
    RenderBufDrawCStr(RenderBuf, White, V2Make(10.0f, 10.0f), V2Make(2.0f, 2.0f), "Hello, World!\n");

    RenderBufDrawRect(RenderBuf, State->SpriteAtlasTex, RectMake(100.0f, 100.0f, 100.0f, 100.0f), AtlasGetRect(&State->SpriteAtlas, "ExcitedMan.png"), White);
}
