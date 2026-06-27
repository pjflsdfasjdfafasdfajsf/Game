//
// NOTE: Game state.
//
#if !defined(STATE_H)
#define STATE_H

#include "Math.h"
#include "Mem.h"
#include "Render.h"
#include "Types.h"

typedef struct Action
{
    Bool IsDown;
    Bool Pressed;
    Bool Released;
} Action;

typedef struct Input
{
    V2 MousePos;
    Action LMB;
    Action RMB;
    Action MMB;

    Action Jump;
    Action Dash;
    Action Slam;
    Action Hook;
    Action Left;
    Action Right;
} Input;

typedef struct Camera
{
    V2 Pos;
    V2I Viewport;
} Camera;

static inline V2 CameraWorldToScreen(Camera Camera, V2 World)
{
    return V2Sub(World, Camera.Pos);
}

// TODO: temporary
#define MapHeight 9
#define MapWidth 30

typedef struct Map
{
    Int32 Grid[MapHeight][MapWidth];
} Map;

typedef enum PlayerState
{
    PlayerState_Normal,
    PlayerState_Dash,
    PlayerState_Slam,
    PlayerState_Hook,    
} PlayerState;

typedef struct Player
{
    V2 Pos;
    V2 Vel;
    PlayerState State;

    Float32 DashTimer;
    Float32 DashDirection;

    V2 HookTarget;
    Float32 HookRopeLength;
    Float32 HookCooldown; // we are cool

    Float32 JumpBufferTimer;
} Player;

typedef struct Time
{
    Float32 Delta;
} Time;

// NOTE: ANYTHING THAT IS MODIFIED BY HOST MUST BE THE AT THE VERY TOP OF THIS
// STRUCT!!!

typedef struct State
{
    Input Input;
    Time Time;

    MemAlloc PermanentAlloc;

    TexHandle SpriteAtlasTex;
    Atlas SpriteAtlas;

    Camera Camera;
    Map Map;
    Player Player;

    Bool IsInitialized;
} State;

#endif
