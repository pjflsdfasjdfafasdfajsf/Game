#pragma once

// NOTE: these headers do not require CRT/libc so it's fine.
#include <stddef.h>
#include <stdint.h>

#define bool int
#define true 1
#define false 0

// NOTE: integer types.

typedef uintptr_t usize;
typedef intptr_t isize;


typedef uint8_t u8;
typedef int8_t i8;

typedef uint16_t u16;
typedef int16_t i16;

typedef uint32_t u32;
typedef int32_t i32;

typedef uint64_t u64;
typedef int64_t i64;

typedef float f32;

// NOTE: macros.

#define IS_BIT_SET(value, bit_flag) (((value) & (bit_flag)) != 0)

#define ABS(x) (((x) < 0) ? -(x) : (x))
#define MIN(x, y) ((x) > (y) ? (y) : (x))
#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define UNUSED(x) (void)x
#define ARRAY_COUNT(array) (sizeof((array)) / sizeof((array)[0]))
#define ASSERT(expression)     \
    if (!(expression)) {       \
        *(volatile int*)0 = 0; \
    }

#define KILOBYTES(value) ((value) * 1024ULL)
#define MEGABYTES(value) (KILOBYTES(value) * 1024ULL)
#define GIGABYTES(value) (MEGABYTES(value) * 1024ULL)
#define TERABYTES(value) (GIGABYTES(value) * 1024ULL)

#define MILLISECONDS(value) ((value) * 1000.0f)

#define FOURCC(a, b, c, d) (((u32)(a) << 24) | ((u32)(b) << 16) | ((u32)(c) << 8) | ((u32)(d)))

// NOTE: math types.

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

        f32 e[4];
    };
} vector4;

static inline vector4 V4(f32 x, f32 y, f32 z, f32 w) {
    return (vector4){{{x, y, z, w}}};
}

// NOTE: colors

static const vector4 red = { .r = 1.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f};
static const vector4 green = { .r = 0.0f, .g = 1.0f, .b = 0.0f, .a = 1.0f};
static const vector4 blue = { .r = 0.0f, .g = 0.0f, .b = 1.0f, .a = 1.0f};
static const vector4 black = { .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f};

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
} vector2;

static inline vector2 v2(f32 x, f32 y) {
    return (vector2){{{x, y}}};
}

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
} vector2u;

static inline vector2u v2u(u32 x, u32 y) {
    return (vector2u){{{x, y}}};
}

typedef struct {
    union {
        struct {
            vector2 min;
            vector2 max;
        };

        struct {
            f32 x;
            f32 y;
            f32 width;
            f32 height; 
        };
    };
} rectangle;

static inline rectangle rect(f32 x, f32 y, f32 width, f32 height) {
    return (rectangle){ .x = x, .y = y, .width = width, .height = height };
}
