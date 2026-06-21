#include "SDL.h"

Int32 main()
{
    SDL App = Init();

    while (Poll())
    {
        Update(&App);
        Render(&App);
    }
}
