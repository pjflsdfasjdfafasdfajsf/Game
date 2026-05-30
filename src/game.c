#include "game_platform.h"

UPDATE_AND_RENDER(UpdateAndRender) {
    GlobalInfoStreamWrite("Hello!");
    RenderClearEntireScreen(commandBuffer, RED); // ClearEntireScreen(BLACK);
}
