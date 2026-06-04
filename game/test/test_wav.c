//
// TODO: currently only fuzzing, add regular tests later
//

#include "utilities/init.h"

#include "game_wav.h"

#if defined(FUZZING)

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    utilities_initialize();
    sound_load_from_wav(&permanent_arena, &error_stream, data, size);

    return 0;
}

#endif
