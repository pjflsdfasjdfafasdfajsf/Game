#include "SDL.h"

Int32 main()
{
    SDL SDL = Init();

    while (Poll())
    {
        Update(&SDL);
        Render(&SDL);
    }
}
