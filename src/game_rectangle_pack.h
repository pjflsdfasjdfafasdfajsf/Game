#pragma once

#include "game_platform.h"

typedef struct {
    Vector2U *points;
    u32 maximumWidth;
    u32 maximumHeight;
    u32 pointCount;
    bool isInitialized;
} Pack2D;

Pack2D Pack2DCreate(MemoryArena *arena, u32 maximumWidth, u32 maximumHeight);

bool Pack2DAdd(Pack2D *packer, u32 rectangleWidth, u32 rectangleHeight, u32 *outPositionX, u32 *outPositionY);
