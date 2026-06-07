
#pragma once

#include "game_types.h"

#define EPSILON32 1e-6f

#define SQRT(x) __builtin_sqrtf(x)

static inline vector2 vector2_add(const vector2 a, const vector2 b) {
    return v2(a.x + b.x, a.y + b.y);
}

static inline vector2 vector2_scale(const vector2 vector, const f32 factor) {
    return v2(vector.x * factor, vector.y * factor);
}

static inline f32 vector2_length_squared(const vector2 vector) {
    return vector.x * vector.x + vector.y * vector.y;
}

static inline f32 vector2_length(const vector2 vector) {
    return SQRT(vector2_length_squared(vector));
}

static inline vector2 vector2_norm(const vector2 vector) {
    f32 length = vector2_length(vector);
    if (length < EPSILON32) {
        return v2(0, 0);
    }

    return v2(vector.x / length, vector.y / length);
}
