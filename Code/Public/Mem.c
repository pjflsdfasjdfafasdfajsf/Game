#include "Mem.h"
#include "Types.h"

//
// NOTE: MemAlloc
//

#define MemAllocDefaultAlign 4

MemAlloc MemAllocInit(void *Mem, Uint32 Cap)
{
    MemAlloc Result = {};

    Assert(Mem);
    Assert(Cap > 0);

    Result.Base = (Uint8 *)Mem;
    Result.Cap = Cap;
    Result.Used = 0;

    return Result;
}

Void *MemAllocPushEx(MemAlloc *MemAlloc, Uint32 Bytes, Uint32 Align)
{
    Assert(Bytes > 0);
    Assert(IsPow2(Align));

    Uint32 Offset = AlignUp(MemAlloc->Used, Align);
    Uint32 Req = Offset + Bytes;

    Bool HasSpace = Req <= MemAlloc->Cap;
    Assert(HasSpace);

    if (HasSpace)
    {
        Void *Result = MemAlloc->Base + Offset;
        MemAlloc->Used = Req;

        return Result;
    }

    return 0;
}

Void *MemAllocPush(MemAlloc *MemAlloc, Uint32 Bytes)
{
    return MemAllocPushEx(MemAlloc, Bytes, MemAllocDefaultAlign);
}

Void MemAllocClear(MemAlloc *MemAlloc)
{
    MemAlloc->Used = 0;
}

//
// NOTE: Memory Reader
//

MemStream MemStreamInit(const Void *Mem, Usize Size)
{
    MemStream Result = {};

    Result.Base = (Uint8 *)Mem;
    Result.Size = Size;
    Result.HasError = (Mem == 0);

    return Result;
}

Void MemStreamSeek(MemStream *S, Usize Pos)
{
    Assert(S);
    if (S->HasError)
    {
        return;
    }

    if (Pos > S->Size)
    {
        S->HasError = True;
        return;
    }

    S->Pos = Pos;

    S->BitBuf = 0;
    S->BitCount = 0;
}

Void MemStreamSkip(MemStream *S, Usize Bytes)
{
    Assert(S);
    if (S->HasError)
    {
        return;
    }

    if (S->Pos + Bytes > S->Size)
    {
        S->HasError = True;
        return;
    }

    S->Pos += Bytes;
}

Uint16 MemStreamReadU16LE(MemStream *S)
{
    Assert(S);
    if (S->HasError)
    {
        return 0;
    }

    if (S->Pos + 2 > S->Size)
    {
        S->HasError = True;
        return 0;
    }

    const Uint8 *Val = S->Base + S->Pos;
    Uint16 Result = (Uint16)Val[0] |
                    ((Uint16)Val[1] << 8);

    S->Pos += 2;
    return Result;
}

Uint32 MemStreamReadU32LE(MemStream *S)
{
    Assert(S);
    if (S->HasError)
    {
        return 0;
    }

    if (S->Pos + 4 > S->Size)
    {
        S->HasError = True;
        return 0;
    }

    const Uint8 *Val = S->Base + S->Pos;
    Uint32 Result = (Uint32)Val[0] |
                    ((Uint32)Val[1] << 8) |
                    ((Uint32)Val[2] << 16) |
                    ((Uint32)Val[3] << 24);

    S->Pos += 4;
    return Result;
}

const Uint8 *MemStreamReadBytes(MemStream *S, Usize Bytes)
{
    Assert(S);
    if (S->HasError)
    {
        return 0;
    }

    if (S->Pos + Bytes > S->Size)
    {
        S->HasError = True;
        return 0;
    }

    const Uint8 *Result = S->Base + S->Pos;
    S->Pos += Bytes;

    return Result;
}

Void MemStreamRefillBits(MemStream *S, Uint32 Num)
{
    Assert(S);
    Assert(Num <= 24);

    if (S->HasError)
    {
        return;
    }

    while (S->BitCount < Num)
    {
        if (S->Pos < S->Size)
        {
            S->BitBuf |= (Uint32)S->Base[S->Pos] << S->BitCount;
            S->Pos++;
            S->BitCount += 8;
        }
        else
        {
            S->HasError = True;
            break;
        }
    }
}

Uint32 MemStreamGetBits(MemStream *S, Uint32 Num)
{
    Assert(S);
    Assert(Num <= 24);

    if (S->HasError)
    {
        return 0;
    }

    MemStreamRefillBits(S, Num);
    if (S->HasError)
    {
        return 0;
    }

    Uint32 Mask = 0;
    if (Num > 0)
    {
        Mask = (1U << Num) - 1U;
    }

    Uint32 Value = S->BitBuf & Mask;

    S->BitBuf >>= Num;
    S->BitCount -= Num;

    return Value;
}

Uint32 MemStreamGetBitsBase(MemStream *S, Uint32 Num, Uint32 Base)
{
    Assert(S);

    if (S->HasError)
    {
        return 0;
    }

    Uint32 Extra = 0;
    if (Num > 0)
    {
        Extra = MemStreamGetBits(S, Num);
    }

    return Base + Extra;
}

Void MemStreamAlignToByteBoundary(MemStream *S)
{
    Assert(S);

    if (S->HasError)
    {
        return;
    }

    // NOTE: If there are unused whole bytes Pos must be rewind back so that
    // they aren't skipped when reading uncompressed bytes
    while (S->BitCount >= 8)
    {
        if (S->Pos > 0)
        {
            S->Pos--;
            S->BitCount -= 8;
        }
        else
        {
            S->HasError = True;
            return;
        }
    }

    S->BitBuf = 0;
    S->BitCount = 0;
}

Void MemStreamWriteU8(MemStream *S, Uint8 Val)
{
    Assert(S);
    if (S->HasError)
    {
        return;
    }

    if (S->Pos + 1 > S->Size)
    {
        S->HasError = True;
        return;
    }

    S->Base[S->Pos++] = Val;
}

Void MemStreamWriteU16LE(MemStream *S, Uint16 Val)
{
    Assert(S);
    if (S->HasError)
    {
        return;
    }

    if (S->Pos + 2 > S->Size)
    {
        S->HasError = True;
        return;
    }

    S->Base[S->Pos++] = (Uint8)(Val & 0xff);
    S->Base[S->Pos++] = (Uint8)((Val >> 8) & 0xff);
}

Void MemStreamWriteU32LE(MemStream *S, Uint32 Val)
{
    Assert(S);
    if (S->HasError)
    {
        return;
    }

    if (S->Pos + 4 > S->Size)
    {
        S->HasError = True;
        return;
    }

    S->Base[S->Pos++] = (Uint8)(Val & 0xff);
    S->Base[S->Pos++] = (Uint8)((Val >> 8) & 0xff);
    S->Base[S->Pos++] = (Uint8)((Val >> 16) & 0xff);
    S->Base[S->Pos++] = (Uint8)((Val >> 24) & 0xff);
}

Void MemStreamWriteBytes(MemStream *S, const Void *Bytes, Usize Size)
{
    Assert(S);
    if (S->HasError)
    {
        return;
    }

    if (Size == 0)
    {
        return;
    }

    Assert(Bytes);

    if (S->Pos + Size > S->Size)
    {
        S->HasError = True;
        return;
    }

    MemCopy(S->Base + S->Pos, Bytes, Size);
    S->Pos += Size;
}

Void MemStreamWriteBits(MemStream *S, Uint32 Val, Uint32 Num)
{
    Assert(S);
    if (S->HasError)
    {
        return;
    }

    S->BitBuf |= (Val & ((1U << Num) - 1)) << S->BitCount;
    S->BitCount += Num;

    while (S->BitCount >= 8)
    {
        if (S->Pos >= S->Size)
        {
            S->HasError = True;
            return;
        }
        S->Base[S->Pos++] = (Uint8)(S->BitBuf & 0xFF);
        S->BitBuf >>= 8;
        S->BitCount -= 8;
    }
}

Void MemStreamFlushBits(MemStream *S)
{
    Assert(S);
    if (S->HasError)
    {
        return;
    }

    if (S->BitCount > 0)
    {
        if (S->Pos >= S->Size)
        {
            S->HasError = True;
            return;
        }
        S->Base[S->Pos++] = (Uint8)(S->BitBuf & 0xFF);
        S->BitBuf = 0;
        S->BitCount = 0;
    }
}

//
// NOTE: String utilities
//

Uint32 CStrLen(const char *CStr)
{
    Assert(CStr);

    Uint32 Len = 0;
    while (CStr[Len])
    {
        Len++;
    }

    return Len;
}

//
// NOTE: Memory utilities
//

Void MemCopy(Void *DestInit, const Void *SrcInit, Usize Size)
{
    Assert(DestInit);
    Assert(SrcInit);

    if (Size == 0)
    {
        return;
    }

    Uint8 *Dest = (Uint8 *)DestInit;
    const Uint8 *Src = (const Uint8 *)SrcInit;

    for (Usize I = 0; I < Size; ++I)
    {
        Dest[I] = Src[I];
    }
}

Bool MemEql(const Void *A, const Void *B, Usize Size)
{
    const Uint8 *ByteA = (const Uint8 *)A;
    const Uint8 *ByteB = (const Uint8 *)B;
    for (Usize I = 0; I < Size; ++I)
    {
        if (ByteA[I] != ByteB[I])
        {
            return False;
        }
    }
    return True;
}

Void MemNullTerminate(char *Buf, Usize Cap, Usize Len)
{
    Assert(Buf);
    Assert(Cap > 0);

    if (Len < Cap)
    {
        Buf[Len] = '\0';
    }
    else
    {
        Buf[Cap - 1] = '\0';
    }
}

const char *MemFindChar(const char *Start, const char *End, char Target)
{
    Assert(Start);
    Assert(End >= Start);

    for (const char *Scan = Start; Scan < End; ++Scan)
    {
        if (*Scan == Target)
        {
            return Scan;
        }
    }
    return 0;
}

Uint32 MemParseUint(const char **CurInit)
{
    Uint32 Result = 0;

    const char *Cur = *CurInit;
    while (*Cur == ' ' || *Cur == '\t')
    {
        Cur++;
    }

    while (*Cur >= '0' && *Cur <= '9')
    {
        Result = Result * 10 + (*Cur - '0');
        Cur++;
    }

    *CurInit = Cur;
    return Result;
}

Void MemAdvanceToNextLine(const char **CurInit, const char *End)
{
    Assert(CurInit);
    Assert(*CurInit);
    Assert(End);

    const char *Cur = *CurInit;

    while (Cur < End && *Cur != '\n')
    {
        Cur++;
    }

    if (Cur < End && *Cur == '\n')
    {
        Cur++;
    }

    *CurInit = Cur;
}

//
// NOTE: Misc
//

// NOTE: From a path 'Foo/Bar/Baz.png' returns 'Baz.png'.
const char *GetBaseName(const char *Path, Uint32 Len)
{
    const char *Result = Path;

    for (Uint32 I = 0; I < Len; ++I)
    {
        if (Path[I] == '/' || Path[I] == '\\')
        {
            Result = Path + I + 1;
        }
    }

    return Result;
}
