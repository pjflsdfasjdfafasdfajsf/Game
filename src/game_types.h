#pragma once

// NOTE: These headers do not require CRT/libc so it's fine.
#include <stddef.h>
#include <stdint.h>

#define bool int
#define true 1
#define false 0

// NOTE: Integer types.

typedef size_t usize;

typedef uint8_t u8;
typedef int8_t i8;

typedef uint16_t u16;
typedef int16_t i16;

typedef uint32_t u32;
typedef int32_t i32;

typedef uint64_t u64;
typedef int64_t i64;

typedef float f32;

// NOTE: Macros.

#define ABS(x) (((x) < 0) ? -(x) : (x))
#define MIN(x, y) ((x) > (y) ? (y) : (x))
#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define UNUSED(x) (void)x

#define KILOBYTES(value) ((value) * 1024ULL)
#define MEGABYTES(value) (KILOBYTES(value) * 1024ULL)
#define GIGABYTES(value) (MEGABYTES(value) * 1024ULL)
#define TERABYTES(value) (GIGABYTES(value) * 1024ULL)

#define FOURCC(a, b, c, d) (((u32)(a) << 24) | ((u32)(b) << 16) | ((u32)(c) << 8) | ((u32)(d)))

// NOTE: Math types.

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
} Vector2U;

static inline Vector2 V2(f32 x, f32 y) {
    return (Vector2){{{x, y}}};
}
