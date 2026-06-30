//
// NOTE: Memory utilities.
//
#if !defined(MEM_H)
#define MEM_H

#include "Types.h"

#define MemSize Mb(2)

//
// NOTE: Memory Reader
//

typedef struct
{
    Uint8 *Base;
    Usize Size;
    Usize Pos;

    Uint32 BitBuf;
    Uint32 BitCount;

    Bool HasError;
} Mem_Stream;

Mem_Stream Mem_Stream_Init(const Void *Mem, Usize Size);

Void Mem_Stream_Seek(Mem_Stream *S, Usize Pos);
Void MemStreamSkip(Mem_Stream *S, Usize Bytes);

// NOTE: Read functions.

Uint16 Mem_Stream_ReadU16LE(Mem_Stream *S);
Uint32 Mem_Stream_ReadU32LE(Mem_Stream *S);
const Uint8 *Mem_Stream_ReadBytes(Mem_Stream *S, Usize Bytes);

Void Mem_Stream_RefillBits(Mem_Stream *S, Uint32 Num);
Uint32 Mem_Stream_GetBits(Mem_Stream *S, Uint32 Num);
Uint32 MemS_tream_GetBitsBase(Mem_Stream *S, Uint32 Num, Uint32 Base);
Void Mem_Stream_AlignToByteBoundary(Mem_Stream *S);

// NOTE: Write functions.

Void Mem_Stream_WriteU8(Mem_Stream *S, Uint8 Val);
Void Mem_Stream_WriteU16LE(Mem_Stream *S, Uint16 Val);
Void Mem_Stream_WriteU32LE(Mem_Stream *S, Uint32 Val);
Void Mem_Stream_WriteBytes(Mem_Stream *S, const Void *Bytes, Usize Size);

Void Mem_Stream_WriteBits(Mem_Stream *S, Uint32 Val, Uint32 Num);
Void Mem_Stream_FlushBits(Mem_Stream *S);

//
// NOTE: String utilities
//

Uint32 Mem_CStrLen(const char *CStr);

//
// NOTE: Memory utilities
//

Void Mem_Copy(Void *DestInit, const Void *SrcInit, Usize Size);
Bool Mem_Eql(const Void *A, const Void *B, Usize Size);
Void Mem_NullTerminate(char *Buf, Usize Cap, Usize Len);

const char *Mem_FindChar(const char *Start, const char *End, char Target);
Uint32 Mem_ParseUint(const char **CurInit);

Void Mem_AdvanceToNextLine(const char **CurInit, const char *End);

//
// NOTE: Misc
//

// NOTE: From a path 'Foo/Bar/Baz.png' returns 'Baz.png'.
const char *Mem_GetBaseName(const char *Path, Uint32 Len);

#endif
