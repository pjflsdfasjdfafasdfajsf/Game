#include "game_platform.h"
#include "game_png.h"
#include "game_types.h"

UPDATE_AND_RENDER(update_and_render)
{
    if (!memory->is_initialized)
    {
        memory->is_initialized = true;
    }

    render_clear_entire_screen(render_buffer, black);
    render_draw_rectangle(render_buffer, v2(10, 10), v2(200, 200), red, 0);
}

GET_SOUND_SAMPLES(get_sound_samples)
{
    UNUSED(sound_buffer);
}