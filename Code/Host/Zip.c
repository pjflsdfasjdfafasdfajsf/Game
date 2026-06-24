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

    MemReader R = MemReaderInit(Mem, Size);

    for (Usize I = MaxOffset; I >= MinOffset; I--)
    {
        MemReaderSeek(&R, I);
        Uint32 Sig = MemReaderReadU32LE(&R);

        if (!R.HasError && Sig == SigEOCD)
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

static inline ZipEntry ParseCDEntry(MemReader *R)
{
    Assert(R);

    ZipEntry Result = {0};

    Uint32 Sig = MemReaderReadU32LE(R);
    if (Sig != SigCDHeader)
    {
        R->HasError = True;
        // NOTE: Corrupt CD
        return Result;
    }

    // NOTE: VersionMadeBy (2), VersionNeeded(2), BitFlag(2)
    MemReaderSkip(R, 6);
    Uint16 CompressionMethod = MemReaderReadU16LE(R);
    // NOTE: ModTime (2), ModDate(2)
    MemReaderSkip(R, 4);

    Uint32 Crc32 = MemReaderReadU32LE(R);
    Uint32 CompressedSize = MemReaderReadU32LE(R);
    Uint32 UncompressedSize = MemReaderReadU32LE(R);
    Uint16 NameLen = MemReaderReadU16LE(R);
    Uint16 ExtraLen = MemReaderReadU16LE(R);
    Uint16 CommentLen = MemReaderReadU16LE(R);
    // NOTE: DiskNumStart (2), IntFileAttr (2), ExtFileAttr (4)
    MemReaderSkip(R, 8);

    Uint32 LocalHeaderOffset = MemReaderReadU32LE(R);

    const Uint8 *NamePtr = MemReaderReadBytes(R, NameLen);
    MemReaderSkip(R, ExtraLen + CommentLen);

    if (R->HasError)
    {
        return Result;
    }

    Result.NamePtr = NamePtr;
    Result.NameLen = NameLen;
    Result.CompressionMethod = CompressionMethod;
    Result.CompressedSize = CompressedSize;
    Result.UncompressedSize = UncompressedSize;
    Result.LocalHeaderOffset = LocalHeaderOffset;
    Result.Crc32 = Crc32;
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

    MemReader R = MemReaderInit(Mem, Size);
    MemReaderSeek(&R, EOCDOffset + 4);

    Uint16 DiskNum = MemReaderReadU16LE(&R);
    Uint16 CdDiskNum = MemReaderReadU16LE(&R);
    Uint16 DiskEntries = MemReaderReadU16LE(&R);
    Uint16 TotalEntries = MemReaderReadU16LE(&R);
    Uint32 CdSize = MemReaderReadU32LE(&R);
    Uint32 CdOffset = MemReaderReadU32LE(&R);
    Uint16 CommentLen = MemReaderReadU16LE(&R);

    if (R.HasError)
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

    MemReader R = MemReaderInit(Zip->Mem, Zip->Size);
    MemReaderSeek(&R, Zip->CdOffset);

    for (Uint32 I = 0; I <= Index; I++)
    {
        Result = ParseCDEntry(&R);
        if (!Result.IsValid)
        {
            return Result;
        }

        if (I == Index)
        {
            Result.IsValid = True;
            return Result;
        }
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

    MemReader R = MemReaderInit(Zip->Mem, Zip->Size);
    MemReaderSeek(&R, Zip->CdOffset);

    for (Uint32 I = 0; I < Zip->Count; I++)
    {
        Result = ParseCDEntry(&R);
        if (!Result.IsValid)
        {
            return Result;
        }

        if (NameMatches(Result.NamePtr, Result.NameLen, File))
        {
            Result.IsValid = True;
            return Result;
        }
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

    MemReader R = MemReaderInit(Zip->Mem, Zip->Size);
    MemReaderSeek(&R, Ent->LocalHeaderOffset);

    Uint32 Sig = MemReaderReadU32LE(&R);
    if (Sig != SigLocalHeader)
    {
        // NOTE: Corrupt local header
        return False;
    }

    // NOTE: Version (2), Flag (2), Compression (2), ModTime (2), ModDate (2), CRC (4), CompressedSize (4), UncompressedSize (4)
    MemReaderSkip(&R, 22);

    Uint16 NameLen = MemReaderReadU16LE(&R);
    Uint16 ExtraLen = MemReaderReadU16LE(&R);
    if (R.HasError)
    {
        // NOTE: Corrupt local header
        return False;
    }

    MemReaderSkip(&R, NameLen + ExtraLen);
    const Uint8 *Payload = MemReaderReadBytes(&R, Ent->UncompressedSize);
    if (R.HasError)
    {
        // NOTE: OOB
        return False;
    }

    if (Buf && Ent->UncompressedSize > 0)
    {
        MemCopy(Buf, Payload, Ent->UncompressedSize);
    }

    return True;
}
