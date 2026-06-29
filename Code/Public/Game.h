
#if !defined(GAME_H)
#define GAME_H

#include "Types.h"

typedef struct GameCompPlayer
{
	// NOTE: Acts as a Player tag so no Data needed
} GameCompPlayer;

#define GameCompPlayerHash HashCStr("GameCompPlayer")

#define MapWidth 40
#define MapHeight 40
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
