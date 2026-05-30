#include "game_rectangle_pack.h"
#include "game_platform.h"

// Skyline packing algortihm I borrowed from Julien Vernay thank you love you twin mwha
//
// https://jvernay.fr/blog/skyline-2d-packer/implementation/

Pack2D Pack2DCreate(MemoryArena *arena, u32 maximumWidth, u32 maximumHeight) {
    Pack2D result;
    MemoryZero(&result, sizeof(result));

    if (maximumWidth == 0 || maximumHeight == 0) {
        return result;
    }

    result.maximumWidth = maximumWidth;
    result.maximumHeight = maximumHeight;

    usize maximumSkylinePoints = (usize)maximumWidth * 2;

    result.points = MemoryArenaPushArray(arena, Vector2U, maximumSkylinePoints);

    return result;
}

bool Pack2DAdd(Pack2D *packer, u32 rectangleWidth, u32 rectangleHeight, u32 *outPositionX, u32 *outPositionY) {
    if (!packer || !packer->points) {
        return false;
    }

    if (rectangleWidth == 0 || rectangleHeight == 0) {
        return false;
    }

    if (!packer->isInitialized) {
        packer->isInitialized = true;
        packer->pointCount = 1;
        packer->points[0].x = 0;
        packer->points[0].y = 0;
    }

    u32 maximumWidth = packer->maximumWidth;
    u32 maximumHeight = packer->maximumHeight;
    u32 pointCount = packer->pointCount;

    u32 bestIndexStart = 0xFFFFFFFF;
    u32 bestIndexEnd = 0xFFFFFFFF;
    u32 bestPositionX = 0xFFFFFFFF;
    u32 bestPositionY = 0xFFFFFFFF;

    for (u32 currentIndex = 0; currentIndex < pointCount; currentIndex++) {
        u32 currentX = packer->points[currentIndex].x;
        u32 currentY = packer->points[currentIndex].y;

        if (rectangleWidth > maximumWidth - currentX) {
            break;
        }

        if (currentY >= bestPositionY) {
            continue;
        }

        u32 rectangleRightEdge = currentX + rectangleWidth;
        u32 overlappingEndIndex;

        for (overlappingEndIndex = currentIndex + 1; overlappingEndIndex < pointCount; overlappingEndIndex++) {
            if (rectangleRightEdge <= packer->points[overlappingEndIndex].x) {
                break;
            }

            if (currentY < packer->points[overlappingEndIndex].y) {
                currentY = packer->points[overlappingEndIndex].y;
            }
        }

        if (currentY >= bestPositionY) {
            continue;
        }

        if (rectangleHeight > maximumHeight - currentY) {
            continue;
        }

        bestIndexStart = currentIndex;
        bestIndexEnd = overlappingEndIndex;
        bestPositionX = currentX;
        bestPositionY = currentY;
    }

    if (bestIndexStart == 0xFFFFFFFF) {
        return false;
    }

    u32 removedPointsCount = bestIndexEnd - bestIndexStart;

    Vector2U newTopLeftPoint;
    newTopLeftPoint.x = bestPositionX;
    newTopLeftPoint.y = bestPositionY + rectangleHeight;

    Vector2U newBottomRightPoint;
    newBottomRightPoint.x = bestPositionX + rectangleWidth;
    newBottomRightPoint.y = packer->points[bestIndexEnd - 1].y;

    bool hasBottomRightPoint = false;
    if (bestIndexEnd < pointCount) {
        hasBottomRightPoint = (newBottomRightPoint.x < packer->points[bestIndexEnd].x);
    } else {
        hasBottomRightPoint = (newBottomRightPoint.x < maximumWidth);
    }

    u32 insertedPointsCount = 1 + (hasBottomRightPoint ? 1 : 0);

    if (insertedPointsCount > removedPointsCount) {
        u32 shiftAmount = insertedPointsCount - removedPointsCount;

        // NOTE: expansion
        for (u32 index = pointCount; index > bestIndexEnd; index--) {
            packer->points[(index - 1) + shiftAmount] = packer->points[index - 1];
        }

        packer->pointCount = pointCount + shiftAmount;
    } else if (insertedPointsCount < removedPointsCount) {
        u32 shiftAmount = removedPointsCount - insertedPointsCount;

        // NOTE: shrinking
        for (u32 index = bestIndexEnd; index < pointCount; index++) {
            packer->points[index - shiftAmount] = packer->points[index];
        }

        packer->pointCount = pointCount - shiftAmount;
    }

    packer->points[bestIndexStart] = newTopLeftPoint;
    if (hasBottomRightPoint) {
        packer->points[bestIndexStart + 1] = newBottomRightPoint;
    }

    if (outPositionX) {
        *outPositionX = bestPositionX;
    }

    if (outPositionY) {
        *outPositionY = bestPositionY;
    }

    return true;
}
