#pragma once

#include "game_types.h"

#define DEFAULT_WINDOW_WIDTH 1280
#define DEFAULT_WINDOW_HEIGHT 720

#define Untextured 0

// NOTE: This vertex type must be shared between all renderer implementations.
typedef struct {
    f32 position[3];
    f32 color[4];
    f32 uv[2];
} Vertex;

// NOTE: Shared utilities.

static inline bool IsPowerOfTwo(usize value) {
    return (value != 0) && ((value & (value - 1)) == 0);
}

// NOTE: `alignment` must be power of two
static inline usize MemoryAlignSize(usize currentSize, usize alignment) {
    if (!IsPowerOfTwo(alignment)) {
        return currentSize;
    }

    usize alignmentMask = alignment - 1;
    usize alignedSize = (currentSize + alignmentMask) & ~alignmentMask;

    return alignedSize;
}

static inline void MemoryZero(void *destination, usize count) {
    char *bytePointer = (char *)destination;

    while (count--) {
        *bytePointer++ = 0;
    }
}

#define ZeroStruct(instance) MemoryZero(&(instance), sizeof(instance))
#define ZeroArray(array) MemoryZero((array), sizeof(array))
#define ZeroItems(pointer, count) MemoryZero((pointer), sizeof(*(pointer)) * (count))
#define ReturnZeroed(instance) \
    do {                       \
        ZeroStruct(instance);  \
        return (instance);     \
    } while (0)

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

static inline isize StringFindLastOccurrenceOfCharacter(const char *string, char targetCharacter) {
    isize lastFoundIndex = -1;
    usize currentIndex = 0;

    while (string[currentIndex] != '\0') {
        if (string[currentIndex] == targetCharacter) {
            lastFoundIndex = (isize)currentIndex;
        }
        currentIndex++;
    }

    return lastFoundIndex;
}

static inline void StringCopy(char *destination, usize destinationCapacity, const char *source) {
    if (!destination || !source || destinationCapacity == 0) {
        return;
    }

    usize sourceLength = StringGetLength(source);
    usize bytesToCopy = (sourceLength < (destinationCapacity - 1)) ? sourceLength : (destinationCapacity - 1);

    MemoryCopyForwards(destination, source, bytesToCopy);
    destination[bytesToCopy] = '\0';
}

static inline void StringAppend(char *destination, usize destinationCapacity, const char *source) {
    if (!destination || !source || destinationCapacity == 0) {
        return;
    }

    usize currentLength = StringGetLength(destination);
    usize remainingCapacity = destinationCapacity - currentLength;

    if (remainingCapacity > 0) {
        StringCopy(destination + currentLength, remainingCapacity, source);
    }
}

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

    usize alignedOffset = MemoryAlignSize(arena->currentOffset, alignment);

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
    MemoryArenaCheckpoint result = {0};

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

typedef struct {
    const u8 *memory;
    usize capacity;
    usize offset;

    bool hasOverflowed;
    bool isWritable;
} MemoryStream;

static inline void MemoryStreamInitializeReadOnly(MemoryStream *stream, const void *memory, usize capacity) {
    ZeroStruct(*stream);
    stream->memory = (const u8 *)memory;
    stream->capacity = capacity;
    stream->isWritable = false;
}

static inline void MemoryStreamInitializeWritable(MemoryStream *stream, void *memory, usize capacity) {
    ZeroStruct(*stream);
    stream->memory = (const u8 *)memory;
    stream->capacity = capacity;
    stream->isWritable = true;
}

static inline bool MemoryStreamHasSpace(MemoryStream *stream, usize readSize) {
    if (stream->hasOverflowed || !stream->memory || stream->offset + readSize > stream->capacity) {
        stream->hasOverflowed = true;

        return false;
    }

    return true;
}

static inline u8 MemoryStreamReadUInt8(MemoryStream *stream) {
    if (!MemoryStreamHasSpace(stream, 1)) {
        return 0;
    }
    u8 result = stream->memory[stream->offset];
    stream->offset += 1;

    return result;
}

static inline u16 MemoryStreamReadUInt16BigEndian(MemoryStream *stream) {
    if (!MemoryStreamHasSpace(stream, 2)) {
        return 0;
    }
    u16 result = ReadUInt16BigEndian(&stream->memory[stream->offset]);
    stream->offset += 2;

    return result;
}

static inline i16 MemoryStreamReadInt16BigEndian(MemoryStream *stream) {
    return (i16)MemoryStreamReadUInt16BigEndian(stream);
}

static inline u32 MemoryStreamReadUInt32BigEndian(MemoryStream *stream) {
    if (!MemoryStreamHasSpace(stream, 4)) {
        return 0;
    }
    u32 result = ReadUInt32BigEndian(&stream->memory[stream->offset]);
    stream->offset += 4;

    return result;
}

static inline i64 MemoryStreamReadInt64BigEndian(MemoryStream *stream) {
    if (!MemoryStreamHasSpace(stream, 8)) {
        return 0;
    }
    i64 result = ReadInt64BigEndian(&stream->memory[stream->offset]);
    stream->offset += 8;

    return result;
}

static inline bool MemoryStreamWriteBytes(MemoryStream *stream, const void *data, usize size) {
    if (!stream->isWritable) {
        stream->hasOverflowed = true;

        return false;
    }

    if (!MemoryStreamHasSpace(stream, size)) {
        return false;
    }

    u8 *writableMemory = (u8 *)stream->memory;
    MemoryCopyForwards((void *)(writableMemory + stream->offset), data, size);
    stream->offset += size;

    return true;
}

static inline bool MemoryStreamWriteUInt8(MemoryStream *stream, u8 value) {
    return MemoryStreamWriteBytes(stream, &value, 1);
}

static inline bool MemoryStreamWriteString(MemoryStream *stream, const char *string) {
    if (!string) {
        return false;
    }
    usize length = StringGetLength(string);
    return MemoryStreamWriteBytes(stream, string, length);
}

static inline bool MemoryStreamWriteLine(MemoryStream *stream, const char *string) {
    return MemoryStreamWriteString(stream, string) && MemoryStreamWriteUInt8(stream, '\n');
}

#define Stringify(x) #x
#define Stringify2(x) Stringify(x)

// --------------------------------------------------

// NOTE: Services that the platform provides to game.

typedef enum {
    RenderCommandType_None = 0,
    RenderCommandType_ClearEntireScreen,
    RenderCommandType_DrawRectangle,
    RenderCommandType_AllocateTexture,
} RenderCommandType;

typedef struct {
    u32 type;
    u32 size;
} RenderCommandHeader;

typedef struct {
    RenderCommandHeader header;
    Vector4 color;
} RenderCommandClearEntireScreen;

typedef struct {
    RenderCommandHeader header;
    Vector2 position;
    Vector2 size;
    Vector4 color;
    u32 texture;
} RenderCommandDrawRectangle;

typedef struct {
    RenderCommandHeader header;
    u32 index;
    u32 bytesPerPixel;
    Vector2U size;
    const void *pixels;
} RenderCommandAllocateTexture;

typedef struct {
    u8 *basePointer;
    usize totalCapacity;
    usize currentOffset;
    u32 commandCount;
    // NOTE: this becomes true if a command that did not fit was pushed in
    bool hasOverflowed;
} RenderCommandBuffer;

static inline void RenderCommandBufferInitialize(RenderCommandBuffer *commandBuffer, void *backingMemory, usize totalCapacity) {
    if (!commandBuffer) {
        return;
    }

    MemoryZero(commandBuffer, sizeof(RenderCommandBuffer));

    if (backingMemory && totalCapacity > 0) {
        commandBuffer->basePointer = (u8 *)backingMemory;
        commandBuffer->totalCapacity = totalCapacity;
    }
}

static inline void RenderCommandBufferReset(RenderCommandBuffer *commandBuffer) {
    if (!commandBuffer) {
        return;
    }

    commandBuffer->currentOffset = 0;
    commandBuffer->commandCount = 0;
    commandBuffer->hasOverflowed = false;
}

static inline void *RenderCommandBufferAllocateBytes(RenderCommandBuffer *commandBuffer, u32 type, usize requestedSize) {
    if (!commandBuffer || commandBuffer->hasOverflowed || !commandBuffer->basePointer || requestedSize < sizeof(RenderCommandHeader)) {
        return 0;
    }

    usize alignedSize = MemoryAlignSize(requestedSize, 8);

    if (commandBuffer->currentOffset + alignedSize > commandBuffer->totalCapacity) {
        commandBuffer->hasOverflowed = true;

        return 0;
    }

    void *allocatedMemory = (void *)(commandBuffer->basePointer + commandBuffer->currentOffset);
    MemoryZero(allocatedMemory, alignedSize);

    RenderCommandHeader *header = (RenderCommandHeader *)allocatedMemory;
    header->type = type;
    header->size = (u32)alignedSize;

    commandBuffer->currentOffset += alignedSize;
    commandBuffer->commandCount++;

    return allocatedMemory;
}

#define RenderCommandBufferPushCommand(buffer, commandName) (RenderCommand##commandName *)RenderCommandBufferAllocateBytes((buffer), RenderCommandType_##commandName, sizeof(RenderCommand##commandName))

// NOTE: Even though without Render prefix it looks a bit less pretty, with it you can easily see all render commands by just typing in 'Render'
// easily rather than guessing their names

static inline void RenderClearEntireScreen(RenderCommandBuffer *commandBuffer, Vector4 color) {
    if (!commandBuffer) {
        return;
    }

    RenderCommandClearEntireScreen *command = RenderCommandBufferPushCommand(commandBuffer, ClearEntireScreen);

    if (command) {
        command->color = color;
    }
}

static inline void RenderDrawRectangle(RenderCommandBuffer *commandBuffer, Vector2 position, Vector2 size, Vector4 color, u32 texture) {
    if (!commandBuffer) {
        return;
    }

    RenderCommandDrawRectangle *command = RenderCommandBufferPushCommand(commandBuffer, DrawRectangle);

    if (command) {
        command->position = position;
        command->size = size;
        command->color = color;
        command->texture = texture;
    }
}

static inline void RenderAllocateTexture(RenderCommandBuffer *commandBuffer, u32 index, Vector2U size, u32 bytesPerPixel, const void *pixels) {
    if (!commandBuffer) {
        return;
    }

    RenderCommandAllocateTexture *command = RenderCommandBufferPushCommand(commandBuffer, AllocateTexture);

    if (command) {
        command->index = index;
        command->size = size;
        command->bytesPerPixel = bytesPerPixel;
        command->pixels = pixels;
    }
}

// NOTE: Services that the game provides to platform.

typedef struct {
    // NOTE: These two standard streams are dumped to console by the platform.
    MemoryStream *standardErrorStream;
    MemoryStream *standardInfoStream;

    MemoryArena permanentArena;
    MemoryArena temporaryArena;
    
    bool isInitialized;
} GameMemory;

#define UPDATE_AND_RENDER(name) void name(GameMemory *memory, RenderCommandBuffer *commandBuffer)
typedef UPDATE_AND_RENDER(UpdateAndRenderFunction);

typedef struct {
    u32 samplesPerSecond;
    u32 channelCount;
    u32 frameCount;
    f32 *samples;
} AudioBuffer;

#define GET_SOUND_SAMPLES(name) void name(AudioBuffer *audioBuffer)
typedef GET_SOUND_SAMPLES(GetSoundSamplesFunc);

