#include "game_rectangle_pack.h"
#include "game_types.h"
#include "game_platform.h"

// skyline packing algortihm I borrowed from julien vernay thank you love you twin mwha
//
// https://jvernay.fr/blog/skyline-2d-packer/implementation/

pack2d pack2d_create(memory_arena *arena, u32 maximum_width, u32 maximum_height) {
    pack2d result = {0};

    if (maximum_width == 0 || maximum_height == 0) {
        return result;
    }

    result.maximum_width = maximum_width;
    result.maximum_height = maximum_height;

    usize maximum_skyline_points = (usize)maximum_width * 2;

    result.points = MEMORY_ARENA_PUSH_ARRAY(arena, vector2_u, maximum_skyline_points);

    return result;
}

bool pack2d_add(pack2d *packer, u32 rectangle_width, u32 rectangle_height, u32 *out_position_x, u32 *out_position_y) {
    if (!packer || !packer->points) {
        return false;
    }

    if (rectangle_width == 0 || rectangle_height == 0) {
        return false;
    }

    if (!packer->is_initialized) {
        packer->is_initialized = true;
        packer->point_count = 1;
        packer->points[0].x = 0;
        packer->points[0].y = 0;
    }

    u32 maximum_width = packer->maximum_width;
    u32 maximum_height = packer->maximum_height;
    u32 point_count = packer->point_count;

    u32 best_index_start = 0xFFFFFFFF;
    u32 best_index_end = 0xFFFFFFFF;
    u32 best_position_x = 0xFFFFFFFF;
    u32 best_position_y = 0xFFFFFFFF;

    for (u32 current_index = 0; current_index < point_count; current_index++) {
        u32 current_x = packer->points[current_index].x;
        u32 current_y = packer->points[current_index].y;

        if (rectangle_width > maximum_width - current_x) {
            break;
        }

        if (current_y >= best_position_y) {
            continue;
        }

        u32 rectangle_right_edge = current_x + rectangle_width;
        u32 overlapping_end_index;

        for (overlapping_end_index = current_index + 1; overlapping_end_index < point_count; overlapping_end_index++) {
            if (rectangle_right_edge <= packer->points[overlapping_end_index].x) {
                break;
            }

            if (current_y < packer->points[overlapping_end_index].y) {
                current_y = packer->points[overlapping_end_index].y;
            }
        }

        if (current_y >= best_position_y) {
            continue;
        }

        if (rectangle_height > maximum_height - current_y) {
            continue;
        }

        best_index_start = current_index;
        best_index_end = overlapping_end_index;
        best_position_x = current_x;
        best_position_y = current_y;
    }

    if (best_index_start == 0xFFFFFFFF) {
        return false;
    }

    u32 removed_points_count = best_index_end - best_index_start;

    vector2_u new_top_left_point;
    new_top_left_point.x = best_position_x;
    new_top_left_point.y = best_position_y + rectangle_height;

    vector2_u new_bottom_right_point;
    new_bottom_right_point.x = best_position_x + rectangle_width;
    new_bottom_right_point.y = packer->points[best_index_end - 1].y;

    bool has_bottom_right_point = false;
    if (best_index_end < point_count) {
        has_bottom_right_point = (new_bottom_right_point.x < packer->points[best_index_end].x);
    } else {
        has_bottom_right_point = (new_bottom_right_point.x < maximum_width);
    }

    u32 inserted_points_count = 1 + (has_bottom_right_point ? 1 : 0);

    if (inserted_points_count > removed_points_count) {
        u32 shift_amount = inserted_points_count - removed_points_count;

        // NOTE: expansion
        for (u32 index = point_count; index > best_index_end; index--) {
            packer->points[(index - 1) + shift_amount] = packer->points[index - 1];
        }

        packer->point_count = point_count + shift_amount;
    } else if (inserted_points_count < removed_points_count) {
        u32 shift_amount = removed_points_count - inserted_points_count;

        // NOTE: shrinking
        for (u32 index = best_index_end; index < point_count; index++) {
            packer->points[index - shift_amount] = packer->points[index];
        }

        packer->point_count = point_count - shift_amount;
    }

    packer->points[best_index_start] = new_top_left_point;
    if (has_bottom_right_point) {
        packer->points[best_index_start + 1] = new_bottom_right_point;
    }

    if (out_position_x) {
        *out_position_x = best_position_x;
    }

    if (out_position_y) {
        *out_position_y = best_position_y;
    }

    return true;
}
