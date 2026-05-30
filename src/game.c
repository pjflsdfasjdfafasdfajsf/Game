#include "game_platform.h"

UPDATE_AND_RENDER(UpdateAndRender) {
    ClearEntireScreen(commandBuffer, V4(0.0f, 0.0f, 0.0f, 0.0f)); // ClearEntireScreen(BLACK);
    DrawRectangle(commandBuffer, V2(10.0f, 10.0f), V2(50.0f, 50.0f), V4(1.0f, 0.0f, 0.0f, 0.0f));
}
