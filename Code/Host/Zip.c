#include "Zip.h"
#include "Mem.h"
#include "Types.h"

// NOTE: End of central directory record signature
#define SigEOCD 0x06054b50U
// NOTE: Central directory file heder signature
#define SigCDHeader 0x02014b50U
// NOTE: Local file header signature
#define SigLocalHeader 0x04034b50U

// NOTE: No compression
#define CompressionStore 0

// NOTE: These are currently used only here and don't really deserve their
// place in Mem.h. Maybe put them out later when we have multiple formats.

static inline Bool ReadU16LE(const Uint8 *Mem, Usize Size, Usize Offset, Uint16 *OutVal)
{
    Assert(Mem);
    Assert(OutVal);

    if (Offset + 2 > Size)
    {
        return False;
    }

    *OutVal = (Uint16)Mem[Offset] |
              ((Uint16)Mem[Offset + 1] << 8);
    return True;
}

static inline Bool ReadU32LE(const Uint8 *Mem, Usize Size, Usize Offset, Uint32 *OutVal)
{
    Assert(Mem);
    Assert(OutVal);

    if (Offset + 4 > Size)
    {
        return False;
    }

    *OutVal = (Uint32)Mem[Offset] |
              ((Uint32)Mem[Offset + 1] << 8) |
              ((Uint32)Mem[Offset + 2] << 16) |
              ((Uint32)Mem[Offset + 3] << 24);
    return True;
}

static inline Bool NameMatches(const Uint8 *NamePtr, Uint16 NameLen, const char *File)
{
    Assert(NamePtr);
    Assert(File);

    Usize TargetLen = 0;
    while (File[TargetLen] != '\0')
    {
        TargetLen++;
    }

    if (TargetLen != NameLen)
    {
        return False;
    }

    for (Uint16 I = 0; I < NameLen; I++)
    {
        if (NamePtr[I] != (Uint8)File[I])
        {
            return False;
        }
    }

    return True;
}

static inline Bool FindEOCD(const Uint8 *Mem, Usize Size, Usize *OutEOCD)
{
    Assert(Mem);
    Assert(Size);

    // NOTE: Smallest possible EOCD has 22 bytes and max comment field is
    // 65535 bytes
    if (Size < 22)
    {
        // NOTE: Too small
        return False;
    }

    Usize SearchLimit = 65535 + 22;
    Usize MinOffset = (Size > SearchLimit) ? (Size - SearchLimit) : 0;
    Usize MaxOffset = Size - 22;

    for (Usize I = MaxOffset; I >= MinOffset; I--)
    {
        Uint32 Sig;

        if (ReadU32LE(Mem, Size, I, &Sig) && Sig == SigEOCD)
        {
            *OutEOCD = I;

            return True;
        }
        if (I == 0)
        {
            break;
        }
    }

    // NOTE: Not found :(
    return False;
}

static inline ZipEntry ParseCDEntry(const ZipArchive *Zip, Usize Offset, Usize *OutNextOffset)
{
    Assert(Zip);
    Assert(OutNextOffset);

    ZipEntry Result = {0};

    const Uint8 *Mem = Zip->Mem;
    Usize Size = Zip->Size;

    Uint32 Sig;
    if (!ReadU32LE(Mem, Size, Offset, &Sig))
    {
        // NOTE: OOB
        return Result;
    }

    if (Sig != SigCDHeader)
    {
        // NOTE: Corrupt CD
        return Result;
    }

    Uint16 CompressionMethod;
    Uint32 Crc32;
    Uint32 CompressedSize;
    Uint32 UncompressedSize;
    Uint16 NameLen;
    Uint16 ExtraLen;
    Uint16 CommentLen;
    Uint32 LocalHeaderOffset;

    if (!ReadU16LE(Mem, Size, Offset + 10, &CompressionMethod) ||
        !ReadU32LE(Mem, Size, Offset + 16, &Crc32) ||
        !ReadU32LE(Mem, Size, Offset + 20, &CompressedSize) ||
        !ReadU32LE(Mem, Size, Offset + 24, &UncompressedSize) ||
        !ReadU16LE(Mem, Size, Offset + 28, &NameLen) ||
        !ReadU16LE(Mem, Size, Offset + 30, &ExtraLen) ||
        !ReadU16LE(Mem, Size, Offset + 32, &CommentLen) ||
        !ReadU32LE(Mem, Size, Offset + 42, &LocalHeaderOffset))
    {
        // NOTE: Corrupt CD
        return Result;
    }

    Usize VarDataOffset = Offset + 46;
    if (VarDataOffset + NameLen + ExtraLen + CommentLen > Size)
    {
        // NOTE: OOB
        return Result;
    }

    Result.NamePtr = Mem + VarDataOffset;
    Result.NameLen = NameLen;
    Result.CompressionMethod = CompressionMethod;
    Result.CompressedSize = CompressedSize;
    Result.UncompressedSize = UncompressedSize;
    Result.LocalHeaderOffset = LocalHeaderOffset;
    Result.Crc32 = Crc32;

    *OutNextOffset = VarDataOffset + NameLen + ExtraLen + CommentLen;

    Result.IsValid = True;
    return Result;
}

ZipArchive ZipOpen(const Uint8 *Mem, Usize Size)
{
    Assert(Mem);

    ZipArchive Result = {0};

    Usize EOCDOffset = 0;
    if (!FindEOCD(Mem, Size, &EOCDOffset))
    {
        return Result;
    }

    uint16_t DiskNum;
    uint16_t CdDiskNum;
    uint16_t DiskEntries;
    uint16_t TotalEntries;
    uint32_t CdSize;
    uint32_t CdOffset;
    uint16_t CommentLen;

    if (!ReadU16LE(Mem, Size, EOCDOffset + 4, &DiskNum) ||
        !ReadU16LE(Mem, Size, EOCDOffset + 6, &CdDiskNum) ||
        !ReadU16LE(Mem, Size, EOCDOffset + 8, &DiskEntries) ||
        !ReadU16LE(Mem, Size, EOCDOffset + 10, &TotalEntries) ||
        !ReadU32LE(Mem, Size, EOCDOffset + 12, &CdSize) ||
        !ReadU32LE(Mem, Size, EOCDOffset + 16, &CdOffset) ||
        !ReadU16LE(Mem, Size, EOCDOffset + 20, &CommentLen))
    {
        // NOTE: Corrupt EOCD
        return Result;
    }

    if (DiskNum != 0 || CdDiskNum != 0 || DiskEntries != TotalEntries)
    {
        // NOTE: Corrupt EOCD
        return Result;
    }

    if (EOCDOffset + 22 + CommentLen > Size)
    {
        // NOTE: Corrupt EOCD
        return Result;
    }

    if ((Usize)CdOffset + CdSize > Size)
    {
        // NOTE: Corrupt EOCD
        return Result;
    }

    Result.Mem = Mem;
    Result.Size = Size;
    Result.CdOffset = CdOffset;
    Result.CdSize = CdSize;
    Result.Count = TotalEntries;

    Result.IsValid = True;
    return Result;
}

ZipEntry ZipGetEntByIndex(const ZipArchive *Zip, Uint32 Index)
{
    Assert(Zip);

    ZipEntry Result = {0};

    if (Index >= Zip->Count)
    {
        // NOTE: OOB
        return Result;
    }

    Usize CurOffset = Zip->CdOffset;
    for (Uint32 I = 0; I <= Index; I++)
    {
        Usize NextOffset = 0;

        Result = ParseCDEntry(Zip, CurOffset, &NextOffset);
        if (!Result.IsValid)
        {
            return Result;
        }

        if (I == Index)
        {
            Result.IsValid = True;
            return Result;
        }

        CurOffset = NextOffset;
    }
    // NOTE: OOB
    // NOTE: Need to set this explicitly since it was set in the loop above so
    // it actually becomes invalid
    Result.IsValid = False;
    return Result;
}

ZipEntry ZipGetEntByName(const ZipArchive *Zip, const char *File)
{
    Assert(Zip);
    Assert(File);

    ZipEntry Result = {0};

    Usize CurOffset = Zip->CdOffset;
    for (Uint32 I = 0; I < Zip->Count; I++)
    {
        Usize NextOffset = 0;

        Result = ParseCDEntry(Zip, CurOffset, &NextOffset);
        if (!Result.IsValid)
        {
            return Result;
        }

        if (NameMatches(Result.NamePtr, Result.NameLen, File))
        {
            Result.IsValid = True;
            return Result;
        }

        CurOffset = NextOffset;
    }
    // NOTE: Not found
    // NOTE: Need to set this explicitly since it was set in the loop above so
    // it actually becomes invalid
    Result.IsValid = False;
    return Result;
}

Bool ZipReadEnt(const ZipArchive *Zip, const ZipEntry *Ent, Uint8 *Buf, Usize BufSize)
{
    Assert(Zip);
    Assert(Ent);

    if (Ent->CompressionMethod != CompressionStore)
    {
        // TODO: Support compression
        return False;
    }

    if (BufSize < Ent->UncompressedSize)
    {
        // NOTE: Buf to small
        return False;
    }

    const Uint8 *Mem = Zip->Mem;
    Usize Size = Zip->Size;
    Usize LFHOffset = Ent->LocalHeaderOffset;

    Uint32 Sig;
    if (!ReadU32LE(Mem, Size, LFHOffset, &Sig))
    {
        // NOTE: OOB
        return False;
    }

    if (Sig != SigLocalHeader)
    {
        // NOTE: Corrupt local header
        return False;
    }

    Uint16 NameLen;
    Uint16 ExtraLen;
    if (!ReadU16LE(Mem, Size, LFHOffset + 26, &NameLen) ||
        !ReadU16LE(Mem, Size, LFHOffset + 28, &ExtraLen))
    {
        // NOTE: Corrupt local header
        return False;
    }

    Usize PayloadOffset = LFHOffset + 30 + NameLen + ExtraLen;
    if (PayloadOffset + Ent->UncompressedSize > Size)
    {
        // NOTE: OOB
        return False;
    }

    if (Buf && Ent->UncompressedSize > 0)
    {
        MemCopy(Buf, Mem + PayloadOffset, Ent->UncompressedSize);
    }

    return True;
}
