#pragma once

// NOTE: This header does not require CRT/libc so we're fine.
#include <stdint.h>

// TODO: game_types.h once it becomes a problem

#define bool int
#define true 1
#define false 0

typedef size_t usize;
typedef uint8_t u8;
typedef uint32_t u32;
typedef float f32;

typedef struct {
    union {
        struct {
            f32 r;
            f32 g;
            f32 b;
            f32 a;
        };

        struct {
            f32 x;
            f32 y;
            f32 z;
            f32 w;
        };
    };
} Vector4;

static inline Vector4 V4(f32 x, f32 y, f32 z, f32 w) {
    return (Vector4){{x, y, z, w}};
}

typedef struct {
    union {
        struct {
            f32 width;
            f32 height;
        };

        struct {
            f32 x;
            f32 y;
        };
    };
} Vector2;

static inline Vector2 V2(f32 x, f32 y) {
    return (Vector2){{x, y}};
}

typedef struct {
    f32 position[3];
    f32 color[4];
    f32 uv[2];
} Vertex;

static inline void MemoryZero(void *destination, usize count) {
    char *bytePointer = (char *)destination;

    while (count--) {
        *bytePointer++ = 0;
    }
}

static inline void MemoryCopyForwards(void *destination, const void *source, usize count) {
    char *destinationPointer = (char *)destination;
    const char *sourcePointer = (const char *)source;

    while (count--) {
        *destinationPointer++ = *sourcePointer++;
    }
}

static inline usize StringGetLength(const char *string) {
    const char *characterPointer = string;

    while (*characterPointer) {
        characterPointer++;
    }

    return characterPointer - string;
}
