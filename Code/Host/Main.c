#include "SDL.h"

Int32 main()
{
    SDL App = Init();

    while (Poll(&App))
    {
        Update(&App);
        Render(&App);
    }
}
