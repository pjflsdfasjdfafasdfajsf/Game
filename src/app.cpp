#include "app.h"

#include "SDK.h"

#define MAP_TILE_SIZE 80

/// Centers the camera on WHO smoothly, clamped to never show past WORLD.
static void UpdateCamera(Camera &camera, Vector2 target, Vector2i viewport, Vector2 world, Float delta_seconds)
{
    Vector2 desired = V2(Clamp(target.x - viewport.x * 0.5f, 0, world.x - viewport.x), Clamp(target.y - viewport.y * 0.5f, 0, world.y - viewport.y));

    Float speed = 10.0f;
    camera.position = camera.position + (desired - camera.position) * (speed * delta_seconds);
}

enum
{
    TILE_EMPTY = 0,
    TILE_SOLID = 1,
    TILE_HOOK = 2,
};

namespace map
{
/// Returns TRUE if the tile at a given coordinates X, Y is set to 1.
/// Tiles outside of the map are not considered solid.
static bool TileIsSolid(State &state, Int32 x, Int32 y)
{
    bool is_outside_the_map = x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT;
    if (is_outside_the_map)
    {
        return false;
    }

    return state.map.grid[y][x] == 1;
}

/// Returns TRUE if the rectangle overlaps any solid tile.
static bool IsOverlappingSolidTile(State &state, Rectangle rectangle)
{
    Vector2i first_touched_tile = V2i(
        (Int32)Floor(rectangle.position.x / MAP_TILE_SIZE),
        (Int32)Floor(rectangle.position.y / MAP_TILE_SIZE));

    Vector2i last_touched_tile = V2i(
        (Int32)Floor((rectangle.position.x + rectangle.size.width - 0.001f) / MAP_TILE_SIZE),
        (Int32)Floor((rectangle.position.y + rectangle.size.height - 0.001f) / MAP_TILE_SIZE));

    SDL_FRect player = {
        rectangle.position.x,
        rectangle.position.y,
        rectangle.size.width,
        rectangle.size.height};

    for (Int32 y = first_touched_tile.y; y <= last_touched_tile.y; y++)
    {
        for (Int32 x = first_touched_tile.x; x <= last_touched_tile.x; x++)
        {

            if (!TileIsSolid(state, x, y))
            {
                continue;
            }

            SDL_FRect tile = {
                (Float)(x * MAP_TILE_SIZE),
                (Float)(y * MAP_TILE_SIZE),
                (Float)MAP_TILE_SIZE,
                (Float)MAP_TILE_SIZE,
            };

            if (SDL_HasRectIntersectionFloat(&player, &tile))
            {
                return true;
            }
        }
    }
    return false;
}

static void MoveAndCollide(State &state, Vector2 &position, Vector2 size, Vector2 move)
{
    Float position_before_moving_x = position.x;
    position.x += move.x;
    if (IsOverlappingSolidTile(state, Rectangle::GetCentered(position, size)))
    {
        position.x = position_before_moving_x;

        Float direction = CopySign(1.0f, move.x);
        while (!IsOverlappingSolidTile(state, Rectangle::GetCentered(V2(position.x + direction, position.y), size)))
        {
            position.x += direction;
        }
    }

    Float position_before_moving_y = position.y;
    position.y += move.y;
    if (IsOverlappingSolidTile(state, Rectangle::GetCentered(position, size)))
    {
        position.y = position_before_moving_y;

        Float direction = CopySign(1.0f, move.y);
        while (!IsOverlappingSolidTile(state, Rectangle::GetCentered(V2(position.x, position.y + direction), size)))
        {
            position.y += direction;
        }
    }
}

static bool FindNearestHookWithinRadius(State &state, Vector2 origin, Float radius, Vector2 *out_nearest_hook)
{
    bool result = false;
    Float closest_distance_squared = radius * radius;

    for (Int32 y = 0; y < MAP_HEIGHT; y++)
    {
        for (Int32 x = 0; x < MAP_WIDTH; x++)
        {
            bool is_hook_tile = (state.map.grid[y][x] == TILE_HOOK);

            if (is_hook_tile)
            {
                Vector2 center = V2((Float)(x * MAP_TILE_SIZE) + (MAP_TILE_SIZE * 0.5f), (Float)(y * MAP_TILE_SIZE) + (MAP_TILE_SIZE * 0.5f));

                Float distance_squared = GetDistanceSquared(origin, center);
                bool is_closer_than_previous = (distance_squared <= closest_distance_squared);

                if (is_closer_than_previous)
                {
                    closest_distance_squared = distance_squared;
                    *out_nearest_hook = center;
                    result = true;
                }
            }
        }
    }

    return result;
}

} // namespace map

static void DrawMap(State &state, RenderCommandBuffer &buffer)
{
    for (Int32 y = 0; y < MAP_HEIGHT; y++)
    {
        for (Int32 x = 0; x < MAP_WIDTH; x++)
        {
            Int32 tile = state.map.grid[y][x];
            if (tile == 0)
            {
                // an empty tile
                continue;
            }

            Vector2 screen = state.camera.WorldToScreen(V2((Float)(x * MAP_TILE_SIZE), (Float)(y * MAP_TILE_SIZE)));
            RGBA color = BLACK;

            Rectangle rectangle = Rectangle::FromVectors(screen, V2((Float)MAP_TILE_SIZE, (Float)MAP_TILE_SIZE));

            switch (tile)
            {
            case TILE_SOLID:
            {
                color = WHITE;
            }
            break;
            case TILE_HOOK:
            {
                color = Color(0, 150, 255);

                rectangle.position.x += MAP_TILE_SIZE * 0.25f;
                rectangle.position.y += MAP_TILE_SIZE * 0.25f;
                rectangle.size.width *= 0.5f;
                rectangle.size.height *= 0.5f;
            }
            break;
            }

            buffer.DrawRectangle(rectangle, color);
        }
    }
}

//
// NOTE: player
//

#define PLAYER_SIZE 40.0f
#define PLAYER_HOOK_RADIUS 600.0f
#define PLAYER_HOOK_FORCE 1000.0f
// in pixels/sec
#define PLAYER_MAX_SPEED 600.0f
#define PLAYER_DASH_SPEED 1500.0f
#define PLAYER_JUMP_SPEED 1000.0f
#define PLAYER_SLAM_SPEED 2500.0f
#define PLAYER_ACCELERATION 3000.0f
#define PLAYER_FRICTION 2500.0f
#define PLAYER_GRAVITY 2000.0f
// how close to 0 velocity.y needs to be to trigger the apex
#define PLAYER_APEX_THRESHOLD 150.0f
#define PLAYER_APEX_GRAVITY_MULTIPLIER 0.5f
#define PLAYER_APEX_CONTROL_MULTIPLIER 2.0f
// in seconds
#define PLAYER_DASH_DURATION 0.15f
#define PLAYER_DASH_COOLDOWN 1.0f
#define PLAYER_JUMP_BUFFER_DURATION 0.15f

namespace player
{

static bool IsOnGround(State &state)
{
    Rectangle feet = Rectangle::GetCentered(state.player.position, V2(PLAYER_SIZE, PLAYER_SIZE));
    feet.position.y += 1.0f;

    return map::IsOverlappingSolidTile(state, feet);
}

// BEFORE
static void UpdateSwing(State &state)
{
    Float distance = GetDistance(state.player.position, state.player.hook_target);
    Vector2 offset = state.player.position - state.player.hook_target;

    bool closer_than_rope = distance <= state.player.hook_rope_length || distance == 0.0f;
    if (closer_than_rope)
    {
        return;
    }

    Vector2 normal = Normalize(offset, distance);
    Vector2 position = state.player.hook_target + normal * state.player.hook_rope_length;

    Vector2 correction = position - state.player.position;
    map::MoveAndCollide(state, state.player.position, V2(PLAYER_SIZE, PLAYER_SIZE), correction);

    Float velocity = state.player.velocity.Dot(normal);
    if (velocity > 0.0f)
    {
        state.player.velocity.x -= normal.x * velocity;
        state.player.velocity.y -= normal.y * velocity;
    }
}

}; // namespace player

static void UpdatePlayer(State &state)
{
    if (state.player.jump_buffer_timer > 0.0f)
    {
        state.player.jump_buffer_timer -= state.time.delta;
    }
    if (state.input.jump.pressed)
    {
        state.player.jump_buffer_timer = PLAYER_JUMP_BUFFER_DURATION;
    }

    state.player.hook_cooldown -= state.time.delta;

    // NOTE: Behavior.
    switch (state.player.state)
    {
    case PlayerState_Normal:
    {
        if (state.player.dash_timer > 0.0f)
        {
            state.player.dash_timer -= state.time.delta;
        }

        Float horizontal_input = (Float)state.input.right.is_down - (Float)state.input.left.is_down;
        bool is_near_jump_apex = Abs(state.player.velocity.y) < PLAYER_APEX_THRESHOLD && !player::IsOnGround(state);

        Float target_speed = horizontal_input * PLAYER_MAX_SPEED;
        Float acceleration_rate = (horizontal_input != 0.0f) ? PLAYER_ACCELERATION : PLAYER_FRICTION;

        bool moving_fast = Abs(state.player.velocity.x) > PLAYER_MAX_SPEED;
        bool holding_same_direction = CopySign(1.0f, state.player.velocity.x) == horizontal_input;

        if (!player::IsOnGround(state) && horizontal_input == 0.0f)
        {
            acceleration_rate = 0.0f;
        }
        else if (!player::IsOnGround(state) && moving_fast && holding_same_direction)
        {
            target_speed = state.player.velocity.x;
        }

        if (is_near_jump_apex)
        {
            acceleration_rate *= PLAYER_APEX_CONTROL_MULTIPLIER;
        }

        state.player.velocity.x = Approach(state.player.velocity.x, target_speed, acceleration_rate * state.time.delta);

        // NOTE: Jumping.
        if (state.player.jump_buffer_timer > 0.0f && player::IsOnGround(state))
        {
            state.player.velocity.y = -PLAYER_JUMP_SPEED;
            state.player.jump_buffer_timer = 0.0f;
        }

        if (!state.input.jump.is_down)
        {
            Float short_jump_threshold = -PLAYER_JUMP_SPEED * 0.5f;
            state.player.velocity.y = Max(state.player.velocity.y, short_jump_threshold);
        }

        // NOTE: Dashing.
        if (state.input.dash.is_down && state.player.dash_timer <= 0.0f)
        {
            state.player.state = PlayerState_Dash;
            state.player.dash_timer = PLAYER_DASH_DURATION;

            if (horizontal_input != 0.0f)
            {
                state.player.dash_direction = horizontal_input;
            }
            else
            {
                state.player.dash_direction = CopySign(1.0f, state.player.velocity.x);
            }
        }

        // NOTE: Slamming.
        if (state.input.slam.is_down && !player::IsOnGround(state))
        {
            state.player.state = PlayerState_Slam;
            state.player.velocity.x = 0.0f;
            state.player.velocity.y = PLAYER_SLAM_SPEED;
        }

        // NOTE: Hook.
        Vector2 nearest_hook = Vector2::Zero();
        if (state.input.hook.is_down && state.player.hook_cooldown <= 0.0f && map::FindNearestHookWithinRadius(state, state.player.position, PLAYER_HOOK_RADIUS, &nearest_hook))
        {
            state.player.state = PlayerState_Hook;
            state.player.hook_target = nearest_hook;

            state.player.hook_rope_length = GetDistance(state.player.position, nearest_hook);
        }

        // NOTE: Gravity.
        state.player.velocity.y += PLAYER_GRAVITY * state.time.delta;
    }
    break;
    case PlayerState_Dash:
    {
        state.player.velocity.x = state.player.dash_direction * PLAYER_DASH_SPEED;
        state.player.velocity.y = 0.0f;

        state.player.dash_timer -= state.time.delta;

        bool has_dash_ended = state.player.dash_timer <= 0.0f;
        if (has_dash_ended)
        {
            state.player.state = PlayerState_Normal;
            state.player.velocity.x = state.player.dash_direction * PLAYER_MAX_SPEED;

            state.player.dash_timer = PLAYER_DASH_COOLDOWN;
        }
    }
    break;
    case PlayerState_Slam:
    {
        state.player.velocity.y = PLAYER_SLAM_SPEED;

        if (player::IsOnGround(state))
        {
            state.player.state = PlayerState_Normal;
        }
    }
    break;

    case PlayerState_Hook:
    {
        if (!state.input.hook.is_down)
        {
            state.player.state = PlayerState_Normal;
            state.player.hook_cooldown = 0.25f;
            break;
        }

        if (state.input.jump.pressed)
        {
            state.player.state = PlayerState_Normal;
            state.player.hook_cooldown = 0.25f;
            state.player.velocity.y = Min(state.player.velocity.y, -PLAYER_JUMP_SPEED);
            break;
        }

        Float horizontal_input = (Float)state.input.right.is_down - (Float)state.input.left.is_down;

        Float distance = GetDistance(state.player.position, state.player.hook_target);
        Vector2 offset = state.player.position - state.player.hook_target;

        if (distance > 0.0f)
        {
            Vector2 normal = Normalize(offset, distance);
            Vector2 tangent = V2(normal.y, -normal.x);

            Vector2 force_from_world = V2(horizontal_input * PLAYER_HOOK_FORCE, 0.0f);
            Float force_along_tangent = force_from_world.Dot(tangent);

            state.player.velocity += tangent * force_along_tangent * state.time.delta;
        }

        state.player.velocity.y += PLAYER_GRAVITY * state.time.delta;
    }
    break;
    }
    // NOTE: physics and collision

    Vector2 move = V2(state.player.velocity.x * state.time.delta, state.player.velocity.y * state.time.delta);
    Float position_before_moving_y = state.player.position.y;

    map::MoveAndCollide(state, state.player.position, V2(PLAYER_SIZE, PLAYER_SIZE), move);

    if (state.player.position.y == position_before_moving_y && move.y != 0.0f)
    {
        state.player.velocity.y = 0.0f;
    }

    if (state.player.state == PlayerState_Hook)
    {
        player::UpdateSwing(state);
    }
}

//

static void DrawPlayer(State &state, RenderCommandBuffer &buffer)
{
    Rectangle box = Rectangle::GetCentered(state.player.position, V2(PLAYER_SIZE, PLAYER_SIZE));
    buffer.DrawRectangle(Rectangle::FromVectors(state.camera.WorldToScreen(box.position), box.size), RED);

    if (state.player.state == PlayerState_Hook)
    {
        buffer.DrawLine(state.camera.WorldToScreen(state.player.position), state.camera.WorldToScreen(state.player.hook_target), WHITE);
    }
}

//
// NOTE: enemies
//

#define ENEMY_SPEED 120.0f
#define ENEMY_SIZE 30.0f

enum EnemyType
{
    ENEMY_TYPE_CHASER,
};

struct Enemy
{
    Vector2 position;
    EnemyType type;
};

namespace enemy
{

static Enemy enemies[] = {
    {.position = V2(200.0f, 200.0f), .type = ENEMY_TYPE_CHASER},
    {.position = V2(900.0f, 500.0f), .type = ENEMY_TYPE_CHASER},
};
static Int32 enemy_count = SDL_arraysize(enemies);

static void UpdateChaser(State &state, Enemy *enemy)
{
    Float distance = GetDistance(state.player.position, enemy->position);

    Vector2 offset = state.player.position - enemy->position;
    if (distance == 0.0f)
    {
        return;
    }

    Vector2 direction = Normalize(offset, distance);
    Vector2 move = V2(direction.x * ENEMY_SPEED * state.time.delta, direction.y * ENEMY_SPEED * state.time.delta);

    map::MoveAndCollide(state, enemy->position, V2(ENEMY_SIZE, ENEMY_SIZE), move);
}

} // namespace enemy

static void UpdateEnemy(State &state, Enemy *enemies, Int32 count)
{
    for (Int32 i = 0; i < count; i++)
    {
        Enemy *enemy = &enemies[i];

        switch (enemy->type)
        {
        case ENEMY_TYPE_CHASER:
        {
            enemy::UpdateChaser(state, enemy);
        }
        break;
        default:
        {
        }
        break;
        }
    }
}

//

static void DrawEnemy(State &state, RenderCommandBuffer &buffer, Enemy *enemies, Int32 count)
{
    for (Int32 i = 0; i < count; i++)
    {
        Rectangle box = Rectangle::GetCentered(enemies[i].position, V2(ENEMY_SIZE, ENEMY_SIZE));
        buffer.DrawRectangle(Rectangle::FromVectors(state.camera.WorldToScreen(box.position), box.size), MAGENTA);
    }
}

State State::Initialize()
{
    State state = {};

    state.map = (Map){
        {
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1},
            {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1},
            {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        }};

    state.ok = true;

    return state;
}

void State::Draw(RenderCommandBuffer &buffer)
{
    buffer.ClearScreen(BLACK);

    DrawMap(*this, buffer);
    DrawPlayer(*this, buffer);
    DrawEnemy(*this, buffer, enemy::enemies, enemy::enemy_count);
}

void State::Update()
{
    UpdatePlayer(*this);
    UpdateEnemy(*this, enemy::enemies, enemy::enemy_count);

    Vector2 world = V2((Float)(MAP_WIDTH * MAP_TILE_SIZE), (Float)(MAP_HEIGHT * MAP_TILE_SIZE));
    UpdateCamera(this->camera, this->player.position, this->camera.viewport, world, this->time.delta);
}
