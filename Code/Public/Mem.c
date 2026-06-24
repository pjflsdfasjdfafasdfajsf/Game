#include "Mem.h"
#include "Types.h"

//
// NOTE: MemAlloc
//

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

Void *MemAllocPush(MemAlloc *MemAlloc, Uint32 Bytes, Uint32 Align)
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

Uint32 MemReadUint(const char **CurInit)
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
