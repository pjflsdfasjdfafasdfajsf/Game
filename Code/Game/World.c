
#include "World.h"
#include "State.h"

//
// NOTE: Main game file.
//

#define MapTileSize 40

#define PlayerSize V2Splat(40.0f)
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

static Void CameraUpdate(Camera *Camera, V2 Target, V2I Viewport, V2 World, Float32 Delta)
{
    Assert(Camera);

    V2 Desired = V2Make(Clamp(Target.X - Viewport.X * 0.5f, 0, World.X - Viewport.X), Clamp(Target.Y - Viewport.Y * 0.5f, 0, World.Y - Viewport.Y));
    Float32 Speed = 10.0f;
    Camera->Pos = V2Add(Camera->Pos, V2Scale(V2Sub(Desired, Camera->Pos), Speed * Delta));
}

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
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0},
            {0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
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

static inline Void MapDraw(RenderBuf *RenderBuf, World *World)
{
    Assert(World);
    Assert(RenderBuf);

    Map *Map = &World->Map;

    for (Int32 Y = 0; Y < MapHeight; Y++)
    {
        for (Int32 X = 0; X < MapWidth; X++)
        {
            Int32 TileID = Map->Grid[Y][X];

            if (TileID == TileEmpty)
            {
                /* NOTE: dont draw empty Tiles */
                continue;
            }

            Rect Tile = RectMake(X * MapTileSize, Y * MapTileSize, MapTileSize, MapTileSize);
            Tile.Pos = CameraWorldToScreen(World->Camera, Tile.Pos);

            Color Color = Black;

            switch (TileID)
            {
            case TileSolid:
            {
                Color = White;
            }
            break;
            case TileHook:
            {
                Color = Blue;
                V2 Center = V2Add(Tile.Pos, V2Scale(Tile.Size, 0.5f));
                RenderBufDrawCircle(RenderBuf, Center, PlayerHookRadius, Green, Hollow);
            }
            break;
            }

            RenderBufDrawRect(RenderBuf, TexHandleInvalid, Tile, Tile, Color, Filled);
        }
    }
}

// Returns TRUE if the tile at a given coordinates X, Y is set to TileSolid.
// Tiles outside of the map are not considered solid.
static Bool TileIsSolid(World *World, Int32 X, Int32 Y)
{
    Assert(World);

    Bool IsOutsideTheMap = X < 0 || X >= MapWidth || Y < 0 || Y >= MapHeight;
    if (IsOutsideTheMap)
    {
        return False;
    }

    return World->Map.Grid[Y][X] == TileSolid;
}

// Returns TRUE if the rectangle overlaps any solid tile.
static Bool MapIsOverlappingSolidTile(World *World, Rect Rectangle)
{
    Assert(World);

    V2I FirstTouchedTile = V2IUnscale(V2IMake2(Rectangle.Pos), MapTileSize);
    V2I LastTouchedTile = V2IUnscale(V2IMake2(V2Add(Rectangle.Pos, V2Sub(Rectangle.Size, V2Splat(0.001f)))), MapTileSize);

    for (Int32 Y = FirstTouchedTile.Y; Y <= LastTouchedTile.Y; Y++)
    {
        for (Int32 X = FirstTouchedTile.X; X <= LastTouchedTile.X; X++)
        {
            if (!TileIsSolid(World, X, Y))
            {
                continue;
            }

            Rect Tile = RectMake(X * MapTileSize, Y * MapTileSize, MapTileSize, MapTileSize);
            if (RectContainsRect(Rectangle, Tile))
            {
                return True;
            }
        }
    }

    return False;
}

static Void MapMoveAndCollide(World *World, V2 *Pos, V2 Size, V2 Move)
{
    Assert(World);
    Assert(Pos);

    Float32 PosBeforeMovingX = Pos->X;
    Pos->X += Move.X;
    if (MapIsOverlappingSolidTile(World, RectGetCentered(*Pos, Size)))
    {
        Pos->X = PosBeforeMovingX;
        Float32 Dir = CopySign(1.0f, Move.X);
        Float32 Vel = World->Player.Vel.X;
        World->Player.Vel.X = 0;

        while (!MapIsOverlappingSolidTile(World, RectGetCentered(V2Make(Pos->X + Dir, Pos->Y), Size)))
        {
            Pos->X += Dir;
            World->Player.Vel.X = Vel;
        }
    }

    Float32 PosBeforeMovingY = Pos->Y;
    Pos->Y += Move.Y;
    if (MapIsOverlappingSolidTile(World, RectGetCentered(*Pos, Size)))
    {
        Pos->Y = PosBeforeMovingY;
        Float32 Dir = CopySign(1.0f, Move.Y);

        while (!MapIsOverlappingSolidTile(World, RectGetCentered(V2Make(Pos->X, Pos->Y + Dir), Size)))
        {
            Pos->Y += Dir;
        }
    }
}

static Bool MapFindNearestHookWithinRadius(World *World, V2 Origin, Float32 Radius, V2 *OutNearestHook)
{
    Bool Result = False;
    Float32 ClosestDistanceSquared = Radius * Radius;

    for (Int32 Y = 0; Y < MapHeight; Y++)
    {
        for (Int32 X = 0; X < MapWidth; X++)
        {
            Bool IsHookTile = (World->Map.Grid[Y][X] == TileHook);

            if (IsHookTile)
            {
                V2 Center = V2Make((Float32)(X * MapTileSize) + (MapTileSize * 0.5f), (Float32)(Y * MapTileSize) + (MapTileSize * 0.5f));

                Float32 DistanceSquared = V2DistSquared(Origin, Center);
                Bool IsCloserThanPrevious = (DistanceSquared <= ClosestDistanceSquared);

                if (IsCloserThanPrevious)
                {
                    ClosestDistanceSquared = DistanceSquared;
                    *OutNearestHook = Center;
                    Result = True;
                }
            }
        }
    }

    return Result;
}

//
// NOTE: player
//

static inline Player PlayerInitialize(Void)
{
    Player Result = {0};

    Result.State = PlayerState_Normal;

    return Result;
}

static Bool PlayerOnGround(World *World)
{
    Rect Feet = RectGetCentered(World->Player.Pos, PlayerSize);
    Feet.Pos.Y += 1.0f;

    return MapIsOverlappingSolidTile(World, Feet);
}

static inline Void PlayerUpdateSwing(World *World)
{
    Assert(World);

    Player *Player = &World->Player;

    Float32 Distance = V2Dist(Player->Pos, Player->HookTarget);
    V2 Offset = V2Sub(Player->Pos, Player->HookTarget);

    Bool CloserThanRope = Distance <= Player->HookRopeLength || Distance == 0.0f;
    if (CloserThanRope)
    {
        return;
    }

    V2 Normal = V2Norm(Offset);
    V2 Pos = V2Add(Player->HookTarget, V2Scale(Normal, Player->HookRopeLength));

    V2 Correction = V2Sub(Pos, Player->Pos);
    MapMoveAndCollide(World, &Player->Pos, PlayerSize, Correction);

    Float32 Vel = V2Dot(Player->Vel, Normal);
    if (Vel > 0.0f)
    {
        Player->Vel.X -= Normal.X * Vel;
        Player->Vel.Y -= Normal.Y * Vel;
    }
}

static inline Void PlayerUpdate(State *State)
{
    World *World = &State->World;
    
    Player *Player = &World->Player;

    if (Player->JumpBufferTimer > 0.0f)
    {
        Player->JumpBufferTimer -= State->Time.Delta;
    }
    if (State->Input.Jump.Pressed)
    {
        Player->JumpBufferTimer = PlayerJumpBufferDuration;
    }

    Player->HookCooldown = State->Time.Delta;

    switch (Player->State)
    {
    case PlayerState_Normal:
    {
        if (Player->DashTimer > 0.0f)
        {
            Player->DashTimer -= State->Time.Delta;
        }

        Float32 HorizontalInput = (Float32)State->Input.Right.IsDown - (Float32)State->Input.Left.IsDown;
        Bool IsNearJumpApex = Abs(Player->Vel.Y) < PlayerApexThreshold && !PlayerOnGround(World);

        Float32 TargetSpeed = HorizontalInput * PlayerMaxSpeed;
        Float32 AccelarationRate = (HorizontalInput != 0.0f) ? PlayerAcceleration : PlayerFriction;

        Bool MovingFast = Abs(Player->Vel.X) > PlayerMaxSpeed;
        Bool HoldingSameDirection = CopySign(1.0f, Player->Vel.X) == HorizontalInput;

        if (!PlayerOnGround(World) && HorizontalInput == 0.0f)
        {
            AccelarationRate = 0.0f;
        }
        else if (!PlayerOnGround(World) && MovingFast && HoldingSameDirection)
        {
            TargetSpeed = Player->Vel.X;
        }

        if (IsNearJumpApex)
        {
            AccelarationRate *= PlayerApexControlMultiplier;
        }

            Player->Vel.X = Approach(Player->Vel.X, TargetSpeed, AccelarationRate * State->Time.Delta);

        // NOTE: Jumping
        if (Player->JumpBufferTimer > 0.0f && PlayerOnGround(World))
        {
            Player->Vel.Y = -PlayerJumpSpeed;
            Player->JumpBufferTimer = 0.0f;
        }

        if (!State->Input.Jump.IsDown)
        {
            Float32 ShortJumpThreshold = -PlayerJumpSpeed * 0.5f;
            Player->Vel.Y = Max(Player->Vel.Y, ShortJumpThreshold);
        }

        // NOTE: Dashing
        if (State->Input.Dash.IsDown && Player->DashTimer <= 0.0f)
        {
            Player->State = PlayerState_Dash;
            Player->DashTimer = PlayerDashDuration;

            if (HorizontalInput != 0.0f)
            {
                Player->DashDirection = HorizontalInput;
            }
            else
            {
                Player->DashDirection = CopySign(1.0f, Player->Vel.X);
            }
        }

        // NOTE: Slamming
        if (State->Input.Slam.IsDown && !PlayerOnGround(World))
        {
            Player->State = PlayerState_Slam;
            Player->Vel.X = 0.0f;
            Player->Vel.Y = PlayerSlamSpeed;
        }

        // NOTE: Hook
        V2 NearestHook = V2Zero;
        if (State->Input.Hook.IsDown && Player->HookCooldown <= 0.0f && MapFindNearestHookWithinRadius(World, Player->Pos, PlayerHookRadius, &NearestHook))
        {
            Player->State = PlayerState_Hook;
            Player->HookTarget = NearestHook;

            Player->HookRopeLength = V2Dist(Player->Pos, NearestHook);
        }

        // NOTE: Gravity
            Player->Vel.Y += PlayerGravity * State->Time.Delta;
    }
    break;
    case PlayerState_Dash:
    {
        Player->Vel.X = Player->DashDirection * PlayerDashSpeed;
        Player->Vel.Y = 0.0f;

            Player->DashTimer -= State->Time.Delta;

        Bool DashHasEnded = Player->DashTimer <= 0.0f;
        if (DashHasEnded)
        {
            Player->State = PlayerState_Normal;
            Player->Vel.X = Player->DashDirection * PlayerMaxSpeed;

            Player->DashTimer = PlayerDashCooldown;
        }
    }
    break;
    case PlayerState_Slam:
    {
        Player->Vel.Y = PlayerSlamSpeed;

        if (PlayerOnGround(World))
        {
            Player->State = PlayerState_Normal;
        }
    }
    break;
    case PlayerState_Hook:
    {
        if (!State->Input.Hook.IsDown)
        {
            Player->State = PlayerState_Normal;
            Player->HookCooldown = PlayerHookCooldown;
            break;
        }

        if (State->Input.Jump.Pressed)
        {
            Player->State = PlayerState_Normal;
            Player->HookCooldown = PlayerHookCooldown;
            Player->Vel.Y = Min(Player->Vel.Y, -PlayerJumpSpeed);
            break;
        }

        Float32 HorizontalInput = (Float32)State->Input.Right.IsDown - (Float32)State->Input.Left.IsDown;

        Float32 Distance = V2Dist(Player->Pos, Player->HookTarget);
        V2 Offset = V2Sub(Player->Pos, Player->HookTarget);

        if (Distance > 0.0f)
        {
            V2 Normal = V2Norm(Offset);
            V2 Tangent = V2Make(Normal.Y, -Normal.X);

            V2 ForceFromWorld = V2Make(HorizontalInput * PlayerHookForce, 0.0f);
            Float32 ForceAlongTangent = V2Dot(ForceFromWorld, Tangent);

                Player->Vel = V2Add(Player->Vel, V2Scale(Tangent, ForceAlongTangent * State->Time.Delta));
        }

            Player->Vel.Y += PlayerGravity * State->Time.Delta;
    }
    break;
    }

    // NOTE: Physics and Collisions
        V2 Move = V2Scale(Player->Vel, State->Time.Delta);
    Float32 PositionBeforeMovingY = Player->Pos.Y;

    MapMoveAndCollide(World, &Player->Pos, PlayerSize, Move);

    if (Player->Pos.Y == PositionBeforeMovingY && Move.Y != 0.0f)
    {
        Player->Vel.Y = 0.0f;
    }

    if (Player->State
     == PlayerState_Hook)
    {
        PlayerUpdateSwing(World);
    }
}

static void PlayerDraw(RenderBuf *RenderBuf, World *World)
{
    Assert(World);
    Assert(RenderBuf);

    Player *Player = &World->Player;

    Rect Centered = RectGetCentered(CameraWorldToScreen(World->Camera, Player->Pos), PlayerSize);
    RenderBufDrawRect(RenderBuf, TexHandleInvalid, Centered, RectZero, Red, Filled);

    if (Player->State
     == PlayerState_Hook)
    {
        RenderBufDrawLine(RenderBuf, CameraWorldToScreen(World->Camera, Player->Pos), CameraWorldToScreen(World->Camera, Player->HookTarget), White);
    }
}

World WorldInit(State *State)
{
    World Result = {0};

    Result.Camera.Viewport = V2IMake2(InternalRes);
    Result.Map = MapInitialize();

    return Result;
}

Void WorldUpdateAndRender(RenderBuf *RenderBuf, State *State)
{
    Assert(RenderBuf);
    Assert(State);

    World *World = &State->World;
    
    PlayerUpdate(State);

    RenderBufClear(RenderBuf, Black);
    MapDraw(RenderBuf, World);
    PlayerDraw(RenderBuf, World);
    RenderBufDrawCStr(RenderBuf, White, V2Make(10.0f, 10.0f), V2Make(2.0f, 2.0f), "Hello, World!\n");

    if (State->Input.RMB.Pressed)
    {
        State->GameState = GameState_Menu;
    }

    V2 WorldPos = V2Make((Float32)(MapWidth * MapTileSize), (Float32)(MapHeight * MapTileSize));
    CameraUpdate(&World->Camera, World->Player.Pos, World->Camera.Viewport, WorldPos, State->Time.Delta); 
}

