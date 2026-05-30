#pragma once

// NOTE: This header does not require CRT/libc so we're fine.
#include <stdint.h>

// TODO: game_types.h once it becomes a problem

#define bool int
#define true 1
#define false 0

typedef size_t usize;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef float f32;

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

#define ABS(x) (((x) < 0) ? -(x) : (x))
#define UNUSED(x) (void)x

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
    return (Vector4){{{x, y, z, w}}};
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

typedef struct {
    union {
        struct {
            u32 width;
            u32 height;
        };

        struct {
            u32 x;
            u32 y;
        };
    };
} UnsignedVector2;

static inline Vector2 V2(f32 x, f32 y) {
    return (Vector2){{{x, y}}};
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

static inline bool MemoryEquals(const void *memoryA, const void *memoryB, usize count) {
    const u8 *bytePointerA = (const u8 *)memoryA;
    const u8 *bytePointerB = (const u8 *)memoryB;

    while (count--) {
        if (*bytePointerA++ != *bytePointerB++) {
            return false;
        }
    }

    return true;
}

static inline usize StringGetLength(const char *string) {
    const char *characterPointer = string;

    while (*characterPointer) {
        characterPointer++;
    }

    return characterPointer - string;
}
