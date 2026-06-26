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

MemReader MemReaderInit(const Uint8 *Mem, Usize Size)
{
    MemReader Result = {};

    Result.Base = Mem;
    Result.Size = Size;
    Result.HasError = (Mem == 0);

    return Result;
}

Void MemReaderSeek(MemReader *R, Usize Pos)
{
    Assert(R);
    if (R->HasError)
    {
        return;
    }

    if (Pos > R->Size)
    {
        R->HasError = True;
        return;
    }

    R->Pos = Pos;

    R->BitBuf = 0;
    R->BitCount = 0;
}

Void MemReaderSkip(MemReader *R, Usize Bytes)
{
    Assert(R);
    if (R->HasError)
    {
        return;
    }

    if (R->Pos + Bytes > R->Size)
    {
        R->HasError = True;
        return;
    }

    R->Pos += Bytes;
}

Uint16 MemReaderReadU16LE(MemReader *R)
{
    Assert(R);
    if (R->HasError)
    {
        return 0;
    }

    if (R->Pos + 2 > R->Size)
    {
        R->HasError = True;
        return 0;
    }

    const Uint8 *Val = R->Base + R->Pos;
    Uint16 Result = (Uint16)Val[0] |
                    ((Uint16)Val[1] << 8);

    R->Pos += 2;
    return Result;
}

Uint32 MemReaderReadU32LE(MemReader *R)
{
    Assert(R);
    if (R->HasError)
    {
        return 0;
    }

    if (R->Pos + 4 > R->Size)
    {
        R->HasError = True;
        return 0;
    }

    const Uint8 *Val = R->Base + R->Pos;
    Uint32 Result = (Uint32)Val[0] |
                    ((Uint32)Val[1] << 8) |
                    ((Uint32)Val[2] << 16) |
                    ((Uint32)Val[3] << 24);

    R->Pos += 4;
    return Result;
}

const Uint8 *MemReaderReadBytes(MemReader *R, Usize Bytes)
{
    Assert(R);
    if (R->HasError)
    {
        return 0;
    }

    if (R->Pos + Bytes > R->Size)
    {
        R->HasError = True;
        return 0;
    }

    const Uint8 *Result = R->Base + R->Pos;
    R->Pos += Bytes;

    return Result;
}

Void MemReaderRefillBits(MemReader *R, Uint32 Num)
{
    Assert(R);
    Assert(Num <= 24);

    if (R->HasError)
    {
        return;
    }

    while (R->BitCount < Num)
    {
        if (R->Pos < R->Size)
        {
            R->BitBuf |= (Uint32)R->Base[R->Pos] << R->BitCount;
            R->Pos++;
            R->BitCount += 8;
        }
        else
        {
            R->HasError = True;
            break;
        }
    }
}

Uint32 MemReaderGetBits(MemReader *R, Uint32 Num)
{
    Assert(R);
    Assert(Num <= 24);

    if (R->HasError)
    {
        return 0;
    }

    MemReaderRefillBits(R, Num);
    if (R->HasError)
    {
        return 0;
    }

    Uint32 Mask = 0;
    if (Num > 0)
    {
        Mask = (1U << Num) - 1U;
    }

    Uint32 Value = R->BitBuf & Mask;

    R->BitBuf >>= Num;
    R->BitCount -= Num;

    return Value;
}

Uint32 MemReaderGetBitsBase(MemReader *R, Uint32 Num, Uint32 Base)
{
    Assert(R);

    if (R->HasError)
    {
        return 0;
    }

    Uint32 Extra = 0;
    if (Num > 0)
    {
        Extra = MemReaderGetBits(R, Num);
    }

    return Base + Extra;
}

Void MemReaderAlignToByteBoundary(MemReader *R)
{
    Assert(R);

    if (R->HasError)
    {
        return;
    }

    // NOTE: If there are unused whole bytes Pos must be rewind back so that
    // they aren't skipped when reading uncompressed bytes
    while (R->BitCount >= 8)
    {
        if (R->Pos > 0)
        {
            R->Pos--;
            R->BitCount -= 8;
        }
        else
        {
            R->HasError = True;
            return;
        }
    }

    R->BitBuf = 0;
    R->BitCount = 0;
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
