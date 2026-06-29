//
// NOTE: Memory utilities.
//
#if !defined(MEM_H)
#define MEM_H

#include "Types.h"

#define MemSize Mb(2)

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
    Uint8 *Base;
    Usize Size;
    Usize Pos;

    // NOTE: Not used yet
    Uint32 BitBuf;
    Uint32 BitCount;

    Bool HasError;
} MemStream;

MemStream MemStreamInit(const Void *Mem, Usize Size);

Void MemStreamSeek(MemStream *S, Usize Pos);
Void MemStreamSkip(MemStream *S, Usize Bytes);

// NOTE: Read functions.

Uint16 MemStreamReadU16LE(MemStream *S);
Uint32 MemStreamReadU32LE(MemStream *S);
const Uint8 *MemStreamReadBytes(MemStream *S, Usize Bytes);

Void MemStreamRefillBits(MemStream *S, Uint32 Num);
Uint32 MemStreamGetBits(MemStream *S, Uint32 Num);
Uint32 MemStreamGetBitsBase(MemStream *S, Uint32 Num, Uint32 Base);
Void MemStreamAlignToByteBoundary(MemStream *S);

// NOTE: Write functions.

Void MemStreamWriteU8(MemStream *S, Uint8 Val);
Void MemStreamWriteU16LE(MemStream *S, Uint16 Val);
Void MemStreamWriteU32LE(MemStream *S, Uint32 Val);
Void MemStreamWriteBytes(MemStream *S, const Void *Bytes, Usize Size);

Void MemStreamWriteBits(MemStream *S, Uint32 Val, Uint32 Num);
Void MemStreamFlushBits(MemStream *S);

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
