#include "game_types.h"
#include "game_platform.h"

static u8 permanentBlock[1024 * 1024 * 64];
static u8 temporaryBlock[1024 * 1024 * 64];
static u8 errorBlock[1024 * 4];

static MemoryArena permanentArena;
static MemoryArena temporaryArena;
static MemoryStream errorStream;
static bool isInitialized = false;

static inline void UtilitiesInitialize(void) {
    if (!isInitialized) {
        MemoryArenaInitialize(&permanentArena, permanentBlock, sizeof(permanentBlock));
        MemoryArenaInitialize(&temporaryArena, temporaryBlock, sizeof(temporaryBlock));
        isInitialized = true;
    }
    MemoryArenaClear(&permanentArena);
    MemoryArenaClear(&temporaryArena);
    MemoryStreamInitializeWritable(&errorStream, errorBlock, sizeof(errorBlock));
}
