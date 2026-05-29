#pragma once

// TODO: game_types.h once it becomes a problem

#define bool int
#define true 1
#define false 0

typedef size_t usize;

static inline void MemoryZero(void *destination, usize count) {
    char *bytePointer = (char *)destination;

    while (count--) {
        *bytePointer++ = 0;
    }
}

static inline usize StringGetLength(const char *string) {
    const char *characterPointer = string;

    while (*characterPointer) {
        characterPointer++;
    }

    return characterPointer - string;
}
