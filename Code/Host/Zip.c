#include "Zip.h"
#include "Public/Mem.h"
#include "Public/Types.h"

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

static inline Int32 MemStreamDecodeSymbol(Mem_Stream *S, const DeflateTree *T)
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
        Uint32 Bit = Mem_Stream_GetBits(S, 1);
        if (S->HasError)
        {
            return -1;
        }

        Offs = 2 * Offs + (Int32)Bit;

        if (Offs < (Int32)T->Counts[Len])
        {
            Int32 Index = Base + Offs;
            if (Index < 0 || Index >= DeflateNumLiteralSymbols)
            {
                S->HasError = True;
                return -1;
            }
            return (Int32)T->Symbols[Index];
        }

        Base += (Int32)T->Counts[Len];
        Offs -= (Int32)T->Counts[Len];
    }

    S->HasError = True;
    return -1;
}

static inline Result BuildTree(DeflateTree *T, const Uint8 *Lengths, Uint32 Num)
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
            return Result_MakeError("Code length for symbol exceeds max limit");
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
            return Result_MakeError("Over-subscribed Huffman tree error");
        }
        Available = 2 * (Available - Used);
        Offs[I] = (Uint16)NumCodes;
        NumCodes += Used;
    }

    if ((NumCodes > 1 && Available > 0) || (NumCodes == 1 && T->Counts[1] != 1))
    {
        return Result_MakeError("Incomplete Huffman tree");
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

    return Result_Success();
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

static inline Result DecodeTrees(Mem_Stream *S, DeflateTree *Lt, DeflateTree *Dt, Bool IsDeflate64)
{
    Assert(S);
    Assert(Lt);
    Assert(Dt);

    if (S->HasError)
    {
        return Result_MakeError("Stream is in error state before decoding trees");
    }

    Uint8 Lengths[DeflateNumLiteralSymbols + DeflateNumDistanceSymbols] = {0};

    static const Uint8 ClcIndex[DeflateNumCodeLengthSymbols] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

    Uint32 Hlit = Mem_Stream_GetBits(S, DeflateHlitBits) + DeflateHlitBase;
    Uint32 Hdist = Mem_Stream_GetBits(S, DeflateHdistBits) + DeflateHdistBase;
    Uint32 Hclen = Mem_Stream_GetBits(S, DeflateHclenBits) + DeflateHclenBase;

    Uint32 MaxHdist = IsDeflate64 ? 32 : DeflateMaxHdist;

    if (Hlit > DeflateMaxHlit || Hdist > MaxHdist)
    {
        S->HasError = True;
        return Result_MakeError("Dynamic tree header parameters exceed spec limits");
    }

    Uint8 CodeLengths[DeflateNumCodeLengthSymbols] = {0};

    for (Uint32 I = 0; I < Hclen; ++I)
    {
        Uint32 Clen = Mem_Stream_GetBits(S, DeflateCodeLengthBits);
        CodeLengths[ClcIndex[I]] = (Uint8)Clen;
    }

    if (S->HasError)
    {
        return Result_MakeError("Error reading code lengths for code length tree");
    }

    DeflateTree CodeTree;
    Result Err = BuildTree(&CodeTree, CodeLengths, DeflateNumCodeLengthSymbols);
    if (Err.Error)
    {
        S->HasError = True;
        return Err;
    }

    if (CodeTree.MaxSym == -1)
    {
        S->HasError = True;
        return Result_MakeError("Code length tree is empty");
    }

    for (Uint32 Num = 0; Num < Hlit + Hdist;)
    {
        Int32 Sym = MemStreamDecodeSymbol(S, &CodeTree);
        if (S->HasError || Sym < 0 || Sym > CodeTree.MaxSym)
        {
            S->HasError = True;
            return Result_MakeError("Failed to decode symbol");
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
                return Result_MakeError("Sym 16 (copy previous) encountered at start of tree length decoding");
            }
            FillValue = Lengths[Num - 1];
            Length = Mem_Stream_GetBits(S, DeflateCopyPrevBits) + DeflateCopyPrevBase;
        }
        else if (Sym == DeflateSymRepeatZero310)
        {
            FillValue = 0;
            Length = Mem_Stream_GetBits(S, DeflateRepeatZero310Bits) + DeflateRepeatZero310Base;
        }
        else if (Sym == DeflateSymRepeatZero11138)
        {
            FillValue = 0;
            Length = Mem_Stream_GetBits(S, DeflateRepeatZero11138Bits) + DeflateRepeatZero11138Base;
        }
        else
        {
            S->HasError = True;
            return Result_MakeError("Unexpected symbol decoded in dynamic tree loop");
        }

        if (S->HasError)
        {
            return Result_MakeError("Error reading extra repeat bits");
        }

        if (Length > Hlit + Hdist - Num)
        {
            S->HasError = True;
            return Result_MakeError("Repeat count overflows hlit + hdist boundary");
        }

        for (Uint32 I = 0; I < Length; ++I)
        {
            Lengths[Num++] = (Uint8)FillValue;
        }
    }

    if (Lengths[DeflateEndOfBlockSymbol] == 0)
    {
        S->HasError = True;
        return Result_MakeError("End-of-block symbol (256) is missing in the dynamic literal/length code lengths");
    }

    Err = BuildTree(Lt, Lengths, Hlit);
    if (Err.Error)
    {
        S->HasError = True;
        return Err;
    }

    Err = BuildTree(Dt, Lengths + Hlit, Hdist);
    if (Err.Error)
    {
        S->HasError = True;
        return Err;
    }

    return Result_Success();
}

static inline Result DecompressBlockData(Mem_Stream *S, const DeflateTree *Lt, const DeflateTree *Dt, Uint8 *Buf, Usize BufSize, Usize *OutDestLen, Bool IsDeflate64)
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
            return Result_MakeError("Failed to decode next literal/length symbol in block");
        }

        if (Sym < DeflateEndOfBlockSymbol)
        {
            if (*OutDestLen >= BufSize)
            {
                return Result_MakeError("Decompression output buffer size limit reached");
            }
            Buf[*OutDestLen] = (Uint8)Sym;
            (*OutDestLen)++;
        }
        else
        {
            if (Sym == DeflateEndOfBlockSymbol)
            {
                return Result_Success();
            }

            if (Sym > Lt->MaxSym || Sym - DeflateFirstLengthSymbol > DeflateMaxLengthIndex || Dt->MaxSym == -1)
            {
                S->HasError = True;
                return Result_MakeError("Invalid match length symbol decoded");
            }

            Uint32 SymIndex = (Uint32)(Sym - DeflateFirstLengthSymbol);
            Uint32 Length = MemS_tream_GetBitsBase(S, LengthBits[SymIndex], LengthBase[SymIndex]);
            Int32 Dist = MemStreamDecodeSymbol(S, Dt);

            if (S->HasError || Dist > Dt->MaxSym || Dist > (Int32)MaxDistIndex)
            {
                S->HasError = True;
                return Result_MakeError("Invalid distance code decoded");
            }

            Uint32 Offs = MemS_tream_GetBitsBase(S, DistBits[Dist], DistBase[Dist]);

            if (Offs > *OutDestLen)
            {
                S->HasError = True;
                return Result_MakeError("Offset refers to data before start of output buffer");
            }

            if (*OutDestLen + Length > BufSize)
            {
                return Result_MakeError("Block data match copy exceeds buffer capacity");
            }

            for (Uint32 I = 0; I < Length; ++I)
            {
                Buf[*OutDestLen + I] = Buf[*OutDestLen + I - Offs];
            }
            *OutDestLen += Length;
        }
    }
}

static inline Result DecompressUncompressedBlock(Mem_Stream *S, Uint8 *Buf, Usize BufSize, Usize *OutDestLen)
{
    Assert(S);
    Assert(Buf);

    if (S->HasError)
    {
        return Result_MakeError("Stream is in error state before uncompressed block decompression");
    }

    Mem_Stream_AlignToByteBoundary(S);
    if (S->HasError)
    {
        return Result_MakeError("Failed to align reader to byte boundary");
    }

    if (S->Pos + DeflateUncompressedHeaderSize > S->Size)
    {
        S->HasError = True;
        return Result_MakeError("Payload ended before reading uncompressed block length headers");
    }

    Uint16 Length = Mem_Stream_ReadU16LE(S);
    Uint16 InvLength = Mem_Stream_ReadU16LE(S);

    if (S->HasError)
    {
        return Result_MakeError("Error reading length headers of uncompressed block");
    }

    if (Length != (Uint16)(~InvLength))
    {
        S->HasError = True;
        return Result_MakeError("Uncompressed block length mismatch");
    }

    if (S->Pos + Length > S->Size)
    {
        S->HasError = True;
        return Result_MakeError("Uncompressed block length exceeds source stream size");
    }

    if (*OutDestLen + Length > BufSize)
    {
        return Result_MakeError("Uncompressed block length overflows output buffer");
    }

    const Uint8 *Src = Mem_Stream_ReadBytes(S, Length);
    if (S->HasError || !Src)
    {
        return Result_MakeError("Failed to read uncompressed block payload data");
    }

    Mem_Copy(Buf + *OutDestLen, Src, Length);
    *OutDestLen += Length;

    S->BitBuf = 0;
    S->BitCount = 0;

    return Result_Success();
}

static inline Result DecompressDeflate(Uint8 *Buf, Usize BufSize, const Uint8 *Payload, Usize CompressedSize, Bool IsDeflate64)
{
    Assert(Buf);
    Assert(BufSize > 0);
    Assert(Payload);
    Assert(CompressedSize > 0);

    Mem_Stream R = Mem_Stream_Init(Payload, CompressedSize);

    Usize OutDestLen = 0;
    Uint32 BFinal = 0;

    do
    {
        BFinal = Mem_Stream_GetBits(&R, 1);
        Uint32 BType = Mem_Stream_GetBits(&R, 2);

        if (R.HasError)
        {
            return Result_MakeError("Failed to decode block headers from stream");
        }

        if (BType == 0)
        {
            Result Err = DecompressUncompressedBlock(&R, Buf, BufSize, &OutDestLen);
            if (Err.Error)
            {
                return Err;
            }
        }
        else if (BType == 1)
        {
            DeflateTree Lt;
            DeflateTree Dt;
            BuildFixedTrees(&Lt, &Dt, IsDeflate64);

            Result Err = DecompressBlockData(&R, &Lt, &Dt, Buf, BufSize, &OutDestLen, IsDeflate64);
            if (Err.Error)
            {
                return Err;
            }
        }
        else if (BType == 2)
        {
            DeflateTree Lt;
            DeflateTree Dt;

            Result Err = DecodeTrees(&R, &Lt, &Dt, IsDeflate64);
            if (Err.Error)
            {
                return Err;
            }
            Err = DecompressBlockData(&R, &Lt, &Dt, Buf, BufSize, &OutDestLen, IsDeflate64);
            if (Err.Error)
            {
                return Err;
            }
        }
        else
        {
            return Result_MakeError("Invalid block type BTYPE=3 encountered in stream");
        }

    } while (!BFinal);

    return Result_Success();
}

//
// NOTE: Internal
//

static inline Bool NameMatches(const Uint8 *NamePtr, Uint16 NameLen, const char *File)
{
    Assert(NamePtr);
    Assert(File);

    Usize TargetLen = Mem_CStrLen(File);
    if (NameLen < TargetLen)
    {
        return False;
    }

    const Uint8 *StartPtr = NamePtr + NameLen - TargetLen;
    if (!Mem_Eql(StartPtr, File, TargetLen))
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

static inline Result FindEOCD(const Uint8 *Mem, Usize Size, Usize *OutEOCD)
{
    Assert(Mem);
    Assert(Size);

    // NOTE: Smallest possible EOCD has 22 bytes and max comment field is
    // 65535 bytes
    if (Size < 22)
    {
        return Result_MakeError("Corrupted memory");
    }

    Usize SearchLimit = 65535 + 22;
    Usize MinOffset = (Size > SearchLimit) ? (Size - SearchLimit) : 0;
    Usize MaxOffset = Size - 22;

    Mem_Stream R = Mem_Stream_Init(Mem, Size);

    for (Usize I = MaxOffset; I >= MinOffset; I--)
    {
        Mem_Stream_Seek(&R, I);
        Uint32 Sig = Mem_Stream_ReadU32LE(&R);

        if (!R.HasError && Sig == SigEOCD)
        {
            *OutEOCD = I;

            return Result_Success();
        }
        if (I == 0)
        {
            break;
        }
    }

    return Result_MakeError("End of central directory signature not found");
}

static inline Result ParseCDEntry(Mem_Stream *S, Zip_Entry *OutEntry)
{
    Assert(S);
    Assert(OutEntry);

    Uint32 Sig = Mem_Stream_ReadU32LE(S);
    if (Sig != SigCDHeader)
    {
        S->HasError = True;
        return Result_MakeError("Central directory header signature mismatch");
    }

    // NOTE: VersionMadeBy (2), VersionNeeded(2), BitFlag(2)
    MemStreamSkip(S, 6);
    Uint16 CompressionMethod = Mem_Stream_ReadU16LE(S);
    // NOTE: ModTime (2), ModDate(2)
    MemStreamSkip(S, 4);

    Uint32 Crc32 = Mem_Stream_ReadU32LE(S);
    Uint32 CompressedSize = Mem_Stream_ReadU32LE(S);
    Uint32 UncompressedSize = Mem_Stream_ReadU32LE(S);
    Uint16 NameLen = Mem_Stream_ReadU16LE(S);
    Uint16 ExtraLen = Mem_Stream_ReadU16LE(S);
    Uint16 CommentLen = Mem_Stream_ReadU16LE(S);
    // NOTE: DiskNumStart (2), IntFileAttr (2), ExtFileAttr (4)
    MemStreamSkip(S, 8);

    Uint32 LocalHeaderOffset = Mem_Stream_ReadU32LE(S);

    const Uint8 *NamePtr = Mem_Stream_ReadBytes(S, NameLen);
    MemStreamSkip(S, ExtraLen + CommentLen);

    if (S->HasError)
    {
        return Result_MakeError("Failed to parse Central Directory entry fields");
    }

    OutEntry->NamePtr = NamePtr;
    OutEntry->NameLen = NameLen;
    OutEntry->CompressionMethod = CompressionMethod;
    OutEntry->CompressedSize = CompressedSize;
    OutEntry->UncompressedSize = UncompressedSize;
    OutEntry->LocalHeaderOffset = LocalHeaderOffset;
    OutEntry->Crc32 = Crc32;

    return Result_Success();
}

//
// NOTE: Implementation
//

Result Zip_Open(const Uint8 *Mem, Usize Size, Zip_Archive *OutZip)
{
    Assert(Mem);
    Assert(OutZip);

    Usize EOCDOffset = 0;
    Result Err = FindEOCD(Mem, Size, &EOCDOffset);
    if (Err.Error)
    {
        return Err;
    }

    Mem_Stream R = Mem_Stream_Init(Mem, Size);
    Mem_Stream_Seek(&R, EOCDOffset + 4);

    Uint16 DiskNum = Mem_Stream_ReadU16LE(&R);
    Uint16 CdDiskNum = Mem_Stream_ReadU16LE(&R);
    Uint16 DiskEntries = Mem_Stream_ReadU16LE(&R);
    Uint16 TotalEntries = Mem_Stream_ReadU16LE(&R);
    Uint32 CdSize = Mem_Stream_ReadU32LE(&R);
    Uint32 CdOffset = Mem_Stream_ReadU32LE(&R);
    Uint16 CommentLen = Mem_Stream_ReadU16LE(&R);

    if (R.HasError)
    {
        return Result_MakeError("Failed to read EOCD record");
    }

    if (DiskNum != 0 || CdDiskNum != 0 || DiskEntries != TotalEntries)
    {
        return Result_MakeError("Disk mismatch or multi-disk ZIP archive is not supported");
    }

    if (EOCDOffset + 22 + CommentLen > Size)
    {
        return Result_MakeError("EOCD comment length overflows file boundaries");
    }

    if ((Usize)CdOffset + CdSize > Size)
    {
        return Result_MakeError("Central Directory offset and size overflow file boundaries");
    }

    OutZip->Mem = Mem;
    OutZip->Size = Size;
    OutZip->CdOffset = CdOffset;
    OutZip->CdSize = CdSize;
    OutZip->Count = TotalEntries;

    return Result_Success();
}

Result Zip_Entry_GetByIndex(const Zip_Archive *Zip, Uint32 Index, Zip_Entry *OutEnt)
{
    Assert(Zip);
    Assert(OutEnt);

    if (Index >= Zip->Count)
    {
        return Result_MakeError("Out of bounds access index");
    }

    Mem_Stream R = Mem_Stream_Init(Zip->Mem, Zip->Size);
    Mem_Stream_Seek(&R, Zip->CdOffset);

    for (Uint32 I = 0; I <= Index; I++)
    {
        Result Err = ParseCDEntry(&R, OutEnt);
        if (Err.Error)
        {
            return Err;
        }

        if (I == Index)
        {
            return Result_Success();
        }
    }

    return Result_MakeError("Out of bounds access index");
}

Result Zip_Entry_GetByName(const Zip_Archive *Zip, const char *File, Zip_Entry *OutEnt)
{
    Assert(Zip);
    Assert(File);
    Assert(OutEnt);

    Mem_Stream R = Mem_Stream_Init(Zip->Mem, Zip->Size);
    Mem_Stream_Seek(&R, Zip->CdOffset);

    for (Uint32 I = 0; I < Zip->Count; I++)
    {
        Result Err = ParseCDEntry(&R, OutEnt);
        if (Err.Error)
        {
            return Err;
        }

        if (NameMatches(OutEnt->NamePtr, OutEnt->NameLen, File))
        {
            return Result_Success();
        }
    }

    return Result_MakeError("Requested file entry not found");
}

Result Zip_Entry_Read(const Zip_Archive *Zip, const Zip_Entry *Ent, Uint8 *Buf, Usize BufSize)
{
    Assert(Zip);
    Assert(Ent);

    if (Ent->CompressionMethod != CompressionStore &&
        Ent->CompressionMethod != CompressionDeflate &&
        Ent->CompressionMethod != CompressionDeflate64)
    {
        return Result_MakeError("Unsupported compression method");
    }

    if (BufSize < Ent->UncompressedSize)
    {
        return Result_MakeError("Buffer too small");
    }

    Mem_Stream R = Mem_Stream_Init(Zip->Mem, Zip->Size);
    Mem_Stream_Seek(&R, Ent->LocalHeaderOffset);

    Uint32 Sig = Mem_Stream_ReadU32LE(&R);
    if (Sig != SigLocalHeader)
    {
        return Result_MakeError("Local file header signature mismatch");
    }

    // NOTE: Version (2), Flag (2), Compression (2), ModTime (2), ModDate (2), CRC (4), CompressedSize (4), UncompressedSize (4)
    MemStreamSkip(&R, 22);

    Uint16 NameLen = Mem_Stream_ReadU16LE(&R);
    Uint16 ExtraLen = Mem_Stream_ReadU16LE(&R);
    if (R.HasError)
    {
        return Result_MakeError("Failed to read header size fields from local header");
    }

    MemStreamSkip(&R, NameLen + ExtraLen);

    if (Ent->CompressionMethod == CompressionStore)
    {
        // NOTE: No compression
        const Uint8 *Payload = Mem_Stream_ReadBytes(&R, Ent->UncompressedSize);
        if (R.HasError)
        {
            return Result_MakeError("Uncompressed store payload read out of bounds");
        }

        if (Buf && Ent->UncompressedSize > 0)
        {
            Mem_Copy(Buf, Payload, Ent->UncompressedSize);
        }
    }
    else
    {
        const Uint8 *Payload = Mem_Stream_ReadBytes(&R, Ent->CompressedSize);
        if (R.HasError)
        {
            return Result_MakeError("Compressed payload read out of bounds");
        }

        Bool IsDeflate64 = (Ent->CompressionMethod == CompressionDeflate64);
        Result Err = DecompressDeflate(Buf, BufSize, Payload, Ent->CompressedSize, IsDeflate64);
        if (Err.Error)
        {
            return Err;
        }
    }

    return Result_Success();
}

Bool Zip_Entry_EndsWith(const Zip_Entry *Ent, const char *Suffix)
{
    Usize SuffixLen = Mem_CStrLen(Suffix);
    if (Ent->NameLen < SuffixLen)
    {
        return False;
    }

    return Mem_Eql(Ent->NamePtr + Ent->NameLen - SuffixLen, Suffix, SuffixLen);
}

//
// NOTE: Writer
//

#define LzHashSize 4096
#define LzWindowSize 8192
#define LzMaxChain 32

static inline Uint32 Hash3(const Uint8 *Ptr)
{
    return ((Ptr[0] << 10) ^ (Ptr[1] << 5) ^ Ptr[2]) & (LzHashSize - 1);
}

static inline Uint32 BitReverse(Uint32 Val, Uint32 Num)
{
    Uint32 Rev = 0;
    for (Uint32 I = 0; I < Num; ++I)
    {
        if (Val & (1U << I))
        {
            Rev |= 1U << (Num - 1 - I);
        }
    }
    return Rev;
}

static inline Void WriteSymbol(Mem_Stream *S, Uint32 Sym)
{
    if (Sym <= 143)
    {
        Uint32 Code = Sym + 0x30;
        Mem_Stream_WriteBits(S, BitReverse(Code, 8), 8);
    }
    else if (Sym <= 255)
    {
        Uint32 Code = Sym - 144 + 400;
        Mem_Stream_WriteBits(S, BitReverse(Code, 9), 9);
    }
    else if (Sym == 256)
    {
        Mem_Stream_WriteBits(S, BitReverse(0, 7), 7);
    }
    else if (Sym <= 279)
    {
        Uint32 Code = Sym - 256;
        Mem_Stream_WriteBits(S, BitReverse(Code, 7), 7);
    }
    else if (Sym <= 285)
    {
        Uint32 Code = Sym - 280 + 192;
        Mem_Stream_WriteBits(S, BitReverse(Code, 8), 8);
    }
}

static inline Void WriteDistanceCode(Mem_Stream *S, Uint32 DistCode)
{
    Mem_Stream_WriteBits(S, BitReverse(DistCode, 5), 5);
}

static inline Void EncodeLength(Uint32 Length, Uint32 *OutCode, Uint32 *OutExtraBits, Uint32 *OutExtraVal)
{
    static const Uint16 Base[29] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const Uint8 Extra[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};

    Assert(Length >= 3 && Length <= 258);

    Uint32 Index = 0;
    while (Index < 28 && Length >= Base[Index + 1])
    {
        Index++;
    }

    *OutCode = 257 + Index;
    *OutExtraBits = Extra[Index];
    *OutExtraVal = Length - Base[Index];
}

static inline Void EncodeDistance(Uint32 Dist, Uint32 *OutCode, Uint32 *OutExtraBits, Uint32 *OutExtraVal)
{
    static const Uint16 Base[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const Uint8 Extra[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

    Assert(Dist >= 1 && Dist <= 32768);

    Uint32 Index = 0;
    while (Index < 29 && Dist >= Base[Index + 1])
    {
        Index++;
    }

    *OutCode = Index;
    *OutExtraBits = Extra[Index];
    *OutExtraVal = Dist - Base[Index];
}

static inline Uint32 FindMatch(const Uint8 *Payload, Usize PayloadSize, Usize CurrentPos, Int32 HashHeadVal, const Int32 *HashPrev, Uint32 *OutDist)
{
    Uint32 BestLen = 0;
    Uint32 BestDist = 0;

    Int32 ChainIndex = HashHeadVal;
    Uint32 Limit = (CurrentPos > LzWindowSize) ? (Uint32)(CurrentPos - LzWindowSize) : 0;
    Uint32 SearchDepth = 0;

    while (ChainIndex >= (Int32)Limit && SearchDepth < LzMaxChain)
    {
        SearchDepth++;

        Usize MatchPos = (Usize)ChainIndex;
        Uint32 Len = 0;
        Uint32 MaxMatch = 258;

        if (CurrentPos + MaxMatch > PayloadSize)
        {
            MaxMatch = (Uint32)(PayloadSize - CurrentPos);
        }

        if (BestLen > 0 && Payload[MatchPos + BestLen - 1] != Payload[CurrentPos + BestLen - 1])
        {
            ChainIndex = HashPrev[ChainIndex & (LzWindowSize - 1)];
            continue;
        }

        while (Len < MaxMatch && Payload[MatchPos + Len] == Payload[CurrentPos + Len])
        {
            Len++;
        }

        if (Len >= 3 && Len > BestLen)
        {
            BestLen = Len;
            BestDist = (Uint32)(CurrentPos - MatchPos);
            if (Len == 258)
            {
                break;
            }
        }

        ChainIndex = HashPrev[ChainIndex & (LzWindowSize - 1)];
    }

    *OutDist = BestDist;
    return BestLen;
}

static inline Bool CompressDeflate(Mem_Stream *S, const Uint8 *Payload, Usize PayloadSize)
{
    Assert(S);
    if (PayloadSize > 0)
    {
        Assert(Payload);
    }

    if (S->HasError)
    {
        return False;
    }

    S->BitBuf = 0;
    S->BitCount = 0;

    Mem_Stream_WriteBits(S, 3, 3);

    if (PayloadSize == 0)
    {
        WriteSymbol(S, 256);
        Mem_Stream_FlushBits(S);
        return True;
    }

    Int32 HashHead[LzHashSize];
    Int32 HashPrev[LzWindowSize];

    for (Uint32 I = 0; I < LzHashSize; ++I)
    {
        HashHead[I] = -1;
    }

    Usize I = 0;
    while (I < PayloadSize)
    {
        Uint32 MatchLen = 0;
        Uint32 MatchDist = 0;

        if (I + 3 <= PayloadSize)
        {
            Uint32 Hash = Hash3(Payload + I);
            Int32 Head = HashHead[Hash];
            MatchLen = FindMatch(Payload, PayloadSize, I, Head, HashPrev, &MatchDist);
        }

        if (MatchLen >= 3)
        {
            Uint32 LenCode = 0, LenExtraBits = 0, LenExtraVal = 0;
            EncodeLength(MatchLen, &LenCode, &LenExtraBits, &LenExtraVal);

            Uint32 DistCode = 0, DistExtraBits = 0, DistExtraVal = 0;
            EncodeDistance(MatchDist, &DistCode, &DistExtraBits, &DistExtraVal);

            WriteSymbol(S, LenCode);
            if (LenExtraBits > 0)
            {
                Mem_Stream_WriteBits(S, LenExtraVal, LenExtraBits);
            }

            WriteDistanceCode(S, DistCode);
            if (DistExtraBits > 0)
            {
                Mem_Stream_WriteBits(S, DistExtraVal, DistExtraBits);
            }

            for (Uint32 J = 0; J < MatchLen; ++J)
            {
                if (I + 3 <= PayloadSize)
                {
                    Uint32 Hash = Hash3(Payload + I);
                    HashPrev[I & (LzWindowSize - 1)] = HashHead[Hash];
                    HashHead[Hash] = (Int32)I;
                }
                I++;
            }
        }
        else
        {
            WriteSymbol(S, Payload[I]);

            if (I + 3 <= PayloadSize)
            {
                Uint32 Hash = Hash3(Payload + I);
                HashPrev[I & (LzWindowSize - 1)] = HashHead[Hash];
                HashHead[Hash] = (Int32)I;
            }
            I++;
        }

        if (S->HasError)
        {
            return False;
        }
    }

    WriteSymbol(S, 256);
    Mem_Stream_FlushBits(S);

    return !S->HasError;
}

//

static Uint32 CRC32(const Uint8 *Mem, Usize Size)
{
    Uint32 CRC = 0xffffffff;
    for (Usize I = 0; I < Size; I++)
    {
        CRC ^= Mem[I];
        for (Int32 J = 0; J < 8; J++)
        {
            if (CRC & 1)
            {
                CRC = (CRC >> 1) ^ 0xedb88320;
            }
            else
            {
                CRC >>= 1;
            }
        }
    }
    return CRC ^ 0xffffffff;
}

Mem_Stream Zip_Writer_Init(Void *Buf, Usize Cap)
{
    return Mem_Stream_Init(Buf, Cap);
}

Result Zip_Writer_Append(Mem_Stream *S, const char *Name, const Uint8 *Mem, Usize Size)
{
    Assert(S);
    Assert(Name);
    if (Size > 0)
    {
        Assert(Mem);
    }

    if (S->HasError)
    {
        return Result_MakeError("Stream is in error state before appending entry");
    }

    Usize NameLen = Mem_CStrLen(Name);
    if (NameLen > 0xffff)
    {
        S->HasError = True;
        return Result_MakeError("Name too long");
    }

    Uint32 CRC = 0;
    if (Mem && Size > 0)
    {
        CRC = CRC32(Mem, Size);
    }

    Usize LfhOffset = S->Pos;

    Mem_Stream_WriteU32LE(S, SigLocalHeader);
    // NOTE: Version needed to extract
    Mem_Stream_WriteU16LE(S, 20);
    // NOTE: General purpose flag
    Mem_Stream_WriteU16LE(S, 0);
    // NOTE: Compression method (8)
    Mem_Stream_WriteU16LE(S, CompressionDeflate);
    // NOTE: Mod time
    Mem_Stream_WriteU16LE(S, 0);
    // NOTE: Mod date
    Mem_Stream_WriteU16LE(S, 0);
    // NOTE: CRC-32 (patched later)
    Mem_Stream_WriteU32LE(S, 0);
    // NOTE: Compressed size (patched later)
    Mem_Stream_WriteU32LE(S, 0);
    // NOTE: Uncompressed size (patched later)
    Mem_Stream_WriteU32LE(S, 0);
    Mem_Stream_WriteU16LE(S, (Uint16)NameLen);
    // NOTE: Extra field length
    Mem_Stream_WriteU16LE(S, 0);

    Mem_Stream_WriteBytes(S, Name, NameLen);

    Usize PayloadStart = S->Pos;

    if (!CompressDeflate(S, Mem, Size))
    {
        return Result_MakeError("Deflate compression failed");
    }

    Usize PayloadEnd = S->Pos;
    Usize CompressedSize = PayloadEnd - PayloadStart;

    Usize CurrentPos = S->Pos;
    Mem_Stream_Seek(S, LfhOffset + 14);
    Mem_Stream_WriteU32LE(S, CRC);
    Mem_Stream_WriteU32LE(S, (Uint32)CompressedSize);
    Mem_Stream_WriteU32LE(S, (Uint32)Size);
    Mem_Stream_Seek(S, CurrentPos);

    if (S->HasError)
    {
        return Result_MakeError("Buffer overflow while writing file entry");
    }

    return Result_Success();
}

Result Zip_Writer_Flush(Mem_Stream *S, Usize *OutWritten)
{
    Assert(S);
    Assert(OutWritten);

    if (S->HasError)
    {
        return Result_MakeError("Stream is in error state before flush");
    }

    Usize CdOffset = S->Pos;
    Uint32 EntryCount = 0;

    Mem_Stream R = Mem_Stream_Init(S->Base, CdOffset);

    while (R.Pos < CdOffset)
    {
        Usize LfhOffset = R.Pos;

        Uint32 Sig = Mem_Stream_ReadU32LE(&R);
        if (Sig != SigLocalHeader)
        {
            S->HasError = True;
            return Result_MakeError("Expected LFH signature not found");
        }

        // NOTE: VersionNeeded and Flag
        MemStreamSkip(&R, 4);
        Uint16 CompressionMethod = Mem_Stream_ReadU16LE(&R);
        Uint16 ModTime = Mem_Stream_ReadU16LE(&R);
        Uint16 ModDate = Mem_Stream_ReadU16LE(&R);
        Uint32 CRC = Mem_Stream_ReadU32LE(&R);
        Uint32 CompressedSize = Mem_Stream_ReadU32LE(&R);
        Uint32 UncompressedSize = Mem_Stream_ReadU32LE(&R);
        Uint16 NameLen = Mem_Stream_ReadU16LE(&R);
        Uint16 ExtraLen = Mem_Stream_ReadU16LE(&R);

        const Uint8 *NamePtr = Mem_Stream_ReadBytes(&R, NameLen);
        MemStreamSkip(&R, ExtraLen + CompressedSize);

        if (R.HasError)
        {
            S->HasError = True;
            return Result_MakeError("Corrupted local header data during finalize");
        }

        Mem_Stream_WriteU32LE(S, SigCDHeader);
        // NOTE: Version made by
        Mem_Stream_WriteU16LE(S, 20);
        // NOTE: Version needed to extract
        Mem_Stream_WriteU16LE(S, 10);
        // NOTE: General purpose flag
        Mem_Stream_WriteU16LE(S, 0);
        Mem_Stream_WriteU16LE(S, CompressionMethod);
        Mem_Stream_WriteU16LE(S, ModTime);
        Mem_Stream_WriteU16LE(S, ModDate);
        Mem_Stream_WriteU32LE(S, CRC);
        Mem_Stream_WriteU32LE(S, CompressedSize);
        Mem_Stream_WriteU32LE(S, UncompressedSize);
        Mem_Stream_WriteU16LE(S, NameLen);
        // NOTE: Extra len
        Mem_Stream_WriteU16LE(S, 0);
        // NOTE: Comment len
        Mem_Stream_WriteU16LE(S, 0);
        // NOTE: Disk num
        Mem_Stream_WriteU16LE(S, 0);
        // NOTE: Internal attr
        Mem_Stream_WriteU16LE(S, 0);
        // NOTE: External attr
        Mem_Stream_WriteU32LE(S, 0);
        Mem_Stream_WriteU32LE(S, (Uint32)LfhOffset);

        Mem_Stream_WriteBytes(S, NamePtr, NameLen);

        EntryCount++;
    }

    Usize CdSize = S->Pos - CdOffset;

    if (EntryCount > 0xFFFF)
    {
        S->HasError = True;
        return Result_MakeError("Too many entries");
    }

    Mem_Stream_WriteU32LE(S, SigEOCD);
    // NOTE: Disk number
    Mem_Stream_WriteU16LE(S, 0);
    // NOTE: CD Disk number
    Mem_Stream_WriteU16LE(S, 0);
    // NOTE: Disk Entries
    Mem_Stream_WriteU16LE(S, (Uint16)EntryCount);
    // NOTE: Total Entries
    Mem_Stream_WriteU16LE(S, (Uint16)EntryCount);
    Mem_Stream_WriteU32LE(S, (Uint32)CdSize);
    Mem_Stream_WriteU32LE(S, (Uint32)CdOffset);
    // NOTE: Comment length
    Mem_Stream_WriteU16LE(S, 0);

    if (S->HasError)
    {
        return Result_MakeError("Buffer overflow during Central Directory write");
    }

    *OutWritten = S->Pos;

    return Result_Success();
}
