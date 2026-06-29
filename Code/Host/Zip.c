#include "Zip.h"
#include "Public/Mem.h"
#include "Public/Types.h"
#include "SDL.h"

// NOTE: End of central directory record signature
#define SigEOCD 0x06054b50U
// NOTE: Central directory file heder signature
#define SigCDHeader 0x02014b50U
// NOTE: Local file header signature
#define SigLocalHeader 0x04034b50U

// NOTE: Compression
#define CompressionStore 0
#define CompressionDeflate 8
#define CompressionDeflate64 9

//
// NOTE: DEFLATE
//

#define DeflateMaxBitLength 15
#define DeflateNumLiteralSymbols 288
#define DeflateNumDistanceSymbols 32
#define DeflateNumCodeLengthSymbols 19

#define DeflateEndOfBlockSymbol 256
#define DeflateFirstLengthSymbol 257
#define DeflateMaxLengthIndex 28
#define DeflateMaxDistIndex 29

#define DeflateHlitBits 5
#define DeflateHlitBase 257
#define DeflateHdistBits 5
#define DeflateHdistBase 1
#define DeflateHclenBits 4
#define DeflateHclenBase 4

#define DeflateMaxHlit 286
#define DeflateMaxHdist 30

#define DeflateCodeLengthBits 3

#define DeflateSymCopyPrev 16
#define DeflateSymRepeatZero310 17
#define DeflateSymRepeatZero11138 18

#define DeflateCopyPrevBits 2
#define DeflateCopyPrevBase 3
#define DeflateRepeatZero310Bits 3
#define DeflateRepeatZero310Base 3
#define DeflateRepeatZero11138Bits 7
#define DeflateRepeatZero11138Base 11

#define DeflateNumLengthTableEntries 30
#define DeflateNumDistTableEntries 30

#define DeflateFixedLtCount7Val 24
#define DeflateFixedLtCount8Val 152
#define DeflateFixedLtCount9Val 112
#define DeflateFixedLtMaxSymbol 285
#define DeflateFixedDtCount5Val 32
#define DeflateFixedDtMaxSymbol 29

#define DeflateUncompressedHeaderSize 4

typedef struct
{
    // NOTE: number of codes with a given length
    Uint16 Counts[DeflateMaxBitLength + 1];
    // NOTE: Sorted by code
    Uint16 Symbols[DeflateNumLiteralSymbols];
    // NOTE: Highest symbol index present in the tree
    Int32 MaxSym;
} DeflateTree;

static inline Int32 MemStreamDecodeSymbol(MemStream *S, const DeflateTree *T)
{
    Assert(S);
    Assert(T);

    if (S->HasError)
    {
        return -1;
    }

    Int32 Base = 0;
    Int32 Offs = 0;

    for (Int32 Len = 1; Len <= DeflateMaxBitLength; ++Len)
    {
        Uint32 Bit = MemStreamGetBits(S, 1);
        if (S->HasError)
        {
            LogCritical("Failed to read bit during Huffman symbol decoding.\n");
            return -1;
        }

        Offs = 2 * Offs + (Int32)Bit;

        if (Offs < (Int32)T->Counts[Len])
        {
            Int32 Index = Base + Offs;
            if (Index < 0 || Index >= DeflateNumLiteralSymbols)
            {
                S->HasError = True;
                LogCritical("Decoded symbol index is out of bounds.\n");
                return -1;
            }
            return (Int32)T->Symbols[Index];
        }

        Base += (Int32)T->Counts[Len];
        Offs -= (Int32)T->Counts[Len];
    }

    S->HasError = True;
    LogCritical("Code exceeded maximum bit length.\n");

    return -1;
}

static inline Bool BuildTree(DeflateTree *T, const Uint8 *Lengths, Uint32 Num)
{
    Assert(T);
    Assert(Lengths);
    Assert(Num <= DeflateNumLiteralSymbols);

    Uint16 Offs[DeflateMaxBitLength + 1];
    for (Uint32 I = 0; I < DeflateMaxBitLength + 1; ++I)
    {
        T->Counts[I] = 0;
        Offs[I] = 0;
    }
    for (Uint32 I = 0; I < DeflateNumLiteralSymbols; ++I)
    {
        T->Symbols[I] = 0;
    }
    T->MaxSym = -1;

    for (Uint32 I = 0; I < Num; ++I)
    {
        if (Lengths[I] > DeflateMaxBitLength)
        {
            LogCritical("Code length for symbol exceeds max limit.\n");
            return False;
        }

        if (Lengths[I] > 0)
        {
            T->MaxSym = (Int32)I;
            T->Counts[Lengths[I]]++;
        }
    }

    Uint32 Available = 1;
    Uint32 NumCodes = 0;
    for (Uint32 I = 0; I < DeflateMaxBitLength + 1; ++I)
    {
        Uint32 Used = T->Counts[I];
        if (Used > Available)
        {
            LogCritical("Over-subscribed Huffman tree error.\n");
            return False;
        }
        Available = 2 * (Available - Used);
        Offs[I] = (Uint16)NumCodes;
        NumCodes += Used;
    }

    if ((NumCodes > 1 && Available > 0) || (NumCodes == 1 && T->Counts[1] != 1))
    {
        LogCritical("Incomplete Huffman tree.\n");
        return False;
    }

    for (Uint32 I = 0; I < Num; ++I)
    {
        if (Lengths[I] > 0)
        {
            T->Symbols[Offs[Lengths[I]]++] = (Uint16)I;
        }
    }

    if (NumCodes == 1)
    {
        T->Counts[1] = 2;
        T->Symbols[1] = (Uint16)(T->MaxSym + 1);
    }

    return True;
}

static inline Void BuildFixedTrees(DeflateTree *Lt, DeflateTree *Dt, Bool IsDeflate64)
{
    Assert(Lt);
    Assert(Dt);

    for (Uint32 I = 0; I < DeflateMaxBitLength + 1; ++I)
    {
        Lt->Counts[I] = 0;
    }
    Lt->Counts[7] = DeflateFixedLtCount7Val;
    Lt->Counts[8] = DeflateFixedLtCount8Val;
    Lt->Counts[9] = DeflateFixedLtCount9Val;

    for (Uint32 I = 0; I < DeflateFixedLtCount7Val; ++I)
    {
        Lt->Symbols[I] = (Uint16)(DeflateEndOfBlockSymbol + I);
    }
    for (Uint32 I = 0; I < 144; ++I)
    {
        Lt->Symbols[DeflateFixedLtCount7Val + I] = (Uint16)I;
    }
    for (Uint32 I = 0; I < 8; ++I)
    {
        Lt->Symbols[DeflateFixedLtCount7Val + 144 + I] = (Uint16)(280 + I);
    }
    for (Uint32 I = 0; I < DeflateFixedLtCount9Val; ++I)
    {
        Lt->Symbols[DeflateFixedLtCount7Val + 144 + 8 + I] = (Uint16)(144 + I);
    }
    Lt->MaxSym = DeflateFixedLtMaxSymbol;

    for (Uint32 I = 0; I < DeflateMaxBitLength + 1; ++I)
    {
        Dt->Counts[I] = 0;
    }

    Dt->Counts[5] = IsDeflate64 ? 32 : DeflateFixedDtCount5Val;

    Uint32 FixedDtCount = IsDeflate64 ? 32 : DeflateFixedDtCount5Val;
    for (Uint32 I = 0; I < FixedDtCount; ++I)
    {
        Dt->Symbols[I] = (Uint16)I;
    }
    Dt->MaxSym = IsDeflate64 ? 31 : DeflateFixedDtMaxSymbol;
}

static inline Bool DecodeTrees(MemStream *S, DeflateTree *Lt, DeflateTree *Dt, Bool IsDeflate64)
{
    Assert(S);
    Assert(Lt);
    Assert(Dt);

    if (S->HasError)
    {
        return False;
    }

    Uint8 Lengths[DeflateNumLiteralSymbols + DeflateNumDistanceSymbols] = {0};

    static const Uint8 ClcIndex[DeflateNumCodeLengthSymbols] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

    Uint32 Hlit = MemStreamGetBits(S, DeflateHlitBits) + DeflateHlitBase;
    Uint32 Hdist = MemStreamGetBits(S, DeflateHdistBits) + DeflateHdistBase;
    Uint32 Hclen = MemStreamGetBits(S, DeflateHclenBits) + DeflateHclenBase;

    SDL_Log("Dynamic tree (HLIT=%u, HDIST=%u, HCLEN=%u)\n", Hlit, Hdist, Hclen);

    Uint32 MaxHdist = IsDeflate64 ? 32 : DeflateMaxHdist;

    if (Hlit > DeflateMaxHlit || Hdist > MaxHdist)
    {
        S->HasError = True;
        LogCritical("Dynamic tree header parameters exceed spec limits.\n");
        return False;
    }

    Uint8 CodeLengths[DeflateNumCodeLengthSymbols] = {0};

    for (Uint32 I = 0; I < Hclen; ++I)
    {
        Uint32 Clen = MemStreamGetBits(S, DeflateCodeLengthBits);
        CodeLengths[ClcIndex[I]] = (Uint8)Clen;
    }

    if (S->HasError)
    {
        LogCritical("Error reading code lengths for code length tree.\n");
        return False;
    }

    DeflateTree CodeTree;
    if (!BuildTree(&CodeTree, CodeLengths, DeflateNumCodeLengthSymbols))
    {
        S->HasError = True;
        LogCritical("Failed to build code length Huffman tree.\n");
        return False;
    }

    if (CodeTree.MaxSym == -1)
    {
        S->HasError = True;
        LogCritical("Code length tree is empty.\n");
        return False;
    }

    for (Uint32 Num = 0; Num < Hlit + Hdist;)
    {
        Int32 Sym = MemStreamDecodeSymbol(S, &CodeTree);
        if (S->HasError || Sym < 0 || Sym > CodeTree.MaxSym)
        {
            S->HasError = True;
            LogCritical("Failed to decode symbol.\n");
            return False;
        }

        Uint32 Length = 0;
        Uint32 FillValue = 0;

        if (Sym < DeflateSymCopyPrev)
        {
            FillValue = (Uint32)Sym;
            Length = 1;
        }
        else if (Sym == DeflateSymCopyPrev)
        {
            if (Num == 0)
            {
                S->HasError = True;
                LogCritical("Sym 16 (copy previous) encountered at start of tree length decoding.\n");
                return False;
            }
            FillValue = Lengths[Num - 1];
            Length = MemStreamGetBits(S, DeflateCopyPrevBits) + DeflateCopyPrevBase;
        }
        else if (Sym == DeflateSymRepeatZero310)
        {
            FillValue = 0;
            Length = MemStreamGetBits(S, DeflateRepeatZero310Bits) + DeflateRepeatZero310Base;
        }
        else if (Sym == DeflateSymRepeatZero11138)
        {
            FillValue = 0;
            Length = MemStreamGetBits(S, DeflateRepeatZero11138Bits) + DeflateRepeatZero11138Base;
        }
        else
        {
            S->HasError = True;
            LogCritical("Unexpected symbol decoded in dynamic tree loop.\n");
            return False;
        }

        if (S->HasError)
        {
            LogCritical("Error reading extra repeat bits.\n");
            return False;
        }

        if (Length > Hlit + Hdist - Num)
        {
            S->HasError = True;
            LogCritical("Repeat count overflows hlit + hdist boundary.\n");
            return False;
        }

        for (Uint32 I = 0; I < Length; ++I)
        {
            Lengths[Num++] = (Uint8)FillValue;
        }
    }

    if (Lengths[DeflateEndOfBlockSymbol] == 0)
    {
        S->HasError = True;
        LogCritical("End-of-block symbol (256) is missing in the dynamic literal/length code lengths.\n");
        return False;
    }

    if (!BuildTree(Lt, Lengths, Hlit))
    {
        S->HasError = True;
        LogCritical("Failed to build dynamic literal/length tree.\n");
        return False;
    }

    if (!BuildTree(Dt, Lengths + Hlit, Hdist))
    {
        S->HasError = True;
        LogCritical("Failed to build dynamic distance tree.\n");
        return False;
    }

    return True;
}

static inline Bool DecompressBlockData(MemStream *S, const DeflateTree *Lt, const DeflateTree *Dt, Uint8 *Buf, Usize BufSize, Usize *OutDestLen, Bool IsDeflate64)
{
    Assert(S);
    Assert(Lt);
    Assert(Dt);
    Assert(Buf);

    Uint8 LengthBits[DeflateNumLengthTableEntries] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 127};
    Uint16 LengthBase[DeflateNumLengthTableEntries] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0};

    Uint8 DistBits[32] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14};
    Uint16 DistBase[32] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 32769, 49153};

    if (IsDeflate64)
    {
        LengthBits[28] = 16;
        LengthBase[28] = 3;
    }

    Uint32 MaxDistIndex = IsDeflate64 ? 31 : DeflateMaxDistIndex;

    for (;;)
    {
        Int32 Sym = MemStreamDecodeSymbol(S, Lt);
        if (S->HasError || Sym < 0)
        {
            S->HasError = True;
            LogCritical("Failed to decode next literal/length symbol in block.\n");
            return False;
        }

        if (Sym < DeflateEndOfBlockSymbol)
        {
            if (*OutDestLen >= BufSize)
            {
                LogCritical("Decompression output buffer size limit reached.\n");
                return False;
            }
            Buf[*OutDestLen] = (Uint8)Sym;
            (*OutDestLen)++;
        }
        else
        {
            if (Sym == DeflateEndOfBlockSymbol)
            {
                return True;
            }

            if (Sym > Lt->MaxSym || Sym - DeflateFirstLengthSymbol > DeflateMaxLengthIndex || Dt->MaxSym == -1)
            {
                S->HasError = True;
                LogCritical("Invalid match length symbol decoded.\n");
                return False;
            }

            Uint32 SymIndex = (Uint32)(Sym - DeflateFirstLengthSymbol);
            Uint32 Length = MemStreamGetBitsBase(S, LengthBits[SymIndex], LengthBase[SymIndex]);
            Int32 Dist = MemStreamDecodeSymbol(S, Dt);

            if (S->HasError || Dist > Dt->MaxSym || Dist > (Int32)MaxDistIndex)
            {
                S->HasError = True;
                LogCritical("Invalid distance code decoded.\n");
                return False;
            }

            Uint32 Offs = MemStreamGetBitsBase(S, DistBits[Dist], DistBase[Dist]);

            if (Offs > *OutDestLen)
            {
                S->HasError = True;
                LogCritical("Offset refers to data before start of output buffer.\n");
                return False;
            }

            if (*OutDestLen + Length > BufSize)
            {
                LogCritical("Block data match copy exceeds buffer capacity.\n");
                return False;
            }

            for (Uint32 I = 0; I < Length; ++I)
            {
                Buf[*OutDestLen + I] = Buf[*OutDestLen + I - Offs];
            }
            *OutDestLen += Length;
        }
    }
}

static inline Bool DecompressUncompressedBlock(MemStream *S, Uint8 *Buf, Usize BufSize, Usize *OutDestLen)
{
    Assert(S);
    Assert(Buf);

    if (S->HasError)
    {
        return False;
    }

    MemStreamAlignToByteBoundary(S);
    if (S->HasError)
    {
        LogCritical("Failed to align reader to byte boundary.\n");
        return False;
    }

    if (S->Pos + DeflateUncompressedHeaderSize > S->Size)
    {
        S->HasError = True;
        LogCritical("Payload ended before reading uncompressed block length headers.\n");
        return False;
    }

    Uint16 Length = MemStreamReadU16LE(S);
    Uint16 InvLength = MemStreamReadU16LE(S);

    if (S->HasError)
    {
        LogCritical("Error reading length headers of uncompressed block.\n");
        return False;
    }

    if (Length != (Uint16)(~InvLength))
    {
        S->HasError = True;
        LogCritical("Uncompressed block length mismatch.\n");
        return False;
    }

    SDL_Log("Decompressed block of %u bytes\n", Length);

    if (S->Pos + Length > S->Size)
    {
        S->HasError = True;
        LogCritical("Uncompressed block length exceeds source stream size.\n");
        return False;
    }

    if (*OutDestLen + Length > BufSize)
    {
        LogCritical("Uncompressed block length overflows output buffer.\n");
        return False;
    }

    const Uint8 *Src = MemStreamReadBytes(S, Length);
    if (S->HasError || !Src)
    {
        LogCritical("Failed to read uncompressed block payload data.\n");
        return False;
    }

    MemCopy(Buf + *OutDestLen, Src, Length);
    *OutDestLen += Length;

    S->BitBuf = 0;
    S->BitCount = 0;

    return True;
}

static inline Bool DecompressDeflate(Uint8 *Buf, Usize BufSize, const Uint8 *Payload, Usize CompressedSize, Bool IsDeflate64)
{
    Assert(Buf);
    Assert(BufSize > 0);
    Assert(Payload);
    Assert(CompressedSize > 0);

    MemStream R = MemStreamInit(Payload, CompressedSize);

    Usize OutDestLen = 0;
    Uint32 BFinal = 0;

    do
    {
        BFinal = MemStreamGetBits(&R, 1);
        Uint32 BType = MemStreamGetBits(&R, 2);

        if (R.HasError)
        {
            LogCritical("Failed to decode block headers from stream.\n");
            return False;
        }

        SDL_Log("BType %u (Final=%u)\n", BType, BFinal);

        if (BType == 0)
        {
            if (!DecompressUncompressedBlock(&R, Buf, BufSize, &OutDestLen))
            {
                LogCritical("Failed to decompress uncompressed block.\n");
                return False;
            }
        }
        else if (BType == 1)
        {
            DeflateTree Lt;
            DeflateTree Dt;
            BuildFixedTrees(&Lt, &Dt, IsDeflate64);

            if (!DecompressBlockData(&R, &Lt, &Dt, Buf, BufSize, &OutDestLen, IsDeflate64))
            {
                LogCritical("Failed to decompress fixed Huffman block data.\n");
                return False;
            }
        }
        else if (BType == 2)
        {
            DeflateTree Lt;
            DeflateTree Dt;

            if (!DecodeTrees(&R, &Lt, &Dt, IsDeflate64))
            {
                LogCritical("Failed to parse dynamic Huffman trees.\n");
                return False;
            }
            if (!DecompressBlockData(&R, &Lt, &Dt, Buf, BufSize, &OutDestLen, IsDeflate64))
            {
                LogCritical("Failed to decompress dynamic Huffman block data.\n");
                return False;
            }
        }
        else
        {
            LogCritical("Invalid block type BTYPE=3 encountered in stream.\n");
            return False;
        }

    } while (!BFinal);

    return True;
}

//
// NOTE: Internal
//

static inline Bool NameMatches(const Uint8 *NamePtr, Uint16 NameLen, const char *File)
{
    Assert(NamePtr);
    Assert(File);

    Usize TargetLen = CStrLen(File);
    if (NameLen < TargetLen)
    {
        return False;
    }

    const Uint8 *StartPtr = NamePtr + NameLen - TargetLen;
    if (!MemEql(StartPtr, File, TargetLen))
    {
        return False;
    }

    if (NameLen == TargetLen)
    {
        return True;
    }

    Uint8 PrevChar = *(StartPtr - 1);
    return (PrevChar == '/' || PrevChar == '\\');
}

static inline Bool FindEOCD(const Uint8 *Mem, Usize Size, Usize *OutEOCD)
{
    Assert(Mem);
    Assert(Size);

    // NOTE: Smallest possible EOCD has 22 bytes and max comment field is
    // 65535 bytes
    if (Size < 22)
    {
        LogCritical("Corrupted memory.\n");
        return False;
    }

    Usize SearchLimit = 65535 + 22;
    Usize MinOffset = (Size > SearchLimit) ? (Size - SearchLimit) : 0;
    Usize MaxOffset = Size - 22;

    MemStream R = MemStreamInit(Mem, Size);

    for (Usize I = MaxOffset; I >= MinOffset; I--)
    {
        MemStreamSeek(&R, I);
        Uint32 Sig = MemStreamReadU32LE(&R);

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

    LogCritical("End of central directory signature not found.\n");
    return False;
}

static inline ZipEntry ParseCDEntry(MemStream *S)
{
    Assert(S);

    ZipEntry Result = {0};

    Uint32 Sig = MemStreamReadU32LE(S);
    if (Sig != SigCDHeader)
    {
        S->HasError = True;
        LogCritical("Central directory header signature mismatch.\n");
        return Result;
    }

    // NOTE: VersionMadeBy (2), VersionNeeded(2), BitFlag(2)
    MemStreamSkip(S, 6);
    Uint16 CompressionMethod = MemStreamReadU16LE(S);
    // NOTE: ModTime (2), ModDate(2)
    MemStreamSkip(S, 4);

    Uint32 Crc32 = MemStreamReadU32LE(S);
    Uint32 CompressedSize = MemStreamReadU32LE(S);
    Uint32 UncompressedSize = MemStreamReadU32LE(S);
    Uint16 NameLen = MemStreamReadU16LE(S);
    Uint16 ExtraLen = MemStreamReadU16LE(S);
    Uint16 CommentLen = MemStreamReadU16LE(S);
    // NOTE: DiskNumStart (2), IntFileAttr (2), ExtFileAttr (4)
    MemStreamSkip(S, 8);

    Uint32 LocalHeaderOffset = MemStreamReadU32LE(S);

    const Uint8 *NamePtr = MemStreamReadBytes(S, NameLen);
    MemStreamSkip(S, ExtraLen + CommentLen);

    if (S->HasError)
    {
        LogCritical("Failed to parse Central Directory entry fields.\n");
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

//
// NOTE: Implementation
//

ZipArchive ZipOpen(const Uint8 *Mem, Usize Size)
{
    Assert(Mem);

    ZipArchive Result = {0};

    Usize EOCDOffset = 0;
    if (!FindEOCD(Mem, Size, &EOCDOffset))
    {
        return Result;
    }

    MemStream R = MemStreamInit(Mem, Size);
    MemStreamSeek(&R, EOCDOffset + 4);

    Uint16 DiskNum = MemStreamReadU16LE(&R);
    Uint16 CdDiskNum = MemStreamReadU16LE(&R);
    Uint16 DiskEntries = MemStreamReadU16LE(&R);
    Uint16 TotalEntries = MemStreamReadU16LE(&R);
    Uint32 CdSize = MemStreamReadU32LE(&R);
    Uint32 CdOffset = MemStreamReadU32LE(&R);
    Uint16 CommentLen = MemStreamReadU16LE(&R);

    if (R.HasError)
    {
        LogCritical("Failed to read EOCD record.\n");
        return Result;
    }

    if (DiskNum != 0 || CdDiskNum != 0 || DiskEntries != TotalEntries)
    {
        LogCritical("Disk mismatch or multi-disk ZIP archive is not supported.\n");
        return Result;
    }

    if (EOCDOffset + 22 + CommentLen > Size)
    {
        LogCritical("EOCD comment length overflows file boundaries.\n");
        return Result;
    }

    if ((Usize)CdOffset + CdSize > Size)
    {
        LogCritical("Central Directory offset and size overflow file boundaries.\n");
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
        LogCritical("Out of bounds access index.\n");
        return Result;
    }

    MemStream R = MemStreamInit(Zip->Mem, Zip->Size);
    MemStreamSeek(&R, Zip->CdOffset);

    for (Uint32 I = 0; I <= Index; I++)
    {
        Result = ParseCDEntry(&R);
        if (!Result.IsValid)
        {
            LogCritical("Failed to parse Central Directory entry during index search.\n");
            return Result;
        }

        if (I == Index)
        {
            Result.IsValid = True;
            return Result;
        }
    }
    LogCritical("Out of bounds access index.\n");
    // NOTE: Need to set this explicitly since it was set in the loop above so
    // it actually becomes invalid
    Result.IsValid = False;
    return Result;
};

ZipEntry ZipGetEntByName(const ZipArchive *Zip, const char *File)
{
    Assert(Zip);
    Assert(File);

    ZipEntry Result = {0};

    MemStream R = MemStreamInit(Zip->Mem, Zip->Size);
    MemStreamSeek(&R, Zip->CdOffset);

    for (Uint32 I = 0; I < Zip->Count; I++)
    {
        Result = ParseCDEntry(&R);
        if (!Result.IsValid)
        {
            LogCritical("Failed to parse Central Directory entry during name search.\n");
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
    LogCritical("Requested file entry not found.\n");
    return Result;
}

Bool ZipReadEnt(const ZipArchive *Zip, const ZipEntry *Ent, Uint8 *Buf, Usize BufSize)
{
    Assert(Zip);
    Assert(Ent);

    SDL_Log("Compression method: %u\n", Ent->CompressionMethod);
    if (Ent->CompressionMethod != CompressionStore &&
        Ent->CompressionMethod != CompressionDeflate &&
        Ent->CompressionMethod != CompressionDeflate64)
    {
        LogCritical("Unsupported compression method.\n");
        return False;
    }

    if (BufSize < Ent->UncompressedSize)
    {
        LogCritical("Buffer too small.\n");
        return False;
    }

    MemStream R = MemStreamInit(Zip->Mem, Zip->Size);
    MemStreamSeek(&R, Ent->LocalHeaderOffset);

    Uint32 Sig = MemStreamReadU32LE(&R);
    if (Sig != SigLocalHeader)
    {
        LogCritical("Local file header signature mismatch.\n");
        return False;
    }

    // NOTE: Version (2), Flag (2), Compression (2), ModTime (2), ModDate (2), CRC (4), CompressedSize (4), UncompressedSize (4)
    MemStreamSkip(&R, 22);

    Uint16 NameLen = MemStreamReadU16LE(&R);
    Uint16 ExtraLen = MemStreamReadU16LE(&R);
    if (R.HasError)
    {
        LogCritical("Failed to read header size fields from local header.\n");
        return False;
    }

    MemStreamSkip(&R, NameLen + ExtraLen);

    if (Ent->CompressionMethod == CompressionStore)
    {
        // NOTE: No compression
        const Uint8 *Payload = MemStreamReadBytes(&R, Ent->UncompressedSize);
        if (R.HasError)
        {
            LogCritical("Uncompressed store payload read out of bounds.\n");
            return False;
        }

        if (Buf && Ent->UncompressedSize > 0)
        {
            MemCopy(Buf, Payload, Ent->UncompressedSize);
        }
    }
    else
    {
        const Uint8 *Payload = MemStreamReadBytes(&R, Ent->CompressedSize);
        if (R.HasError)
        {
            LogCritical("Compressed payload read out of bounds.\n");
            return False;
        }

        Bool IsDeflate64 = (Ent->CompressionMethod == CompressionDeflate64);
        if (!DecompressDeflate(Buf, BufSize, Payload, Ent->CompressedSize, IsDeflate64))
        {
            LogCritical("Deflate decompression failed.\n");
            return False;
        }
    }

    return True;
}

Bool ZipEntEndsWith(const ZipEntry *Ent, const char *Suffix)
{
    Usize SuffixLen = SDL_strlen(Suffix);
    if (Ent->NameLen < SuffixLen)
    {
        return False;
    }

    return MemEql(Ent->NamePtr + Ent->NameLen - SuffixLen, Suffix, SuffixLen);
}
