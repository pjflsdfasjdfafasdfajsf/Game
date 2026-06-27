
#include "World.h"
#include "State.h"

//
// NOTE: Main game file.
//
#include <SDK.h>

#define MapTileSize 40

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

static inline Void MapDraw(RenderBuf *RenderBuf, State *State)
{
    Assert(State);
    Assert(RenderBuf);

    World *World = &State->World;
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
                RenderBufDrawCircle(RenderBuf, Center, GetParam(World->Player.Params.HookRadius), Green, Hollow);
            }
            break;
            }

            RenderBufDrawRect(RenderBuf, TexHandleInvalid, Tile, Tile, Color, Filled);
        }
    }
}

// Returns TRUE if the tile at a given coordinates X, Y is set to TileSolid.
// Tiles outside of the map are not considered solid.
static Bool TileIsSolid(State *State, Int32 X, Int32 Y)
{
    Assert(State);

    Bool IsOutsideTheMap = X < 0 || X >= MapWidth || Y < 0 || Y >= MapHeight;
    if (IsOutsideTheMap)
    {
        return False;
    }

    return State->World.Map.Grid[Y][X] == TileSolid;
}

// Returns TRUE if the rectangle overlaps any solid tile.
static Bool MapIsOverlappingSolidTile(State *State, Rect Rectangle)
{
    Assert(State);

    V2I FirstTouchedTile = V2IUnscale(V2IMake2(Rectangle.Pos), MapTileSize);
    V2I LastTouchedTile = V2IUnscale(V2IMake2(V2Add(Rectangle.Pos, V2Sub(Rectangle.Size, V2Splat(0.001f)))), MapTileSize);

    for (Int32 Y = FirstTouchedTile.Y; Y <= LastTouchedTile.Y; Y++)
    {
        for (Int32 X = FirstTouchedTile.X; X <= LastTouchedTile.X; X++)
        {
            if (!TileIsSolid(State, X, Y))
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

static Void MapMoveAndCollide(State *State, V2 *Pos, V2 Size, V2 Move)
{
    Assert(State);
    Assert(Pos);

    Float32 PosBeforeMovingX = Pos->X;
    Pos->X += Move.X;
    if (MapIsOverlappingSolidTile(State, RectGetCentered(*Pos, Size)))
    {
        Pos->X = PosBeforeMovingX;
        Float32 Dir = CopySign(1.0f, Move.X);
        Float32 Vel = State->World.Player.Vel.X;
        State->World.Player.Vel.X = 0;

        while (!MapIsOverlappingSolidTile(State, RectGetCentered(V2Make(Pos->X + Dir, Pos->Y), Size)))
        {
            Pos->X += Dir;
            State->World.Player.Vel.X = Vel;
        }
    }

    Float32 PosBeforeMovingY = Pos->Y;
    Pos->Y += Move.Y;
    if (MapIsOverlappingSolidTile(State, RectGetCentered(*Pos, Size)))
    {
        Pos->Y = PosBeforeMovingY;
        Float32 Dir = CopySign(1.0f, Move.Y);

        while (!MapIsOverlappingSolidTile(State, RectGetCentered(V2Make(Pos->X, Pos->Y + Dir), Size)))
        {
            Pos->Y += Dir;
        }
    }
}

static Bool MapFindNearestHookWithinRadius(State *State, V2 *OutNearestHook)
{
    Bool Result = False;

    Float32 Radius = GetParam(State->World.Player.Params.HookRadius);
    Float32 ClosestDistanceSquared = Radius * Radius;

    for (Int32 Y = 0; Y < MapHeight; Y++)
    {
        for (Int32 X = 0; X < MapWidth; X++)
        {
            if (State->World.Map.Grid[Y][X] != TileHook)
            {
                continue;
            }

            V2 Center = V2Make((Float32)(X * MapTileSize) + (MapTileSize * 0.5f),(Float32)(Y * MapTileSize) + (MapTileSize * 0.5f));
            Float32 DistanceSquared = V2DistSquared(State->World.Player.Pos, Center);

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

//
// NOTE: player
//

static inline Player PlayerInitialize(Void)
{
    Player Result = {0};

    Result.State = PlayerState_Normal;

    Result.Params.Size.Value = V2Splat(40.0f);
    Result.Params.HookRadius.Value = 267.0f;
    Result.Params.HookForce.Value = 1000.0f;
    Result.Params.MaxSpeed.Value = 600.0f;
    Result.Params.DashSpeed.Value = 1500.0f;
    Result.Params.JumpSpeed.Value = 1000.0f;
    Result.Params.SlamSpeed.Value = 2500.0f;
    Result.Params.Acceleration.Value = 3000.0f;
    Result.Params.Friction.Value = 2500.0f;
    Result.Params.Gravity.Value = 2000.0f;
    Result.Params.ApexThreshold.Value = 150.0f;
    Result.Params.ApexGravityMultiplier.Value = 0.5f;
    Result.Params.ApexControlMultiplier.Value = 2.0f;
    Result.Params.DashDuration.Value = 0.15f;
    Result.Params.DashCooldown.Value = 1.0f;
    Result.Params.JumpBufferDuration.Value = 0.15f;
    Result.Params.HookCooldown.Value = 0.25f;

    // NOTE: Multipliers
    Result.Params.Size.Multiplier = V2Splat(1.0f);
    Result.Params.HookRadius.Multiplier = 1.0f;
    Result.Params.HookForce.Multiplier = 1.0f;
    Result.Params.MaxSpeed.Multiplier = 1.0f;
    Result.Params.DashSpeed.Multiplier = 1.0f;
    Result.Params.JumpSpeed.Multiplier = 1.0f;
    Result.Params.SlamSpeed.Multiplier = 1.0f;
    Result.Params.Acceleration.Multiplier = 1.0f;
    Result.Params.Friction.Multiplier = 1.0f;
    Result.Params.Gravity.Multiplier = 1.0f;
    Result.Params.ApexThreshold.Multiplier = 1.0f;
    Result.Params.ApexGravityMultiplier.Multiplier = 1.0f;
    Result.Params.ApexControlMultiplier.Multiplier = 1.0f;
    Result.Params.DashDuration.Multiplier = 1.0f;
    Result.Params.DashCooldown.Multiplier = 1.0f;
    Result.Params.JumpBufferDuration.Multiplier = 1.0f;
    Result.Params.HookCooldown.Multiplier = 1.0f;

    return Result;
}

static Bool PlayerOnGround(State *State)
{
    Rect Feet = RectGetCentered(State->World.Player.Pos, GetParamV2(State->World.Player.Params.Size));
    Feet.Pos.Y += 1.0f;

    return MapIsOverlappingSolidTile(State, Feet);
}

static inline Void PlayerUpdateSwing(State *State)
{
    Assert(State);

    Player *Player = &State->World.Player;

    Float32 Distance = V2Dist(Player->Pos, Player->Hook.Target);
    V2 Offset = V2Sub(Player->Pos, Player->Hook.Target);

    Bool CloserThanRope = Distance <= Player->Hook.Len || Distance == 0.0f;
    if (CloserThanRope)
    {
        return;
    }

    V2 Normal = V2Norm(Offset);
    V2 Pos = V2Add(Player->Hook.Target, V2Scale(Normal, Player->Hook.Len));

    V2 Correction = V2Sub(Pos, Player->Pos);
    MapMoveAndCollide(State, &Player->Pos, GetParamV2(State->World.Player.Params.Size), Correction);

    Float32 Vel = V2Dot(Player->Vel, Normal);
    if (Vel > 0.0f)
    {
        Player->Vel.X -= Normal.X * Vel;
        Player->Vel.Y -= Normal.Y * Vel;
    }
}

static inline Void PlayerUpdate(State *State)
{
    Player *Player = &State->World.Player;

    if (Player->JumpBufferTimer > 0.0f)
    {
        Player->JumpBufferTimer -= State->Time.Delta;
    }
    if (State->Input.Jump.Pressed)
    {
        Player->JumpBufferTimer = GetParam(State->World.Player.Params.JumpBufferDuration);
    }

    Player->Hook.Cooldown = State->Time.Delta;

    switch (Player->State)
    {
    case PlayerState_Normal:
    {
        if (Player->Dash.Timer > 0.0f)
        {
            Player->Dash.Timer -= State->Time.Delta;
        }

        Float32 HorizontalInput = (Float32)State->Input.Right.IsDown - (Float32)State->Input.Left.IsDown;
        Bool IsNearJumpApex = Abs(Player->Vel.Y) < GetParam(Player->Params.ApexThreshold) && !PlayerOnGround(State);

        Float32 TargetSpeed = HorizontalInput * GetParam(Player->Params.MaxSpeed);
        Float32 AccelarationRate = (HorizontalInput != 0.0f) ? GetParam(Player->Params.Acceleration) : GetParam(Player->Params.Friction);

        Bool MovingFast = Abs(Player->Vel.X) > GetParam(Player->Params.MaxSpeed);
        Bool HoldingSameDirection = CopySign(1.0f, Player->Vel.X) == HorizontalInput;

        if (!PlayerOnGround(State) && HorizontalInput == 0.0f)
        {
            AccelarationRate = 0.0f;
        }
        else if (!PlayerOnGround(State) && MovingFast && HoldingSameDirection)
        {
            TargetSpeed = Player->Vel.X;
        }

        if (IsNearJumpApex)
        {
            AccelarationRate *= GetParam(Player->Params.ApexControlMultiplier);
        }

        Player->Vel.X = Approach(Player->Vel.X, TargetSpeed, AccelarationRate * State->Time.Delta);

        // NOTE: Jumping
        if (Player->JumpBufferTimer > 0.0f && PlayerOnGround(State))
        {
            Player->Vel.Y = -GetParam(Player->Params.JumpSpeed);
            Player->JumpBufferTimer = 0.0f;
        }

        if (!State->Input.Jump.IsDown)
        {
            Float32 ShortJumpThreshold = -GetParam(Player->Params.JumpSpeed) * 0.5f;
            Player->Vel.Y = Max(Player->Vel.Y, ShortJumpThreshold);
        }

        // NOTE: Dashing
        if (State->Input.Dash.IsDown && Player->Dash.Timer <= 0.0f)
        {
            Player->State = PlayerState_Dash;
            Player->Dash.Timer = GetParam(Player->Params.DashDuration);

            if (HorizontalInput != 0.0f)
            {
                Player->Dash.Direction = HorizontalInput;
            }
            else
            {
                Player->Dash.Direction = CopySign(1.0f, Player->Vel.X);
            }
        }

        // NOTE: Slamming
        if (State->Input.Slam.IsDown && !PlayerOnGround(State))
        {
            Player->State = PlayerState_Slam;
            Player->Vel.X = 0.0f;
            Player->Vel.Y = GetParam(Player->Params.SlamSpeed);
        }

        // NOTE: Hook
        V2 NearestHook = V2Zero;
        if (State->Input.Hook.IsDown && Player->Hook.Cooldown <= 0.0f && MapFindNearestHookWithinRadius(State, &NearestHook))
        {
            Player->State = PlayerState_Hook;
            Player->Hook.Target = NearestHook;

            Player->Hook.Len = V2Dist(Player->Pos, NearestHook);
        }

        // NOTE: Gravity
        Player->Vel.Y += GetParam(Player->Params.Gravity) * State->Time.Delta;
    }
    break;
    case PlayerState_Dash:
    {
        Player->Vel.X = Player->Dash.Direction * GetParam(Player->Params.DashSpeed);
        Player->Vel.Y = 0.0f;

        Player->Dash.Timer -= State->Time.Delta;

        Bool DashHasEnded = Player->Dash.Timer <= 0.0f;
        if (DashHasEnded)
        {
            Player->State = PlayerState_Normal;
            Player->Vel.X = Player->Dash.Direction * GetParam(Player->Params.MaxSpeed);

            Player->Dash.Timer = GetParam(Player->Params.DashCooldown);
        }
    }
    break;
    case PlayerState_Slam:
    {
        Player->Vel.Y = GetParam(Player->Params.SlamSpeed);

        if (PlayerOnGround(State))
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
            Player->Hook.Cooldown = Player->Hook.Cooldown;
            break;
        }

        if (State->Input.Jump.Pressed)
        {
            Player->State = PlayerState_Normal;
            Player->Hook.Cooldown = Player->Hook.Cooldown;
            Player->Vel.Y = Min(Player->Vel.Y, -GetParam(Player->Params.JumpSpeed));
            break;
        }

        Float32 HorizontalInput = (Float32)State->Input.Right.IsDown - (Float32)State->Input.Left.IsDown;

        Float32 Distance = V2Dist(Player->Pos, Player->Hook.Target);
        V2 Offset = V2Sub(Player->Pos, Player->Hook.Target);

        if (Distance > 0.0f)
        {
            V2 Normal = V2Norm(Offset);
            V2 Tangent = V2Make(Normal.Y, -Normal.X);

            V2 ForceFromWorld = V2Make(HorizontalInput * GetParam(Player->Params.HookForce), 0.0f);
            Float32 ForceAlongTangent = V2Dot(ForceFromWorld, Tangent);

            Player->Vel = V2Add(Player->Vel, V2Scale(Tangent, ForceAlongTangent * State->Time.Delta));
        }

        Player->Vel.Y += GetParam(Player->Params.Gravity) * State->Time.Delta;
    }
    break;
    }

    // NOTE: Physics and Collisions
    V2 Move = V2Scale(Player->Vel, State->Time.Delta);
    Float32 PositionBeforeMovingY = Player->Pos.Y;

    MapMoveAndCollide(State, &Player->Pos, GetParamV2(Player->Params.Size), Move);

    if (Player->Pos.Y == PositionBeforeMovingY && Move.Y != 0.0f)
    {
        Player->Vel.Y = 0.0f;
    }

    if (Player->State == PlayerState_Hook)
    {
        PlayerUpdateSwing(State);
    }
}

static void PlayerDraw(RenderBuf *RenderBuf, State *State)
{
    Assert(State);
    Assert(RenderBuf);

    Player *Player = &State->World.Player;

    Rect Centered = RectGetCentered(CameraWorldToScreen(State->World.Camera, Player->Pos), GetParamV2(Player->Params.Size));
    RenderBufDrawRect(RenderBuf, TexHandleInvalid, Centered, RectZero, Red, Filled);

    if (Player->State == PlayerState_Hook)
    {
        RenderBufDrawLine(RenderBuf, CameraWorldToScreen(State->World.Camera, Player->Pos), CameraWorldToScreen(State->World.Camera, Player->Hook.Target), White);
    }
}

World WorldInit(State *State)
{
    World Result = {0};

    Result.Camera.Viewport = V2IMake2(InternalRes);
    Result.Map = MapInitialize();
    Result.Player = PlayerInitialize();

    return Result;
}

Void WorldUpdateAndRender(RenderBuf *RenderBuf, State *State)
{
    Assert(RenderBuf);
    Assert(State);

    World *World = &State->World;
    
    PlayerUpdate(State);

    RenderBufClear(RenderBuf, Black);
    MapDraw(RenderBuf, State);
    PlayerDraw(RenderBuf, State);
    RenderBufDrawCStr(RenderBuf, White, V2Make(10.0f, 10.0f), V2Make(2.0f, 2.0f), "Hello, World!\n");

    if (State->Input.RMB.Pressed)
    {
        State->GameState = GameState_Menu;
    }

    V2 WorldPos = V2Make((Float32)(MapWidth * MapTileSize), (Float32)(MapHeight * MapTileSize));
    CameraUpdate(&World->Camera, World->Player.Pos, World->Camera.Viewport, WorldPos, State->Time.Delta); 
}

