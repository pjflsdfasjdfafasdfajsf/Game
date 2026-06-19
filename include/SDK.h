//
// NOTE: Primary API.
// 1. The API here in the first place is C-compatible, so do not expect any classes, operators, etc. This way you can create and use bindings
// for any language and use the correspodning language features. Though note that this C header also ships with C++ bindings.
// 2. Nothing is prefixed here (except for the cases when functions 'belong' to a structure) because it is not expected that you will use any
// big libraries for your mods. (Raylib for example, which has the same naming convention and does not prefix anything. You're weird if you
// are trying to use Raylib in your mods.)
//
#if !defined(PENGUIN_H)
#define PENGUIN_H

#include <stddef.h>
#include <stdint.h>

//
// NOTE: Constants.
//

// TODO: temporary
#define MAP_HEIGHT 9
#define MAP_WIDTH 30

//
// NOTE: Types.
//

#ifndef __cplusplus
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <stdbool.h>
#else
#ifndef bool
typedef int bool;
#define true 1
#define false 0
#endif
#endif
#endif

typedef uint8_t Uint8;

typedef uint32_t Uint32;
typedef int32_t Int32;

typedef uint64_t Uint64;

typedef float Float;

typedef uintptr_t Uintptr;
// NOTE: Do not use this for ANY of SDK stuff because it is different on x32/x64.
typedef size_t Usize;

//
// NOTE: Macros.
// 

#if defined(__cplusplus)
#define CPP 1
#endif

#if defined (_MSC_VER)

#define MSVC

#elif defined(__GNUC__)

#define GCC

#elif defined(__clang__)

#define CLANG

#endif

#if defined(MSVC)

#define Assert(Expression) if(!(Expression)) { __debugbreak(); }

#elif defined(GCC) || defined(CLANG)

#define Assert(Expression) if(!(Expression)) { __builtin_trap(); }

#else
#define Assert(Expression) if(!(Expression)) { *(volatile int *)0 = 0; }

#endif 

#define CUSTOM_INPUT_MAX_ACTIONS 256

//
// NOTE: Math.
//

#define Kilobytes(x) (x * 1024)

#define Min(a, b) (((a) < (b)) ? (a) : (b))
#define Max(a, b) (((a) > (b)) ? (a) : (b))
#define Clamp(value, minimum, maximum) Max((minimum), Min((value), (maximum)))

#define Abs(x) (((x) < 0) ? -(x) : (x))

#define SquareRoot(x) __builtin_sqrtf(x)
#define Floor(x) __builtin_floorf(x)
#define CopySign(magnitude, sign) __builtin_copysignf((magnitude), (sign))

#define SquareRoot(x) __builtin_sqrtf(x)

typedef struct Vector2 {
    union {
        struct {
            Float x, y;
        };

        struct {
            Float width, height;
        };
    };

#if defined(CPP)

    static inline Vector2 Zero();

    inline Vector2 operator+(Vector2 b) const;

    inline Vector2 operator-(Vector2 b) const;

    inline Vector2 operator*(Float scalar) const;

    inline Vector2 operator/(Float scalar) const;

    inline Float Dot(Vector2 b) const;

    // NOTE: No equivalent in C.
    inline Vector2 &operator+=(Vector2 b) {
        x += b.x;
        y += b.y;
        return *this;
    }

#endif // CPP

} Vector2;

static inline Vector2 V2(Float x, Float y) {
    return (Vector2){{{x, y}}};
}

static inline Vector2 V2_Zero() {
    return V2(0, 0);
}

static inline Vector2 V2_Sub(Vector2 a, Vector2 b) {
    return V2(a.x - b.x, a.y - b.y);
}

static inline Vector2 V2_Add(Vector2 a, Vector2 b) {
    return V2(a.x + b.x, a.y + b.y);
}

static inline Vector2 V2_Div(Vector2 vector, Float scalar) {
    return V2(vector.x / scalar, vector.y / scalar);
}

static inline Vector2 V2_Mul(Vector2 vector, Float scalar) {
    return V2(vector.x * scalar, vector.y * scalar);
}

static inline Float V2_Dot(Vector2 a, Vector2 b) {
    return (a.x * b.x) + (a.y * b.y);
}

#if defined(CPP)

inline Vector2 Vector2::Zero() {
    return V2_Zero();
}

inline Vector2 Vector2::operator+(Vector2 b) const {
    return V2_Add(*this, b);
}

inline Vector2 Vector2::operator-(Vector2 b) const {
    return V2_Sub(*this, b);
}

inline Vector2 Vector2::operator*(Float scalar) const {
    return V2_Mul(*this, scalar);
}

inline Vector2 Vector2::operator/(Float scalar) const {
    return V2_Div(*this, scalar);
}

inline Float Vector2::Dot(Vector2 b) const {
    return V2_Dot(*this, b);
}

#endif // CPP

typedef struct Vector2i {
    union {
        struct {
            Int32 x, y;
        };

        struct {
            Int32 width, height;
        };
    };

#if defined(CPP)

    static inline Vector2i Zero();

#endif // CPP
} Vector2i;

static inline Vector2i V2i(Int32 x, Int32 y) {
    return (Vector2i){{{x, y}}};
}

static inline Vector2i V2i_Zero() {
    return V2i(0, 0);
}

#if defined(CPP)

inline Vector2i Vector2i::Zero() {
    return V2i_Zero();
}

#endif // CPP


typedef struct RGBA {
    Uint8 red, green, blue, alpha;
} RGBA;

/// RGBA with Alpha set to 255.
static inline RGBA Color3(Uint8 red, Uint8 green, Uint8 blue) {
    return (RGBA){red, green, blue, 255};
}

static inline RGBA Color4(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha) {
    return (RGBA){red, green, blue, alpha};
}

#if defined(CPP)

static inline RGBA Color(Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha = 255) {
    return (RGBA){red, green, blue, alpha};
}

#endif // CPP

typedef struct Vector4 {
    union {
        struct {
            Float x, y, z, w;
        };

        struct {
            Float red, green, blue, alpha;
        };
    };
} Vector4;

static inline Vector4 V4(Float x, Float y, Float z, Float w) {
    return (Vector4){{{x, y, z, w}}};
}

// NOTE: Color definitions.

#define WHITE Color3(255, 255, 255)
#define BLACK Color3(0, 0, 0)
#define RED Color3(255, 0, 0)
#define MAGENTA Color3(255, 0, 255)

// NOTE: Rectangle.

typedef struct Rectangle {
    Vector2 position;
    Vector2 size;

#if defined(CPP)

    static inline Rectangle FromFloats(Float x, Float y, Float width, Float height);

    static inline Rectangle FromVectors(Vector2 position, Vector2 size);

    static inline Rectangle GetCentered(Vector2 position, Vector2 size);

#endif // CPP
} Rectangle;

static inline Rectangle Rectangle_FromFloats(Float x, Float y, Float width, Float height) {
    return (Rectangle){V2(x, y), V2(width, height)};
}

static inline Rectangle Rectangle_FromVectors(Vector2 position, Vector2 size) {
    return Rectangle_FromFloats(position.x, position.y, size.width, size.height);
}

static inline Rectangle Rectangle_GetCentered(Vector2 position, Vector2 size) {
    return Rectangle_FromVectors(V2(position.x - (size.width * 0.5f), position.y - (size.height * 0.5f)), size);
}

#if defined(CPP)

inline Rectangle Rectangle::FromFloats(Float x, Float y, Float width, Float height) {
    return Rectangle_FromFloats(x, y, width, height);
}

inline Rectangle Rectangle::FromVectors(Vector2 position, Vector2 size) {
    return Rectangle_FromVectors(position, size);
}

inline Rectangle Rectangle::GetCentered(Vector2 position, Vector2 size) {
    return Rectangle_GetCentered(position, size);
}

#endif // CPP

// NOTE: Generic math.

/// Moves CURRENT towards TARGET at a maximum rate of MAXIMUM_DELTA.
static inline Float Approach(Float current, Float target, Float maximum_delta) {
    if (current < target) {
        return Min(current + maximum_delta, target);
    } else {
        return Max(current - maximum_delta, target);
    }
}

static inline Float GetDistanceSquared(Vector2 a, Vector2 b) {
    Vector2 offset = V2_Sub(a, b);
    return V2_Dot(offset, offset);
}

static inline Float GetDistance(Vector2 a, Vector2 b) {
    return SquareRoot(GetDistanceSquared(a, b));
}

static inline Vector2 Normalize(Vector2 vector, Float distance) {
    if (distance == 0.0f) {
        return V2(0.0f, 0.0f);
    }
    return V2_Div(vector, distance);
}

typedef struct Camera {
    /// Top-left of the view in world space.
    Vector2 position;
    Vector2i viewport;

#if defined(CPP)

    inline Vector2 WorldToScreen(Vector2 world) {
        return world - this->position;
    }

#endif // CPP
} Camera;

static inline Vector2 Camera_WorldToScreen(Camera camera, Vector2 world) {
    return V2_Sub(world, camera.position);
}

//
// NOTE: Rendering.
//

typedef enum RenderCommandType {
    RenderCommandType_None,
    RenderCommandType_ClearScreen,
    RenderCommandType_DrawRectangle,
    RenderCommandType_DrawLine,
} RenderCommandType;

typedef struct RenderCommand_ClearScreen {
    RenderCommandType type;
    RGBA color;
} RenderCommand_ClearScreen;

typedef struct RenderCommand_DrawRectangle {
    RenderCommandType type;
    Rectangle rectangle;
    RGBA color;
} RenderCommand_DrawRectangle;

typedef struct RenderCommand_DrawLine {
    RenderCommandType type;
    Vector2 start;
    Vector2 end;
    RGBA color;
} RenderCommand_DrawLine;

typedef struct RenderCommandBuffer {
    union {
        unsigned char *memory;
        // NOTE: Makes this structure same for x32 and x64.
        Uint64 padding;
    };
    Uint32 size;
    Uint32 capacity;

#if defined(CPP)

    inline void ClearScreen(RGBA color);

    inline void DrawRectangle(Rectangle rectangle, RGBA color);

    inline void DrawLine(Vector2 start, Vector2 end, RGBA color);

#endif // CPP
} RenderCommandBuffer;

/// Pushes raw bytes to command buffer. Prefer using helpers instead.
static inline void *RenderCommandBuffer_Push(RenderCommandBuffer *buffer, Uint32 memory_bytes_needed) {
    bool has_enough_space = (buffer->size + memory_bytes_needed) <= buffer->capacity;
    Assert(has_enough_space);

    if (has_enough_space) {
        void *address = buffer->memory + buffer->size;
        buffer->size += memory_bytes_needed;
        return address;
    }

    return 0;
}

static inline void RenderCommandBuffer_ClearScreen(RenderCommandBuffer *buffer, RGBA color) {
    RenderCommand_ClearScreen *command = (RenderCommand_ClearScreen *)RenderCommandBuffer_Push(buffer, sizeof(RenderCommand_ClearScreen));

    if (command) {
        command->type = RenderCommandType_ClearScreen;
        command->color = color;
    }
}

static inline void RenderCommandBuffer_DrawRectangle(RenderCommandBuffer *buffer, Rectangle rectangle, RGBA color) {
    RenderCommand_DrawRectangle *command = (RenderCommand_DrawRectangle *)RenderCommandBuffer_Push(buffer, sizeof(RenderCommand_DrawRectangle));

    if (command) {
        command->type = RenderCommandType_DrawRectangle;
        command->rectangle = rectangle;
        command->color = color;
    }
}

static inline void RenderCommandBuffer_DrawLine(RenderCommandBuffer *buffer, Vector2 start, Vector2 end, RGBA color) {
    RenderCommand_DrawLine *command = (RenderCommand_DrawLine *)RenderCommandBuffer_Push(buffer, sizeof(RenderCommand_DrawLine));

    if (command) {
        command->type = RenderCommandType_DrawLine;
        command->start = start;
        command->end = end;
        command->color = color;
    }
}

#if defined(CPP)

inline void RenderCommandBuffer::ClearScreen(RGBA color) {
    RenderCommandBuffer_ClearScreen(this, color);
}

inline void RenderCommandBuffer::DrawRectangle(Rectangle rectangle, RGBA color) {
    RenderCommandBuffer_DrawRectangle(this, rectangle, color);
}

inline void RenderCommandBuffer::DrawLine(Vector2 start, Vector2 end, RGBA color) {
    RenderCommandBuffer_DrawLine(this, start, end, color);
}

#endif // CPP

//
// NOTE: Game
//

typedef enum PlayerState {
    PLAYER_STATE_NORMAL,
    PLAYER_STATE_DASH,
    PLAYER_STATE_SLAM,
    PLAYER_STATE_HOOK,
} PlayerState;

typedef struct Player {
    Vector2 position;
    Vector2 velocity;
    PlayerState state;

    Float dash_timer;
    Float dash_direction;

    Vector2 hook_target;
    Float hook_rope_length;
    Float hook_cooldown;

    Float jump_buffer_timer;
} Player;

typedef struct Time {
    Float delta;
} Time;

typedef struct Map {
    int grid[MAP_HEIGHT][MAP_WIDTH];
} Map;

typedef struct Action {
    bool is_down;
    bool pressed;
    bool released;
} Action;

typedef struct Input {
    Action jump;
    Action dash;
    Action slam;
    Action hook;
    Action left;
    Action right;
    /// Mod-specific input requests.
    Action custom[CUSTOM_INPUT_MAX_ACTIONS];
} Input;

typedef struct State {
    Player player;
    Camera camera;
    Time time;
    Map map;
    Input input;

    bool ok;

#if defined(CPP) && defined(INTERNAL)

    static State Initialize();

    void Draw(RenderCommandBuffer &buffer);
    void Update();

#endif // CPP
} State;

#if defined(INTERNAL)

State State_Initialize();

void State_Draw(State *state, RenderCommandBuffer *buffer);
void State_Update(State *state);

#endif // INTERNAL

//
// NOTE: WASM
//

#if defined(__wasm__)

#define ENV "env"

#define IMPORT(name) __attribute__((import_module(ENV), import_name(name)))
#define EXPORT(name) __attribute__((export_name(name)))

#define Initialize EXPORT("guest_initialize") void _Guest_Initialize(void)
#define Update                                                           \
    void _Guest_Update_INNER(State *state, RenderCommandBuffer *buffer); \
    EXPORT("guest_update")                                               \
    void _Guest_Update(void) {                                           \
        _Guest_Update_INNER(&_state, &_buffer);                          \
    }                                                                    \
    void _Guest_Update_INNER(State *state, RenderCommandBuffer *buffer)

// NOTE: There is an issue that WASM engine has no idea where anything is stored so this is solved by setting up the needed
// permanent local storage here. The getters are called only once and then are cached.

static State _state;
static unsigned char _buffer_backing_memory[Kilobytes(128)];
static RenderCommandBuffer _buffer = {_buffer_backing_memory, 0, sizeof(_buffer_backing_memory)};

EXPORT("guest_get_state")
State *_Guest_GetState() {
    return &_state;
}

EXPORT("guest_get_buffer")
RenderCommandBuffer *_Guest_GetBuffer() {
    return &_buffer;
}

IMPORT("host_log")
void Log(const char *message);

IMPORT("host_set_metadata")
void SetMetadata(const char *name, const char *version, const char *summary);

/// The returned index represents the index it was assigned to in Input.custom array.
/// It is recommended to prefix `name` with the name of the mod to avoid any conflicts. For example:
// `example_mod_do_a_cool_thing`
IMPORT("host_register_action")
Int32 RegisterAction(const char *name);

/// Register `action` with `RegisterAction` first before calling this.
IMPORT("host_register_default_key")
void RegisterDefaultKey(const char *action, const char *key);

#endif // __wasm__

#endif
