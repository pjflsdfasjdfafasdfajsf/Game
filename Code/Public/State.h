//
// NOTE: Game state.
//
#if !defined(STATE_H)
#define STATE_H

#include "Mem.h"
#include "Render.h"
#include "UI.h"

#define Param(Type, Name) \
    struct                \
    {                     \
        Type Value;       \
        Type Multiplier;  \
    } Name
#define GetParam(Param) ((Param).Value * (Param).Multiplier)
#define GetParamV2(Param) V2Mul((Param).Value, (Param).Multiplier)

typedef struct Time
{
    Float32 Delta;
} Time;

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

    struct
    {
        Float32 Timer;
        Float32 Direction;
    } Dash;

    struct
    {
        // NOTE: Position of the hook target players hooking on
        V2 Target;
        // NOTE: Length of the 'rope'
        Float32 Len;
        Float32 Cooldown;
    } Hook;

    Float32 JumpBufferTimer;
    // NOTE: This exists primarily for modders. It is very important that if
    // you want your mod to have basic compatability with other mods you set
    // MULTIPLIERS of parameters, not their VALUES.
    struct
    {
        Param(V2, Size);
        Param(Float32, HookRadius);
        Param(Float32, HookForce);
        Param(Float32, MaxSpeed);
        Param(Float32, DashSpeed);
        Param(Float32, JumpSpeed);
        Param(Float32, SlamSpeed);
        Param(Float32, Acceleration);
        Param(Float32, Friction);
        Param(Float32, Gravity);
        Param(Float32, ApexThreshold);
        Param(Float32, ApexGravityMultiplier);
        Param(Float32, ApexControlMultiplier);
        Param(Float32, DashDuration);
        Param(Float32, DashCooldown);
        Param(Float32, JumpBufferDuration);
        Param(Float32, HookCooldown);
    } Params;
} Player;

typedef struct World
{
    Camera Camera;
    Map Map;
    Player Player;
} World;

typedef struct Menu
{
    UIContext UI;
} Menu;

typedef enum
{
    GameState_Game,
    GameState_Menu,
} GameState;

// NOTE: ANYTHING THAT IS MODIFIED BY HOST MUST BE THE AT THE VERY TOP OF THIS
// STRUCT!!!

typedef struct State
{
    Input Input;
    Time Time;

    MemAlloc PermanentAlloc;

    TexHandle SpriteAtlasTex;
    Atlas SpriteAtlas;

    GameState GameState;
    World World;
    Menu Menu;

    Bool IsInitialized;
} State;

#endif
