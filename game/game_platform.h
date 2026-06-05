#pragma once

#include "game_types.h"

// NOTE: doesnt need linking with libc
#include <stdarg.h>

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

#define UNTEXTURED 0

// NOTE: this vertex type must be shared between all renderer implementations.
typedef struct {
    f32 position[3];
    f32 color[4];
    f32 uv[2];
} vertex;

// NOTE: shared utilities.

static inline bool is_power_of_two(usize value) {
    return (value != 0) && ((value & (value - 1)) == 0);
}

// NOTE: `alignment` must be power of two
static inline usize memory_align_size(usize current_size, usize alignment) {
    if (!is_power_of_two(alignment)) {
        return current_size;
    }

    usize alignment_mask = alignment - 1;
    usize aligned_size = (current_size + alignment_mask) & ~alignment_mask;

    return aligned_size;
}

static inline void memory_zero(void *destination, usize count) {
    char *byte_pointer = (char *)destination;

    while (count--) {
        *byte_pointer++ = 0;
    }
}

#define ZERO_STRUCT(instance) memory_zero(&(instance), sizeof(instance))
#define ZERO_ARRAY(array) memory_zero((array), sizeof(array))
#define ZERO_ITEMS(pointer, count) memory_zero((pointer), sizeof(*(pointer)) * (count))
#define RETURN_ZEROED(instance) \
    do {                        \
        ZERO_STRUCT(instance);  \
        return (instance);      \
    } while (0)

static inline void memory_copy_forwards(void *destination, const void *source, usize count) {
    char *destination_pointer = (char *)destination;
    const char *source_pointer = (const char *)source;

    while (count--) {
        *destination_pointer++ = *source_pointer++;
    }
}

static inline bool memory_equals(const void *memory_a, const void *memory_b, usize count) {
    const u8 *byte_pointer_a = (const u8 *)memory_a;
    const u8 *byte_pointer_b = (const u8 *)memory_b;

    while (count--) {
        if (*byte_pointer_a++ != *byte_pointer_b++) {
            return false;
        }
    }

    return true;
}

static inline usize string_get_length(const char *string) {
    const char *character_pointer = string;

    while (*character_pointer) {
        character_pointer++;
    }

    return character_pointer - string;
}

static inline isize string_find_last_occurrence_of_character(const char *string, char target_character) {
    isize last_found_index = -1;
    usize current_index = 0;

    while (string[current_index] != '\0') {
        if (string[current_index] == target_character) {
            last_found_index = (isize)current_index;
        }
        current_index++;
    }

    return last_found_index;
}

static inline void string_copy(char *destination, usize destination_capacity, const char *source) {
    if (!destination || !source || destination_capacity == 0) {
        return;
    }

    usize source_length = string_get_length(source);
    usize bytes_to_copy = (source_length < (destination_capacity - 1)) ? source_length : (destination_capacity - 1);

    memory_copy_forwards(destination, source, bytes_to_copy);
    destination[bytes_to_copy] = '\0';
}

static inline void string_append(char *destination, usize destination_capacity, const char *source) {
    if (!destination || !source || destination_capacity == 0) {
        return;
    }

    usize current_length = string_get_length(destination);
    usize remaining_capacity = destination_capacity - current_length;

    if (remaining_capacity > 0) {
        string_copy(destination + current_length, remaining_capacity, source);
    }
}

///

static inline u16 read_uint16_big_endian(const u8 *memory) {
    return ((u16)memory[0] << 8) |
           ((u16)memory[1] << 0);
}

static inline i16 read_int16_big_endian(const u8 *memory) {
    return (i16)read_uint16_big_endian(memory);
}

static inline u32 read_uint32_big_endian(const u8 *memory) {
    return ((u32)memory[0] << 24) |
           ((u32)memory[1] << 16) |
           ((u32)memory[2] << 8) |
           ((u32)memory[3] << 0);
}

static inline u64 read_uint64_big_endian(const u8 *memory) {
    return ((u64)memory[0] << 56) |
           ((u64)memory[1] << 48) |
           ((u64)memory[2] << 40) |
           ((u64)memory[3] << 32) |
           ((u64)memory[4] << 24) |
           ((u64)memory[5] << 16) |
           ((u64)memory[6] << 8) |
           ((u64)memory[7] << 0);
}

static inline i64 read_int64_big_endian(const u8 *memory) {
    return (i64)read_uint64_big_endian(memory);
}

///

static inline u16 read_uint16_little_endian(const u8 *memory) {
    return (u16)((u16)memory[0] | ((u16)memory[1] << 8));
}

static inline u32 read_uint32_little_endian(const u8 *memory) {
    return (u32)((u32)memory[0] | ((u32)memory[1] << 8) |
                 ((u32)memory[2] << 16) | ((u32)memory[3] << 24));
}

typedef struct {
    u8 *base_pointer;
    usize total_capacity;
    usize current_offset;
} memory_arena;

typedef struct {
    memory_arena *arena;
    usize saved_offset;
} memory_arena_checkpoint;

static inline void memory_arena_initialize(memory_arena *arena, void *backing_memory, usize total_capacity) {
    if (!arena) {
        return;
    }

    arena->base_pointer = (u8 *)backing_memory;
    arena->total_capacity = total_capacity;
    arena->current_offset = 0;
}

static inline usize memory_arena_get_remaining_capacity(const memory_arena *arena) {
    if (!arena) {
        return 0;
    }

    return arena->total_capacity - arena->current_offset;
}

static inline void *memory_arena_allocate_bytes_aligned(memory_arena *arena, usize allocation_size, usize alignment) {
    if (!arena || !arena->base_pointer || allocation_size == 0 || alignment == 0) {
        return 0;
    }

    if ((alignment & (alignment - 1)) != 0) {
        return 0;
    }

    usize aligned_offset = memory_align_size(arena->current_offset, alignment);

    if (aligned_offset + allocation_size > arena->total_capacity) {
        return 0;
    }

    void *allocated_memory = (void *)(arena->base_pointer + aligned_offset);
    arena->current_offset = aligned_offset + allocation_size;

    return allocated_memory;
}

static inline void *memory_arena_allocate_bytes(memory_arena *arena, usize allocation_size) {
    return memory_arena_allocate_bytes_aligned(arena, allocation_size, 8);
}

static inline void *memory_arena_allocate_bytes_and_zero(memory_arena *arena, usize allocation_size) {
    void *allocated_memory = memory_arena_allocate_bytes(arena, allocation_size);

    if (allocated_memory) {
        memory_zero(allocated_memory, allocation_size);
    }

    return allocated_memory;
}

static inline void memory_arena_clear(memory_arena *arena) {
    if (arena) {
        arena->current_offset = 0;
    }
}

static inline memory_arena_checkpoint memory_arena_create_checkpoint(memory_arena *arena) {
    memory_arena_checkpoint result = {0};

    if (arena) {
        result.arena = arena;
        result.saved_offset = arena->current_offset;
    }

    return result;
}

static inline void memory_arena_restore_checkpoint(memory_arena_checkpoint checkpoint) {
    if (checkpoint.arena) {
        checkpoint.arena->current_offset = checkpoint.saved_offset;
    }
}

#define MEMORY_ARENA_PUSH_ARRAY(arena, type, count) (type *)memory_arena_allocate_bytes_and_zero((arena), sizeof(type) * (count))
#define MEMORY_ARENA_PUSH_BYTES(arena, size) memory_arena_allocate_bytes_and_zero((arena), (size))

typedef struct {
    const u8 *memory;
    usize capacity;
    usize offset;

    bool has_overflowed;
    bool is_writable;
} memory_stream;

static inline void memory_stream_initialize_read_only(memory_stream *stream, const void *memory, usize capacity) {
    ZERO_STRUCT(*stream);
    stream->memory = (const u8 *)memory;
    stream->capacity = capacity;
    stream->is_writable = false;
}

static inline void memory_stream_initialize_writable(memory_stream *stream, void *memory, usize capacity) {
    ZERO_STRUCT(*stream);
    stream->memory = (const u8 *)memory;
    stream->capacity = capacity;
    stream->is_writable = true;
}

static inline bool memory_stream_has_space(memory_stream *stream, usize read_size) {
    if (stream->has_overflowed || !stream->memory || stream->offset + read_size > stream->capacity) {
        stream->has_overflowed = true;

        return false;
    }

    return true;
}

static inline u8 memory_stream_read_uint8(memory_stream *stream) {
    if (!memory_stream_has_space(stream, 1)) {
        return 0;
    }
    u8 result = stream->memory[stream->offset];
    stream->offset += 1;

    return result;
}

static inline u32 memory_stream_read_uint32_little_endian(memory_stream *stream) {
    u32 result = read_uint32_little_endian(&stream->memory[stream->offset]);
    stream->offset += 4;
    return result;
}

static inline u16 memory_stream_read_uint16_big_endian(memory_stream *stream) {
    if (!memory_stream_has_space(stream, 2)) {
        return 0;
    }
    u16 result = read_uint16_big_endian(&stream->memory[stream->offset]);
    stream->offset += 2;

    return result;
}

static inline i16 memory_stream_read_int16_big_endian(memory_stream *stream) {
    return (i16)memory_stream_read_uint16_big_endian(stream);
}

static inline u32 memory_stream_read_uint32_big_endian(memory_stream *stream) {
    if (!memory_stream_has_space(stream, 4)) {
        return 0;
    }
    u32 result = read_uint32_big_endian(&stream->memory[stream->offset]);
    stream->offset += 4;

    return result;
}

static inline i64 memory_stream_read_int64_big_endian(memory_stream *stream) {
    if (!memory_stream_has_space(stream, 8)) {
        return 0;
    }
    i64 result = read_int64_big_endian(&stream->memory[stream->offset]);
    stream->offset += 8;

    return result;
}

static inline bool memory_stream_write_bytes(memory_stream *stream, const void *data, usize size) {
    if (!stream->is_writable) {
        stream->has_overflowed = true;

        return false;
    }

    if (!memory_stream_has_space(stream, size)) {
        return false;
    }

    u8 *writable_memory = (u8 *)stream->memory;
    memory_copy_forwards((void *)(writable_memory + stream->offset), data, size);
    stream->offset += size;

    return true;
}

static inline bool memory_stream_write_uint8(memory_stream *stream, u8 value) {
    return memory_stream_write_bytes(stream, &value, 1);
}

static inline bool memory_stream_write_string(memory_stream *stream, const char *string) {
    if (!string) {
        return false;
    }
    usize length = string_get_length(string);
    return memory_stream_write_bytes(stream, string, length);
}

static char decimal_characters[] = "0123456789";
static char lower_hex_characters[] = "0123456789abcdef";
static char upper_hex_characters[] = "0123456789ABCDEF";

static inline bool memory_stream_write_ascii_from_u64(memory_stream *stream, u64 value, u32 base, char *digits) {
    usize string_length = 0;
    u64 copy_value = value;

    // get string length
    do {
        copy_value /= base;
        ++string_length;
    } while (copy_value != 0);

    char characters[string_length];
    u32 index = 0;
    copy_value = value;

    // write characters into array
    do {
        characters[index++] = digits[copy_value % base];
        copy_value /= base;
    } while (copy_value != 0);

    // write characters into stream
    while (index != 0) {
        if (!memory_stream_write_uint8(stream, characters[--index])) {
            return false;
        }
    }

    return true;
}

static inline bool memory_stream_write_string_format(memory_stream *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);

    char *at = (char *)format;
    while (*at) {
        if (*at != '%') {
            if (!memory_stream_write_uint8(stream, *at)) {
                return false;
            }
            ++at;
            continue;
        }
        ++at;

        switch (*at) {
        case 's': {
            char *string = va_arg(args, char *);
            if (!memory_stream_write_string(stream, string)) {
                return false;
            }
        } break;
        case 'u': {
            if (!memory_stream_write_ascii_from_u64(stream, (u64)va_arg(args, u32), 10, decimal_characters)) {
                return false;
            }
        } break;
        case 'd': {
            i32 n = va_arg(args, i32);
            if (n < 0) {
                n = -n;
                if (!memory_stream_write_uint8(stream, '-')) {
                    return false;
                }
            }

            if (!memory_stream_write_ascii_from_u64(stream, (u64)n, 10, decimal_characters)) {
                return false;
            }
        } break;
        case 'x':
        case 'X': {
            char *characters = *at == 'x' ? lower_hex_characters : upper_hex_characters;
            if (!memory_stream_write_string(stream, "0x")) {
                return false;
            }
            if (!memory_stream_write_ascii_from_u64(stream, (u64)va_arg(args, u32), 16, characters)) {
                return false;
            }
        } break;
        }

        ++at;
    }

    va_end(args);

    return true;
}

#define STRINGIFY(x) #x
#define STRINGIFY2(x) STRINGIFY(x)

// --------------------------------------------------

// NOTE: services that the platform provides to game.

typedef enum {
    render_command_type_none = 0,
    render_command_type_clear_entire_screen,
    render_command_type_draw_rectangle,
    render_command_type_allocate_texture,
} render_command_type;

typedef struct {
    u32 type;
    u32 size;
} render_command_header;

typedef struct {
    render_command_header header;
    vector4 color;
} render_command_clear_entire_screen;

typedef struct {
    render_command_header header;
    vector2 position;
    vector2 size;
    vector4 color;
    u32 texture;
} render_command_draw_rectangle;

typedef struct {
    render_command_header header;
    u32 index;
    u32 bytes_per_pixel;
    vector2u size;
    const void *pixels;
} render_command_allocate_texture;

typedef struct {
    u8 *base_pointer;
    usize total_capacity;
    usize current_offset;
    u32 command_count;
    // NOTE: this becomes true if a command that did not fit was pushed in
    bool has_overflowed;
} render_command_buffer;

static inline void render_command_buffer_initialize(render_command_buffer *command_buffer, void *backing_memory, usize total_capacity) {
    if (!command_buffer) {
        return;
    }

    memory_zero(command_buffer, sizeof(render_command_buffer));

    if (backing_memory && total_capacity > 0) {
        command_buffer->base_pointer = (u8 *)backing_memory;
        command_buffer->total_capacity = total_capacity;
    }
}

static inline void render_command_buffer_reset(render_command_buffer *command_buffer) {
    if (!command_buffer) {
        return;
    }

    command_buffer->current_offset = 0;
    command_buffer->command_count = 0;
    command_buffer->has_overflowed = false;
}

static inline void *render_command_buffer_allocate_bytes(render_command_buffer *command_buffer, u32 type, usize requested_size) {
    if (!command_buffer || command_buffer->has_overflowed || !command_buffer->base_pointer || requested_size < sizeof(render_command_header)) {
        return 0;
    }

    usize aligned_size = memory_align_size(requested_size, 8);

    if (command_buffer->current_offset + aligned_size > command_buffer->total_capacity) {
        command_buffer->has_overflowed = true;

        return 0;
    }

    void *allocated_memory = (void *)(command_buffer->base_pointer + command_buffer->current_offset);
    memory_zero(allocated_memory, aligned_size);

    render_command_header *header = (render_command_header *)allocated_memory;
    header->type = type;
    header->size = (u32)aligned_size;

    command_buffer->current_offset += aligned_size;
    command_buffer->command_count++;

    return allocated_memory;
}

#define RENDER_COMMAND_BUFFER_PUSH_COMMAND(buffer, command_name) (render_command_##command_name *)render_command_buffer_allocate_bytes((buffer), render_command_type_##command_name, sizeof(render_command_##command_name))

// NOTE: even though without render prefix it looks a bit less pretty, with it you can easily see all render commands by just typing in 'render'
// easily rather than guessing their names

static inline void render_clear_entire_screen(render_command_buffer *command_buffer, vector4 color) {
    if (!command_buffer) {
        return;
    }

    render_command_clear_entire_screen *command = RENDER_COMMAND_BUFFER_PUSH_COMMAND(command_buffer, clear_entire_screen);

    if (command) {
        command->color = color;
    }
}

static inline void render_draw_rectangle(render_command_buffer *command_buffer, vector2 position, vector2 size, vector4 color, u32 texture) {
    if (!command_buffer) {
        return;
    }

    render_command_draw_rectangle *command = RENDER_COMMAND_BUFFER_PUSH_COMMAND(command_buffer, draw_rectangle);

    if (command) {
        command->position = position;
        command->size = size;
        command->color = color;
        command->texture = texture;
    }
}

static inline void render_allocate_texture(render_command_buffer *command_buffer, u32 index, vector2u size, u32 bytes_per_pixel, const void *pixels) {
    if (!command_buffer) {
        return;
    }

    render_command_allocate_texture *command = RENDER_COMMAND_BUFFER_PUSH_COMMAND(command_buffer, allocate_texture);

    if (command) {
        command->index = index;
        command->size = size;
        command->bytes_per_pixel = bytes_per_pixel;
        command->pixels = pixels;
    }
}

// NOTE: services that the game provides to platform.

typedef struct {
    // NOTE: these two standard streams are dumped to console by the platform.
    memory_stream *standard_error_stream;
    memory_stream *standard_info_stream;

    memory_arena permanent_arena;
    memory_arena temporary_arena;

    bool is_initialized;
} game_memory;

typedef struct {
    i32 half_transition_count;
    bool ended_down;
} game_button;

// NOTE: when adding a key you MUST do a clean rebuild
// make wont recognize which files to rebuild and you also
// wont get compile errors but the input will be messed up.
#define KEYS \
    KEY(w, W) \
    KEY(s, S) \
    KEY(a, A) \
    KEY(d, D) \
    
typedef struct {
    union {
#define KEY(...) +1
        game_button buttons[0 KEYS];
#undef KEY

        struct {
#define KEY(lower, upper) game_button lower;
            KEYS
#undef KEY
        };
    };
} game_input;

// went down at least once and is currently down
#define was_pressed(button) ((button).ended_down && (button).half_transition_count > 0)
// released this frame
#define was_released(button) (!(button).ended_down && (button).half_transition_count > 0)
#define is_down(button) ((button).ended_down)

#define UPDATE_AND_RENDER(name) void name(game_memory *memory, game_input *input, render_command_buffer *command_buffer)
typedef UPDATE_AND_RENDER(update_and_render_function);

typedef struct {
    u32 samples_per_second;
    u32 channel_count;
    u32 frame_count;
    f32 *samples;
} sound_buffer;

#define GET_SOUND_SAMPLES(name) void name(sound_buffer *sound_buffer)
typedef GET_SOUND_SAMPLES(get_sound_samples_function);
