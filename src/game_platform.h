#pragma once

// NOTE: This header does not require CRT/libc so we're fine.
#include <stdint.h>

// TODO: game_types.h once it becomes a problem

#define bool int
#define true 1
#define false 0

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

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

#define ABS(x) (((x) < 0) ? -(x) : (x))
#define MIN(x, y) ((x) > (y) ? (y) : (x))
#define MAX(x, y) ((x) < (y) ? (y) : (x))
#define UNUSED(x) (void)x

#define KILOBYTES(value) ((value) * 1024ULL)
#define MEGABYTES(value) (KILOBYTES(value) * 1024ULL)
#define GIGABYTES(value) (MEGABYTES(value) * 1024ULL)
#define TERABYTES(value) (GIGABYTES(value) * 1024ULL)

#define FOURCC(a, b, c, d) (((u32)(a) << 24) | ((u32)(b) << 16) | ((u32)(c) << 8) | ((u32)(d)))

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

// NOTE: Reading utilities.

static inline u16 ReadUInt16BigEndian(const u8 *memory) {
    return ((u16)memory[0] << 8) |
           ((u16)memory[1] << 0);
}

static inline i16 ReadInt16BigEndian(const u8 *memory) {
    return (i16)ReadUInt16BigEndian(memory);
}

static inline u32 ReadUInt32BigEndian(const u8 *memory) {
    return ((u32)memory[0] << 24) |
           ((u32)memory[1] << 16) |
           ((u32)memory[2] << 8) |
           ((u32)memory[3] << 0);
}

static inline u64 ReadUInt64BigEndian(const u8 *memory) {
    return ((u64)memory[0] << 56) |
           ((u64)memory[1] << 48) |
           ((u64)memory[2] << 40) |
           ((u64)memory[3] << 32) |
           ((u64)memory[4] << 24) |
           ((u64)memory[5] << 16) |
           ((u64)memory[6] << 8) |
           ((u64)memory[7] << 0);
}

static inline i64 ReadInt64BigEndian(const u8 *memory) {
    return (i64)ReadUInt64BigEndian(memory);
}

// NOTE: Memory arena.

typedef struct {
    u8 *basePointer;
    usize totalCapacity;
    usize currentOffset;
} MemoryArena;

typedef struct {
    MemoryArena *arena;
    usize savedOffset;
} MemoryArenaCheckpoint;

static inline void MemoryArenaInitialize(MemoryArena *arena, void *backingMemory, usize totalCapacity) {
    if (!arena) {
        return;
    }

    arena->basePointer = (u8 *)backingMemory;
    arena->totalCapacity = totalCapacity;
    arena->currentOffset = 0;
}

static inline usize MemoryArenaGetRemainingCapacity(const MemoryArena *arena) {
    if (!arena) {
        return 0;
    }

    return arena->totalCapacity - arena->currentOffset;
}

static inline void *MemoryArenaAllocateBytesAligned(MemoryArena *arena, usize allocationSize, usize alignment) {
    if (!arena || !arena->basePointer || allocationSize == 0 || alignment == 0) {
        return 0;
    }

    if ((alignment & (alignment - 1)) != 0) {
        return 0;
    }

    usize alignmentMask = alignment - 1;
    usize alignedOffset = (arena->currentOffset + alignmentMask) & ~alignmentMask;

    if (alignedOffset + allocationSize > arena->totalCapacity) {
        return 0;
    }

    void *allocatedMemory = (void *)(arena->basePointer + alignedOffset);
    arena->currentOffset = alignedOffset + allocationSize;

    return allocatedMemory;
}

static inline void *MemoryArenaAllocateBytes(MemoryArena *arena, usize allocationSize) {
    return MemoryArenaAllocateBytesAligned(arena, allocationSize, 8);
}

static inline void *MemoryArenaAllocateBytesAndZero(MemoryArena *arena, usize allocationSize) {
    void *allocatedMemory = MemoryArenaAllocateBytes(arena, allocationSize);
    
    if (allocatedMemory) {
        MemoryZero(allocatedMemory, allocationSize);
    }
    
    return allocatedMemory;
}

static inline void MemoryArenaClear(MemoryArena *arena) {
    if (arena) {
        arena->currentOffset = 0;
    }
}

static inline MemoryArenaCheckpoint MemoryArenaCreateCheckpoint(MemoryArena *arena) {
    MemoryArenaCheckpoint result;
    MemoryZero(&result, sizeof(result));

    if (arena) {
        result.arena = arena;
        result.savedOffset = arena->currentOffset;
    }

    return result;
}

static inline void MemoryArenaRestoreCheckpoint(MemoryArenaCheckpoint checkpoint) {
    if (checkpoint.arena) {
        checkpoint.arena->currentOffset = checkpoint.savedOffset;
    }
}

#define MemoryArenaPushArray(arena, type, count) (type *)MemoryArenaAllocateBytesAndZero((arena), sizeof(type) * (count))
#define MemoryArenaPushBytes(arena, size) MemoryArenaAllocateBytesAndZero((arena), (size))
