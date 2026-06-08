#pragma once

#include "game_types.h"
/** NOTE: standard 32-bit float epsilon */
#define FLOAT_EPSILON 1e-6f
#define FLOAT_MAX 3.402823466e+38f

/****************************************************
 * NOTE: vector math
 ****************************************************/

static inline vector2 vector2_add(const vector2 a, const vector2 b)
{
    return v2(a.x + b.x, a.y + b.y);
}

static inline vector2 vector2_sub(const vector2 a, const vector2 b)
{
    return v2(a.x - b.x, a.y - b.y);
}

static inline vector2 vector2_scale(const vector2 vector, const f32 factor)
{
    return v2(vector.x * factor, vector.y * factor);
}

static inline vector2 vector2_abs(const vector2 vector)
{
    return v2(ABS(vector.x), ABS(vector.y));
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
    if (length < FLOAT_EPSILON)
    {
        return v2(0, 0);
    }

    return v2(vector.x / length, vector.y / length);
}

/****************************************************
 * NOTE: AABB
 ****************************************************/

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

static inline aabb_collision_result aabb_collision(const rectangle a, const rectangle b)
{
    aabb_collision_result result = {0};
    result.is_colliding = false;
    result.impact_side = AABB_SIDE_NONE;
    result.penetration_depth = v2(0.0f, 0.0f);

    vector2 a_half = v2(a.width * 0.5f, a.height * 0.5f);
    vector2 b_half = v2(b.width * 0.5f, b.height * 0.5f);

    vector2 a_center = vector2_add(v2(a.x, a.y), a_half);
    vector2 b_center = vector2_add(v2(b.x, b.y), b_half);

    vector2 distance_between_centers = vector2_sub(a_center, b_center);
    /** NOTE: (to not be colliding) */
    vector2 minimum_safe_distance = vector2_add(a_half, b_half);
    vector2 absolute_distance = vector2_abs(distance_between_centers);

    /** NOTE: if the actual distance is greater than the minimum distance on either axis there's a gap between boxes
     * and therefore are not colliding
     */
    if (absolute_distance.x >= minimum_safe_distance.x || absolute_distance.y >= minimum_safe_distance.y)
    {
        return result;
    }

    result.is_colliding = true;

    vector2 overlap = vector2_sub(minimum_safe_distance, absolute_distance);

    if (overlap.x < overlap.y)
    {
        /** NOTE: the collision is primarily horizontal */
        if (distance_between_centers.x > 0.0f)
        {
            /** NOTE: box A center is further right than box B center, so box B hit box A on its left side */
            result.impact_side = AABB_SIDE_LEFT;
            result.penetration_depth = v2(overlap.x, 0.0f);
        }
        else
        {
            result.impact_side = AABB_SIDE_RIGHT;
            result.penetration_depth = v2(-overlap.x, 0.0f);
        }
    }
    else
    {
        /** NOTE: the collision is primarily vertical */
        if (distance_between_centers.y > 0.0f)
        {
            result.impact_side = AABB_SIDE_BOTTOM;
            result.penetration_depth = v2(0.0f, overlap.y);
        }
        else
        {
            result.impact_side = AABB_SIDE_TOP;
            result.penetration_depth = v2(0.0f, -overlap.y);
        }
    }

    return result;
}

/****************************************************
 * NOTE: raycasting
 ****************************************************/

/** please tell me we will have to use both finite and infinite versions this code is so good im so proud of it pls pls */

typedef struct
{
    bool is_hitting;
    /** NOTE: how far along the line segment the hit happened, from 0.0 to 1.0 (if returned by ray_intersect_rectangle_finite,
     * is an absolute distance otherwise)
     */
    f32 hit_time;
    vector2 hit_position;
} raycast_result;

/** NOTE: DO NOT USE THIS, see ray_intersect_rectangle_infinite/finite instead
 *
 * this is an implementation of the slab method:
 * https://en.wikipedia.org/wiki/Slab_method
 *
 * t is how far along the direction vector the ray is allowed to register a hit
 */
static inline raycast_result ray_intersect_rectangle_inner(const vector2 start, const vector2 direction, const f32 t, rectangle rect)
{
    raycast_result result = {0};

    /** NOTE: you may think that this will break in case direction.x/.y is zero but in case of zeroes it would evaluate
     * to Infinity/-Infinity and the next min/max operations will work perfectly fine
     */
    f32 inverse_direction_x = 1.0f / direction.x;
    f32 inverse_direction_y = 1.0f / direction.y;

    f32 time_left = (rect.x - start.x) * inverse_direction_x;
    f32 time_right = (rect.x + rect.width - start.x) * inverse_direction_x;

    f32 entry_time_x = MIN(time_left, time_right);
    f32 exit_time_x = MAX(time_left, time_right);

    f32 time_top = (rect.y - start.y) * inverse_direction_y;
    f32 time_bottom = (rect.y + rect.height - start.y) * inverse_direction_y;

    f32 entry_time_y = MIN(time_top, time_bottom);
    f32 exit_time_y = MAX(time_top, time_bottom);

    f32 entry_time = MAX(entry_time_x, entry_time_y);
    f32 exit_time = MIN(exit_time_x, exit_time_y);

    bool is_overlapping_axes = entry_time <= exit_time;
    bool is_in_front_of_ray = exit_time > 0.0f;
    bool is_within_bounds = entry_time <= t;

    if (is_overlapping_axes && is_in_front_of_ray && is_within_bounds)
    {
        result.is_hitting = true;
        /** NOTE: this handles the case when ray starts inside the box */
        result.hit_time = MAX(entry_time, 0.0f);

        vector2 traveled_vector = vector2_scale(direction, result.hit_time);
        result.hit_position = vector2_add(start, traveled_vector);
    }

    return result;
}

static inline raycast_result ray_intersect_rectangle_finite(const vector2 start, const vector2 end, rectangle rect)
{
    vector2 direction = vector2_sub(end, start);
    return ray_intersect_rectangle_inner(start, direction, 1.0f, rect);
}

/** NOTE: in the returned `raycast_result` `hit_time` instead means the absolute distance
 *  traveled to hit the rectangle
 */
static inline raycast_result ray_intersect_rectangle_infinite(const vector2 start, const vector2 direction, rectangle rect)
{
    vector2 norm = vector2_norm(direction);

    if (norm.x == 0.0f && norm.y == 0.0f)
    {
        return (raycast_result){0};
    }

    return ray_intersect_rectangle_inner(start, norm, FLOAT_MAX, rect);
}
