//
// NOTE: This is more of a build-time utility though not exactly. Cross-platform file watching (used in cmake watch)
//

#if !defined(__linux__)
#error "TODO: Unsuported platform for file watching"
#endif

// TODO

#include "SDL.h"
#include "Types.h"

typedef Void *Handle;

Handle WatchInit(const char *Dir);
Bool WatchWait(Handle Handle);

// NOTE:
// Arguments:
// 1. What directory to watch
// 2. What to do when a file changes in that directory (command)
Int32 main(Int32 Argc, char **Argv)
{
    if (Argc != 3)
    {
        SDL_Log("USAGE: %s <directory_to_watch> <command_to_run>\n", Argv[0]);

        return 1;
    }
}
