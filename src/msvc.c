// NOTE: This file exists just to satisfy the linker

#include "game_types.h"

#ifdef _MSC_VER
int _fltused = 0;

void *memset(void *destination, int value, usize count);

#pragma intrinsic(memset)
#pragma function(memset)

void *memset(void *destination, int value, usize count) {
    char *bytePointer = (char *)destination;

    while (count--) {
        *bytePointer++ = (char)value;
    }

    return destination;
}
#endif
