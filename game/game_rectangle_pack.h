#pragma once

#include "game_platform.h"
#include "game_types.h"

typedef struct {
    Vector2U *points;
    u32 MaximumWidth;
    u32 MaximumHeight;
    u32 pointCount;
    bool isInitialized;
} Pack2D;

Pack2D Pack2DCreate(MemoryArena *arena, u32 MaximumWidth, u32 MaximumHeight);

bool Pack2DAdd(Pack2D *packer, u32 rectangleWidth, u32 rectangleHeight, u32 *outPositionX, u32 *outPositionY);
