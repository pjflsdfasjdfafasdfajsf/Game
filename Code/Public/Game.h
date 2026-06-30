
#if !defined(GAME_H)
#define GAME_H

#include "Types.h"
#include "Math.h"

typedef enum PlayerState
{
	PlayerState_Normal,
	PlayerState_Dash,
	PlayerState_Slam,
	PlayerState_Hook,
} PlayerState;

typedef struct GameCompPlayer
{
	PlayerState State;

	// TODO: Turn into a physics component.
	V2 Vel;
	
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
} GameCompPlayer;

#define GameCompPlayerHash HashCStr("GameCompPlayer")

#define MapWidth 30
#define MapHeight 7
#define MapTileSize 40.0f

enum
{
    TileEmpty,
    TileSolid,
    TileHook,  
};	

typedef struct GameCompMap
{
	Int32 Grid[MapHeight][MapWidth];
} GameCompMap;

#define GameCompMapHash HashCStr("GameCompMap")

#endif
