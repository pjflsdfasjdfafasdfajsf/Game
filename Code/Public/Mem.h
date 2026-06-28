//
// NOTE: Memory utilities.
//
#if !defined(MEM_H)
#define MEM_H

#include "Types.h"

#define ExtraMemSize Mb(2)

// NOTE: This is a simple bump allocator.
typedef struct
{
    Uint8 *Base;
    Uint32 Cap;
    Uint32 Used;
} MemAlloc;

MemAlloc MemAllocInit(void *Mem, Uint32 Cap);

// NOTE: If you stumbled upont this accidentally -- use MemAllocPush. This
// function is for more complex usecases where you need alignment.
Void *MemAllocPushEx(MemAlloc *MemAlloc, Uint32 Bytes, Uint32 Align);
Void *MemAllocPush(MemAlloc *MemAlloc, Uint32 Bytes);

Void MemAllocClear(MemAlloc *MemAlloc);

//
// NOTE: Memory Reader
//

typedef struct
{
    const Uint8 *Base;
    Usize Size;
    Usize Pos;

    // NOTE: Not used yet
    Uint32 BitBuf;
    Uint32 BitCount;

    Bool HasError;
} MemReader;

MemReader MemReaderInit(const Uint8 *Mem, Usize Size);

Void MemReaderSeek(MemReader *R, Usize Pos);
Void MemReaderSkip(MemReader *R, Usize Bytes);

Uint16 MemReaderReadU16LE(MemReader *R);
Uint32 MemReaderReadU32LE(MemReader *R);
const Uint8 *MemReaderReadBytes(MemReader *R, Usize Bytes);

Void MemReaderRefillBits(MemReader *R, Uint32 Num);
Uint32 MemReaderGetBits(MemReader *R, Uint32 Num);
Uint32 MemReaderGetBitsBase(MemReader *R, Uint32 Num, Uint32 Base);
Void MemReaderAlignToByteBoundary(MemReader *R);

//
// NOTE: String utilities
//

Uint32 CStrLen(const char *CStr);

//
// NOTE: Memory utilities
//

Void MemCopy(Void *DestInit, const Void *SrcInit, Usize Size);
Bool MemEql(const Void *A, const Void *B, Usize Size);
Void MemNullTerminate(char *Buf, Usize Cap, Usize Len);

const char *MemFindChar(const char *Start, const char *End, char Target);
Uint32 MemParseUint(const char **CurInit);

Void MemAdvanceToNextLine(const char **CurInit, const char *End);

//
// NOTE: Misc
//

// NOTE: From a path 'Foo/Bar/Baz.png' returns 'Baz.png'.
const char *GetBaseName(const char *Path, Uint32 Len);

#endif
