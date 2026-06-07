#pragma once

#include "game_types.h"
/** NOTE: standard 32-bit float epsilon */
#define EPSILON 1e-6f

/** NOTE: which side of box A was impact during collision */
typedef enum
{
    AABB_SIDE_NONE = 0,
    AABB_SIDE_LEFT,
    AABB_SIDE_RIGHT,
    AABB_SIDE_TOP,
    AABB_SIDE_BOTTOM,
} aabb_side;

typedef struct
{
    bool is_colliding;
    aabb_side impact_side;
    /** NOTE: how far to push box A to resolve collision */
    vector2 penetration_depth;
} aabb_collision_result;

static inline vector2 vector2_add(const vector2 a, const vector2 b)
{
    return v2(a.x + b.x, a.y + b.y);
}

static inline vector2 vector2_scale(const vector2 vector, const f32 factor)
{
    return v2(vector.x * factor, vector.y * factor);
}

static inline f32 vector2_length_squared(const vector2 vector)
{
    return vector.x * vector.x + vector.y * vector.y;
}

static inline f32 vector2_length(const vector2 vector)
{
    return SQRT(vector2_length_squared(vector));
}

static inline vector2 vector2_norm(const vector2 vector)
{
    f32 length = vector2_length(vector);
    if (length < EPSILON)
    {
        return v2(0, 0);
    }

    return v2(vector.x / length, vector.y / length);
}

static inline aabb_collision_result aabb_collision(const rectangle a, const rectangle b)
{
    aabb_collision_result result = {0};
    result.is_colliding = false;
    result.impact_side = AABB_SIDE_NONE;
    result.penetration_depth = v2(0.0f, 0.0f);

    f32 a_half_x = a.width * 0.5f;
    f32 a_half_y = a.height * 0.5f;
    
    f32 b_half_x = b.width * 0.5f;
    f32 b_half_y = b.height * 0.5f;

    vector2 a_center;
    a_center.x = a.x + a_half_x;
    a_center.y = a.y + a_half_y;

    vector2 b_center;
    b_center.x = b.x + b_half_x;
    b_center.y = b.y + b_half_y;

    vector2 distance_between_centers;
    distance_between_centers.x = a_center.x - b_center.x;
    distance_between_centers.y = a_center.y - b_center.y;
    /** NOTE: (to not be colliding) */
    vector2 minimum_safe_distance;
    minimum_safe_distance.x = a_half_x + b_half_x;
    minimum_safe_distance.y = a_half_y + b_half_y;

    f32 absolute_distance_x = ABS(distance_between_centers.x);
    f32 absolute_distance_y = ABS(distance_between_centers.y);
    /** NOTE: if the actual distance is greater than the minimum distance on either distance there's a gap between boxes
     * and therefore are not colliding
     */
    if (absolute_distance_x >= minimum_safe_distance.x)
    {
        return result;
    }
    if (absolute_distance_y >= minimum_safe_distance.y)
    {
        return result;
    }
    result.is_colliding = true;

    f32 overlap_x = minimum_safe_distance.x - absolute_distance_x;
    f32 overlap_y = minimum_safe_distance.y - absolute_distance_y;

    if (overlap_x < overlap_y) {
        /** NOTE: the collision is primarily horizontal */
        if (distance_between_centers.x > 0.0f) {
            /** NOTE: box A center is further right than box B center, so box B hit box A on its left side */
            result.impact_side = AABB_SIDE_LEFT;
            result.penetration_depth = v2(overlap_x, 0.0f);
        } else {
            result.impact_side = AABB_SIDE_RIGHT;
            result.penetration_depth = v2(-overlap_x, 0.0f);
        }
    } else {
        /** NOTE: the collision is primarily vertical */
        if (distance_between_centers.y > 0.0f) {
            result.impact_side = AABB_SIDE_BOTTOM;
            result.penetration_depth = v2(0.0f, overlap_y);
        } else {
            result.impact_side = AABB_SIDE_TOP;
            result.penetration_depth = v2(0.0f, -overlap_y);
        }
    }

    return result;
}