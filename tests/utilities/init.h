#include "game_types.h"
#include "game_platform.h"

static u8 permanent_block[1024 * 1024 * 64];
static u8 temporary_block[1024 * 1024 * 64];
static u8 error_block[1024 * 4];

static memory_arena permanent_arena;
static memory_arena temporary_arena;
static memory_stream error_stream;
static bool is_initialized = false;

static inline void utilities_initialize(void) {
    if (!is_initialized) {
        memory_arena_initialize(&permanent_arena, permanent_block, sizeof(permanent_block));
        memory_arena_initialize(&temporary_arena, temporary_block, sizeof(temporary_block));
        is_initialized = true;
    }
    memory_arena_clear(&permanent_arena);
    memory_arena_clear(&temporary_arena);
    memory_stream_initialize_writable(&error_stream, error_block, sizeof(error_block));
}
