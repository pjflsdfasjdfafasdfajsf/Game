#include <SDK.h>
#include "Game.h"
#include "Ent.h"
#include "Input.h"

#define PlayerHookRadius 267.0f
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
#define PlayerHookCooldown 0.25f

//
// NOTE: Map
// 

static Void MapInitialize(Void)
{
    CompInit(GameCompMapHash, sizeof(GameCompMap));
    EntResult Map = EntInit();

    GameCompMap MapComp = {
        .Grid = {
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2},
            {0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
            {0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
            {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        },  
    };
    EntAddComp(Map.ID, GameCompMapHash, &MapComp);

    for (Int32 Y = 0; Y < MapHeight; Y++)
    {
        for (Int32 X = 0; X < MapWidth; X++)
        {
            if (MapComp.Grid[Y][X] == TileEmpty)
            {
                continue;
            }

            EntResult Tile = EntInit();

            CompTransform Transform = {
                .Pos = V2Make(X * MapTileSize, Y * MapTileSize),
                .Size = V2Splat(MapTileSize),
            };
            EntAddComp(Tile.ID, CompTransformHash, &Transform);

            Color Color = White;

            if (MapComp.Grid[Y][X] == TileHook)
            {
                 Color = Blue;   
            }

            CompRenderable Renderable = {
                .Type = RenderableType_Rect,
                .Rect = {
                .Color = Color,
                .Filled = True,
            }};
            EntAddComp(Tile.ID, CompRenderableHash, &Renderable);
        }
    }
}

static Bool MapTileIsSolid(Int32 X, Int32 Y)
{
    Bool IsOutsideTheMap = X < 0 || X >= MapWidth || Y < 0 || Y >= MapHeight;
    if (IsOutsideTheMap)
    {
        return False;
    }

    Iter It = IterInit((CompID[]){GameCompMapHash}, 1);

    EntID EntID;
    GameCompMap GameMap;

    IterNext(&It, &EntID, &GameMap);
    return GameMap.Grid[Y][X] == TileSolid; 
}

static Bool MapIsOverlappingSolidTile(Rect Object)
{
    V2I FirstTouchedTile = V2IUnscale(V2IMake2(Object.Pos), MapTileSize);
    V2I LastTouchedTile = V2IUnscale(V2IMake2(V2Add(Object.Pos, V2Sub(Object.Size, V2Splat(0.001f)))), MapTileSize);

    for (Int32 Y = FirstTouchedTile.Y; Y <= LastTouchedTile.Y; Y++)
    {
        for (Int32 X = FirstTouchedTile.X; X <= LastTouchedTile.X; X++)
        {
            if (!MapTileIsSolid(X, Y))
            {
                continue;
            }

            Rect Tile = RectMake(X * MapTileSize, Y * MapTileSize, MapTileSize, MapTileSize);
            if (RectContainsRect(Object, Tile))
            {
                return True;
            }
        }
    }

    return False;
}

static Bool MapFindNearestHookWithinRadius(CompTransform *PlayerTransfrom, V2 *OutNearestHook)
{
    Assert(PlayerTransfrom);
    
    Bool Result = False;

    Float32 Radius = PlayerHookRadius;
    Float32 ClosestDistanceSquared = Radius * Radius;

     Iter It = IterInit((CompID[]){GameCompMapHash}, 1);

    EntID EntID;
    GameCompMap GameMap;

    IterNext(&It, &EntID, &GameMap);

    for (Int32 Y = 0; Y < MapHeight; Y++)
    {
        for (Int32 X = 0; X < MapWidth; X++)
        {
            if (GameMap.Grid[Y][X] != TileHook)
            {
                continue;
            }

            V2 Center = V2Make((Float32)(X * MapTileSize) + (MapTileSize * 0.5f), (Float32)(Y * MapTileSize) + (MapTileSize * 0.5f));
            Float32 DistanceSquared = V2DistSquared(PlayerTransfrom->Pos, Center);

            if (DistanceSquared > ClosestDistanceSquared)
            {
                continue;
            }

            ClosestDistanceSquared = DistanceSquared;
            *OutNearestHook = Center;
            Result = True;
        }
    }

    return Result;
}

static Void MapMoveAndCollide(GameCompPlayer *GamePlayer, V2 *Pos, V2 Size, V2 Move)
{
    Assert(GamePlayer);
    Assert(Pos);
    
    Float32 PosBeforeMovingX = Pos->X;
    Pos->X += Move.X;
    if (MapIsOverlappingSolidTile(RectGetCentered(*Pos, Size)))
    {
        Pos->X = PosBeforeMovingX;
        Float32 Dir = CopySign(1.0f, Move.X);
        Float32 Vel = GamePlayer->Vel.X;
        GamePlayer->Vel.X = 0;

        while (!MapIsOverlappingSolidTile(RectGetCentered(V2Make(Pos->X + Dir, Pos->Y), Size)))
        {
            Pos->X += Dir;
            GamePlayer->Vel.X = Vel;
        }
    }

    Float32 PosBeforeMovingY = Pos->Y;
    Pos->Y += Move.Y;
    if (MapIsOverlappingSolidTile(RectGetCentered(*Pos, Size)))
    {
        Pos->Y = PosBeforeMovingY;
        Float32 Dir = CopySign(1.0f, Move.Y);

        while (!MapIsOverlappingSolidTile(RectGetCentered(V2Make(Pos->X, Pos->Y + Dir), Size)))
        {
            Pos->Y += Dir;
        }
    }
}

//
// NOTE: Player
// 

static Void PlayerInitialize()
{
    CompInit(GameCompPlayerHash, sizeof(GameCompPlayer));
    
    EntResult Player = EntInit();
    GameCompPlayer GameCompPlayer = {
        .State = PlayerState_Normal,
    };
    EntAddComp(Player.ID, GameCompPlayerHash, &GameCompPlayer);

    CompTransform Transform = {
        .Pos = V2Make(100.0f, 100.0f),
        .Size = V2Splat(MapTileSize),
    };
    EntAddComp(Player.ID, CompTransformHash, &Transform);

    CompRenderable Renderable = {
        .Type = RenderableType_Rect,
        .Rect = {
            .Color = Red,
            .Filled = True,
        }};
    EntAddComp(Player.ID, CompRenderableHash, &Renderable);
}

static Bool PlayerOnGround(CompTransform *PlayerTransform, GameCompPlayer *GamePlayer)
{
    Assert(PlayerTransform);
    Assert(GamePlayer);

    Rect Feet = RectGetCentered(PlayerTransform->Pos, PlayerTransform->Size);
    Feet.Pos.Y += 1.0f;

    return MapIsOverlappingSolidTile(Feet);
}

static Void PlayerUpdateSwing(CompTransform *PlayerTransform, GameCompPlayer *GamePlayer)
{
    Assert(PlayerTransform);
    Assert(GamePlayer);

    Float32 Distance = V2Dist(PlayerTransform->Pos, GamePlayer->Hook.Target);
    V2 Offset = V2Sub(PlayerTransform->Pos, GamePlayer->Hook.Target);

    Bool CloserThanRope = Distance <= GamePlayer->Hook.Len || Distance == 0.0f;
    if (CloserThanRope)
    {
        return;
    }

    V2 Normal = V2Norm(Offset);
    V2 Pos = V2Add(GamePlayer->Hook.Target, V2Scale(Normal, GamePlayer->Hook.Len));

    V2 Correction = V2Sub(Pos, PlayerTransform->Pos);
    MapMoveAndCollide(GamePlayer, &PlayerTransform->Pos, PlayerTransform->Size, Correction);

    Float32 Vel = V2Dot(GamePlayer->Vel, Normal);
    if (Vel > 0.0f)
    {
        GamePlayer->Vel.X -= Normal.X * Vel;
        GamePlayer->Vel.Y -= Normal.Y * Vel;
    }    
}

static Void PlayerUpdate(Float32 DeltaTime)
{
    Iter It = IterInit((CompID[]){GameCompPlayerHash, CompTransformHash}, 2);

    EntID EntID;
    GameCompPlayer GamePlayer;
    CompTransform Transform;

    while (IterNext(&It, &EntID, &GamePlayer, &Transform))
    {
        if (GamePlayer.JumpBufferTimer > 0.0f)
        {
            GamePlayer.JumpBufferTimer -= DeltaTime;
        }
        if (GetInputState(Key_Space).Pressed)
        {
            GamePlayer.JumpBufferTimer = PlayerJumpBufferDuration;
        }

        GamePlayer.Hook.Cooldown -= DeltaTime;

        switch (GamePlayer.State)
        {
        case PlayerState_Normal:
        {
            if (GamePlayer.Dash.Timer > 0.0f)
            {
                GamePlayer.Dash.Timer -= DeltaTime;
            }

            Float32 HorizontalInput = (Float32)GetInputState(Key_D).Down - (Float32)GetInputState(Key_A).Down;
            Bool IsNearJumpApex = Abs(GamePlayer.Vel.Y) < PlayerApexThreshold && !PlayerOnGround(&Transform, &GamePlayer);

            Float32 TargetSpeed = HorizontalInput * PlayerMaxSpeed;
            Float32 AccelarationRate = (HorizontalInput != 0.0f) ? PlayerAcceleration : PlayerFriction;

            Bool MovingFast = Abs(GamePlayer.Vel.X) > PlayerMaxSpeed;
            Bool HoldingSameDirection = CopySign(1.0f, GamePlayer.Vel.X) == HorizontalInput;

            if (!PlayerOnGround(&Transform, &GamePlayer) && HorizontalInput == 0.0f)
            {
                AccelarationRate = 0.0f;
            }
            else if (!PlayerOnGround(&Transform, &GamePlayer) && MovingFast && HoldingSameDirection)
            {
                TargetSpeed = GamePlayer.Vel.X;
            }

            if (IsNearJumpApex)
            {
                AccelarationRate *= PlayerApexControlMultiplier;
            }

            GamePlayer.Vel.X = Approach(GamePlayer.Vel.X, TargetSpeed, AccelarationRate * DeltaTime);

            // NOTE: Jumping
            if (GamePlayer.JumpBufferTimer > 0.0f && PlayerOnGround(&Transform, &GamePlayer))
            {
                GamePlayer.Vel.Y = -PlayerJumpSpeed;
                GamePlayer.JumpBufferTimer = 0.0f;
            }

            if (!GetInputState(Key_Space).Down)
            {
                Float32 ShortJumpThreshold = -PlayerJumpSpeed * 0.5f;
                GamePlayer.Vel.Y = Max(GamePlayer.Vel.Y, ShortJumpThreshold);
            }

            // NOTE: Dashing
            if (GetInputState(Key_Shift).Down && GamePlayer.Dash.Timer <= 0.0f)
            {
                GamePlayer.State = PlayerState_Dash;
                GamePlayer.Dash.Timer = PlayerDashDuration;

                if (HorizontalInput != 0.0f)
                {
                    GamePlayer.Dash.Direction = HorizontalInput;
                }
                else
                {
                    GamePlayer.Dash.Direction = CopySign(1.0f, GamePlayer.Vel.X);
                }
            }

            // NOTE: Slamming
            if (GetInputState(Key_S).Down && !PlayerOnGround(&Transform, &GamePlayer))
            {
                GamePlayer.State = PlayerState_Slam;
                GamePlayer.Vel.X = 0.0f;
                GamePlayer.Vel.Y = PlayerSlamSpeed;
            }

            // NOTE: Hook
            V2 NearestHook = V2Zero;
            if (GetInputState(Key_E).Down && GamePlayer.Hook.Cooldown <= 0.0f && MapFindNearestHookWithinRadius(&Transform, &NearestHook))
            {
                GamePlayer.State = PlayerState_Hook;
                GamePlayer.Hook.Target = NearestHook;

                GamePlayer.Hook.Len = V2Dist(Transform.Pos, NearestHook);
            }

            // NOTE: Gravity
            GamePlayer.Vel.Y += PlayerGravity * DeltaTime;
        } break;
        case PlayerState_Dash:
        {
            GamePlayer.Vel.X = GamePlayer.Dash.Direction * PlayerDashSpeed;
            GamePlayer.Vel.Y = 0.0f;

            GamePlayer.Dash.Timer -= DeltaTime;

            Bool DashHasEnded = GamePlayer.Dash.Timer <= 0.0f;
            if (DashHasEnded)
            {
                GamePlayer.State = PlayerState_Normal;
                GamePlayer.Vel.X = GamePlayer.Dash.Direction * PlayerMaxSpeed;

                GamePlayer.Dash.Timer = PlayerDashCooldown;
            }
        }
        break;
        case PlayerState_Slam:
        {
            GamePlayer.Vel.Y = PlayerSlamSpeed;

            if (PlayerOnGround(&Transform, &GamePlayer))
            {
                GamePlayer.State = PlayerState_Normal;
            }
        }
        break;
        case PlayerState_Hook:
        {
            if (!GetInputState(Key_E).Down)
            {
                GamePlayer.State = PlayerState_Normal;
                GamePlayer.Hook.Cooldown = PlayerHookCooldown;
                break;
            }

            if (GetInputState(Key_E).Pressed)
            {
                GamePlayer.State = PlayerState_Normal;
                GamePlayer.Hook.Cooldown = PlayerHookCooldown;
                GamePlayer.Vel.Y = Min(GamePlayer.Vel.Y, -PlayerJumpSpeed);
                break;
            }

            Float32 HorizontalInput = (Float32)GetInputState(Key_A).Down - (Float32)GetInputState(Key_D).Down;

            Float32 Distance = V2Dist(Transform.Pos, GamePlayer.Hook.Target);
            V2 Offset = V2Sub(Transform.Pos, GamePlayer.Hook.Target);

            if (Distance > 0.0f)
            {
                V2 Normal = V2Norm(Offset);
                V2 Tangent = V2Make(Normal.Y, -Normal.X);

                V2 ForceFromWorld = V2Make(HorizontalInput * PlayerHookForce, 0.0f);
                Float32 ForceAlongTangent = V2Dot(ForceFromWorld, Tangent);

                GamePlayer.Vel = V2Add(GamePlayer.Vel, V2Scale(Tangent, ForceAlongTangent * DeltaTime));
            }

            GamePlayer.Vel.Y += PlayerGravity * DeltaTime;
        }
        break;
        }

        // NOTE: Physics and Collisions
        V2 Move = V2Scale(GamePlayer.Vel, DeltaTime);
        Float32 PositionBeforeMovingY = Transform.Pos.Y;

        MapMoveAndCollide(&GamePlayer, &Transform.Pos, Transform.Size, Move);

        if (Transform.Pos.Y == PositionBeforeMovingY && Move.Y != 0.0f)
        {
            GamePlayer.Vel.Y = 0.0f;
        }

        if (GamePlayer.State == PlayerState_Hook)
        {
            PlayerUpdateSwing(&Transform, &GamePlayer);
        }
                    
        // TODO: No copy
        EntAddComp(EntID, CompTransformHash, &Transform);
        EntAddComp(EntID, GameCompPlayerHash, &GamePlayer);
    }
}

Init(Init)
{
    MapInitialize();
    PlayerInitialize();
}

UpdateAndRender(UpdateAndRender)
{
    DeltaTime = 1 / 60.0f;
    PlayerUpdate(DeltaTime);
}
