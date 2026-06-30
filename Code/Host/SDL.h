#if !defined(HOST_SDL_H)
#define HOST_SDL_H

#include <SDL3/SDL.h>

#include "Zip.h"

enum
{
    MouseButtonLeft = SDL_SCANCODE_COUNT + 0,
    MouseButtonRight = SDL_SCANCODE_COUNT + 1,
    MouseButtonMiddle = SDL_SCANCODE_COUNT + 2,
    KeysCount = SDL_SCANCODE_COUNT + 3,
};
typedef struct
{
    SDL_Window *Window;
} SDL;

// NOTE: On any failures all of these function just traps the process.

SDL Init();

Bool Poll(SDL *App);
void Render(SDL *App);
Void Update(SDL *App);

#define LogCritical(Message, ...) SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "%s:%d (%s): " Message, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#endif
