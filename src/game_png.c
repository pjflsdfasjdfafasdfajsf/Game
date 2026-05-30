#include "game_png.h"
#include "game_platform.h"
#include "game_types.h"

// NOTE: CRC32.

static u32 CRC32Table[256];
static bool CRC32TableInitialized = false;

static void CRC32TableInitialize(void) {
    if (CRC32TableInitialized) {
        return;
    }

    const u32 CRC32Polynomial = 0xEDB88320;

    for (u32 i = 0; i < 256; i++) {
        u32 crc = i;
        for (u32 j = 0; j < 8; j++) {
            if (IsBitSet(crc, 1)) {
                crc = CRC32Polynomial ^ (crc >> 1);
            } else {
                crc = (crc >> 1);
            }
        }
        CRC32Table[i] = crc;
    }

    CRC32TableInitialized = true;
}

static u32 CRC32Calculate(const u8 *memory, usize length) {
    CRC32TableInitialize();

    u32 crc = 0xFFFFFFFF;

    for (usize i = 0; i < length; i++) {
        u32 tableIndex = (crc ^ memory[i]) & 0xFF;
        crc = CRC32Table[tableIndex] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

// NOTE: Primary PNG stuff.

static const u8 PNGFileSignature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
static const usize PNGFileSignatureLength = 8;
static const usize PNGChunkLengthSize = 4;
static const usize PNGChunkTypeSize = 4;
static const usize PNGChunkCRCSize = 4;
static const u32 PNGIHDRChunkLength = 13;
static const u32 PNGMaxIDATChunks = 1024;

typedef enum {
    PNGColorType_Grayscale = 0,
    PNGColorType_TrueColor = 2,
    PNGColorType_Indexed = 3,
    PNGColorType_GrayscaleAlpha = 4,
    PNGColorTypeTrue_ColorAlpha = 6
} PNGColorType;

typedef struct {
    u32 width;
    u32 height;
    u8 bitDepth;
    u8 colorType;
    u8 compressionMethod;
    u8 filterMethod;
    u8 interlaceMethod;
} PNGHeader;

typedef struct {
    const u8 *memory;
    u32 length;
} PNGIDATChunk;

// NOTE: Deflating.

static const u16 deflateLengthBases[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};

static const u8 deflateLengthExtraBits[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};

static const u16 deflateDistanceBases[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577};

static const u8 deflateDistanceExtraBits[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static const u8 deflateCodeLengthOrder[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

typedef struct {
    u16 counts[16];
    u16 symbols[288];
} HuffmanTree;

typedef struct {
    const PNGIDATChunk *chunks;
    usize chunkCount;
    usize currentChunkIndex;
    usize byteOffsetInChunk;

    u32 bitBuffer;
    u32 bitsAvailable;
    bool hasOverflowed;
} BitStream;

static inline u32 StreamReadBits(BitStream *stream, u32 bitCount) {
    while (stream->bitsAvailable < bitCount) {
        if (stream->currentChunkIndex >= stream->chunkCount) {
            stream->hasOverflowed = true;
            return 0;
        }

        const PNGIDATChunk *currentChunk = &stream->chunks[stream->currentChunkIndex];

        u8 nextByte = currentChunk->memory[stream->byteOffsetInChunk];
        stream->bitBuffer |= ((u32)nextByte << stream->bitsAvailable);
        stream->bitsAvailable += 8;

        stream->byteOffsetInChunk++;

        if (stream->byteOffsetInChunk >= currentChunk->length) {
            stream->currentChunkIndex++;
            stream->byteOffsetInChunk = 0;
        }
    }

    u32 result = stream->bitBuffer & ((1U << bitCount) - 1);
    stream->bitBuffer >>= bitCount;
    stream->bitsAvailable -= bitCount;

    return result;
}

static u32 StreamDecodeSymbol(BitStream *stream, const HuffmanTree *tree) {
    u32 code = 0;
    u32 firstCode = 0;
    u32 firstSymbolIndex = 0;

    for (u32 length = 1; length <= 15; length++) {
        code = (code << 1) | StreamReadBits(stream, 1);

        u32 count = tree->counts[length];
        if (code - firstCode < count) {
            return tree->symbols[firstSymbolIndex + (code - firstCode)];
        }

        firstSymbolIndex += count;
        firstCode = (firstCode + count) << 1;
    }

    stream->hasOverflowed = true;
    return 0;
}

static bool HuffmanTreeInitialize(HuffmanTree *tree, const u8 *lengths, u32 count) {
    ZeroArray(tree->counts);
    ZeroArray(tree->symbols);

    for (u32 i = 0; i < count; i++) {
        if (lengths[i] > 15) {
            return false;
        }

        tree->counts[lengths[i]]++;
    }
    tree->counts[0] = 0;

    u16 nextSymbolIndex[16];
    ZeroArray(nextSymbolIndex);

    u16 symbolIndex = 0;
    for (u32 i = 1; i <= 15; i++) {
        nextSymbolIndex[i] = symbolIndex;
        symbolIndex += tree->counts[i];
    }

    for (u32 i = 0; i < count; i++) {
        u8 length = lengths[i];

        if (length != 0) {
            tree->symbols[nextSymbolIndex[length]++] = (u16)i;
        }
    }
    return true;
}

static bool DecompressDeflate(const PNGIDATChunk *chunks, usize chunkCount, u8 *outputBuffer, usize outputCapacity) {
    BitStream stream;
    ZeroStruct(stream);

    stream.chunks = chunks;
    stream.chunkCount = chunkCount;

    u32 zlibMethodFlags = StreamReadBits(&stream, 8);
    u32 zlibAdditionalFlags = StreamReadBits(&stream, 8);

    if ((zlibMethodFlags & 0x0F) != 8) {
        // NOTE: Not DEFLATE
        return false;
    }
    if (((zlibMethodFlags << 8) + zlibAdditionalFlags) % 31 != 0) {
        // NOTE: Checksum failed
        return false;
    }
    if (IsBitSet(zlibAdditionalFlags, 0x20)) {
        // NOTE: Preset dictionaries are not allowed in PNG
        return false;
    }

    usize outputPosition = 0;
    bool isFinalBlock = false;

    while (!isFinalBlock && !stream.hasOverflowed) {
        isFinalBlock = StreamReadBits(&stream, 1);
        u32 blockType = StreamReadBits(&stream, 2);

        if (blockType == 0) {
            stream.bitBuffer = 0;
            stream.bitsAvailable = 0;

            u32 length = StreamReadBits(&stream, 16);
            u32 invertedLength = StreamReadBits(&stream, 16);
            if (length != (~invertedLength & 0xFFFF)) {
                return false;
            }

            for (u32 i = 0; i < length; i++) {
                if (outputPosition >= outputCapacity) {
                    return false;
                }

                outputBuffer[outputPosition++] = (u8)StreamReadBits(&stream, 8);
            }
        } else if (blockType == 1 || blockType == 2) {
            HuffmanTree literalTree;
            ZeroStruct(literalTree);

            HuffmanTree distanceTree;
            ZeroStruct(distanceTree);

            // NOTE: Fixed huffman
            if (blockType == 1) {
                u8 lengths[288];
                for (u32 i = 0; i <= 143; i++) {
                    lengths[i] = 8;
                }
                for (u32 i = 144; i <= 255; i++) {
                    lengths[i] = 9;
                }
                for (u32 i = 256; i <= 279; i++) {
                    lengths[i] = 7;
                }
                for (u32 i = 280; i <= 287; i++) {
                    lengths[i] = 8;
                }

                HuffmanTreeInitialize(&literalTree, lengths, 288);

                u8 distLengths[32];
                for (u32 i = 0; i < 32; i++) {
                    distLengths[i] = 5;
                }

                HuffmanTreeInitialize(&distanceTree, distLengths, 32);
            } else { // NOTE: Dynamic huffman
                u32 hlit = StreamReadBits(&stream, 5) + 257;
                u32 hdist = StreamReadBits(&stream, 5) + 1;
                u32 hclen = StreamReadBits(&stream, 4) + 4;

                u8 codeLengthLengths[19];
                ZeroArray(codeLengthLengths);

                for (u32 i = 0; i < hclen; i++) {
                    codeLengthLengths[deflateCodeLengthOrder[i]] = (u8)StreamReadBits(&stream, 3);
                }

                HuffmanTree codeLengthTree;
                ZeroStruct(codeLengthTree);

                HuffmanTreeInitialize(&codeLengthTree, codeLengthLengths, 19);

                u8 lengths[320];
                ZeroArray(lengths);

                u32 totalCodes = hlit + hdist;
                u32 index = 0;

                while (index < totalCodes) {
                    u32 symbol = StreamDecodeSymbol(&stream, &codeLengthTree);
                    if (symbol <= 15) {
                        lengths[index++] = (u8)symbol;
                    } else if (symbol == 16) {
                        u32 repeatCount = StreamReadBits(&stream, 2) + 3;

                        if (index == 0) {
                            return false;
                        }
                        u8 repeatValue = lengths[index - 1];

                        while (repeatCount--) {
                            lengths[index++] = repeatValue;
                        }
                    } else if (symbol == 17) {
                        u32 repeatCount = StreamReadBits(&stream, 3) + 3;

                        while (repeatCount--) {
                            lengths[index++] = 0;
                        }
                    } else if (symbol == 18) {
                        u32 repeatCount = StreamReadBits(&stream, 7) + 11;

                        while (repeatCount--) {
                            lengths[index++] = 0;
                        }
                    }
                }

                HuffmanTreeInitialize(&literalTree, lengths, hlit);
                HuffmanTreeInitialize(&distanceTree, lengths + hlit, hdist);
            }

            while (!stream.hasOverflowed) {
                u32 symbol = StreamDecodeSymbol(&stream, &literalTree);

                if (symbol < 256) {
                    if (outputPosition >= outputCapacity) {
                        return false;
                    }

                    outputBuffer[outputPosition++] = (u8)symbol;
                } else if (symbol == 256) {
                    break;
                } else {
                    if (symbol > 285 || symbol < 257) {
                        return false;
                    }
                    symbol -= 257;
                    u32 length = deflateLengthBases[symbol] + StreamReadBits(&stream, deflateLengthExtraBits[symbol]);

                    u32 distanceSymbol = StreamDecodeSymbol(&stream, &distanceTree);
                    if (distanceSymbol >= 30) {
                        return false;
                    }

                    u32 distance = deflateDistanceBases[distanceSymbol] + StreamReadBits(&stream, deflateDistanceExtraBits[distanceSymbol]);

                    for (u32 i = 0; i < length; i++) {
                        if (outputPosition >= outputCapacity || outputPosition < distance) {
                            return false;
                        }

                        outputBuffer[outputPosition] = outputBuffer[outputPosition - distance];
                        outputPosition++;
                    }
                }
            }
        } else {
            return false;
        }
    }

    return !stream.hasOverflowed;
}

static inline u8 PaethPredictor(u8 leftByte, u8 upByte, u8 upLeftByte) {
    const int a = (int)leftByte;
    const int b = (int)upByte;
    const int c = (int)upLeftByte;

    const int initialEstimate = a + b - c;
    const int distanceA = Abs(initialEstimate - a);
    const int distanceB = Abs(initialEstimate - b);
    const int distanceC = Abs(initialEstimate - c);

    if (distanceA <= distanceB && distanceA <= distanceC) {
        return (u8)a;
    } else if (distanceB <= distanceC) {
        return (u8)b;
    } else {
        return (u8)c;
    }
}

Image ImageLoadFromPNG(MemoryArena *permanentArena, MemoryArena *temporaryArena, const void *memory, usize length) {
    Image result;
    ZeroStruct(result);

    if (!memory) {
        return result;
    }

    if (length < PNGFileSignatureLength) {
        return result;
    }

    const u8 *imageBufferPointer = (const u8 *)memory;

    bool hasValidSignature = MemoryEquals(imageBufferPointer, PNGFileSignature, PNGFileSignatureLength);
    if (!hasValidSignature) {
        return result;
    }

    BinaryStream stream;
    BinaryStreamInitialize(&stream, memory, length);
    stream.offset = PNGFileSignatureLength;

    bool hasParsedIHDR = false;

    PNGHeader imageHeader;
    ZeroStruct(imageHeader);

    PNGIDATChunk *idatChunks = MemoryArenaPushArray(temporaryArena, PNGIDATChunk, PNGMaxIDATChunks);
    if (!idatChunks) {
        return result;
    }

    usize idatChunkCount = 0;
    usize totalIDATDataSize = 0;

    bool hasSeenIDAT = false;
    bool isPreviousChunkIDAT = false;

    while (stream.offset < stream.capacity) {
        if (!BinaryStreamHasSpace(&stream, PNGChunkLengthSize + PNGChunkTypeSize)) {
            break;
        }

        u32 chunkLength = BinaryStreamReadUInt32BigEndian(&stream);

        const u8 *chunkTypePointer = &stream.memory[stream.offset];
        stream.offset += PNGChunkTypeSize;

        if (!BinaryStreamHasSpace(&stream, chunkLength + PNGChunkCRCSize)) {
            break;
        }

        const u8 *chunkDataPointer = &stream.memory[stream.offset];
        stream.offset += chunkLength;

        u32 expectedCRC = BinaryStreamReadUInt32BigEndian(&stream);

        usize crcDataLength = PNGChunkTypeSize + chunkLength;
        u32 calculatedCRC = CRC32Calculate(chunkTypePointer, crcDataLength);

        if (calculatedCRC != expectedCRC) {
            break;
        }

        if (MemoryEquals(chunkTypePointer, "IHDR", 4)) {
            if (chunkLength != PNGIHDRChunkLength) {
                break;
            }

            if (hasParsedIHDR) {
                break;
            }

            imageHeader.width = ReadUInt32BigEndian(&chunkDataPointer[0]);
            imageHeader.height = ReadUInt32BigEndian(&chunkDataPointer[4]);
            imageHeader.bitDepth = chunkDataPointer[8];
            imageHeader.colorType = chunkDataPointer[9];
            imageHeader.compressionMethod = chunkDataPointer[10];
            imageHeader.filterMethod = chunkDataPointer[11];
            imageHeader.interlaceMethod = chunkDataPointer[12];

            if (imageHeader.width == 0 || imageHeader.height == 0) {
                break;
            }

            if (imageHeader.compressionMethod != 0 || imageHeader.filterMethod != 0 || imageHeader.bitDepth != 8 || imageHeader.interlaceMethod != 0) {
                break;
            }

            hasParsedIHDR = true;
            isPreviousChunkIDAT = false;
        } else if (!hasParsedIHDR) {
            break;
        } else if (MemoryEquals(chunkTypePointer, "IDAT", 4)) {
            if (hasSeenIDAT && !isPreviousChunkIDAT) {
                break;
            }

            if (idatChunkCount >= PNGMaxIDATChunks) {
                break;
            }

            idatChunks[idatChunkCount].memory = chunkDataPointer;
            idatChunks[idatChunkCount].length = chunkLength;

            idatChunkCount++;
            totalIDATDataSize += chunkLength;

            hasSeenIDAT = true;
            isPreviousChunkIDAT = true;
        } else if (MemoryEquals(chunkTypePointer, "IEND", 4)) {
            isPreviousChunkIDAT = false;

            break;
        } else {
            bool isCriticalChunk = !IsBitSet(chunkTypePointer[0], 0x20);
            if (isCriticalChunk) {
                break;
            }

            isPreviousChunkIDAT = false;
        }
    }

    if (!hasParsedIHDR || totalIDATDataSize == 0) {
        return result;
    }

    u32 bytesPerPixel = 0;

    if (imageHeader.colorType == PNGColorType_Grayscale) {
        bytesPerPixel = 1;
    } else if (imageHeader.colorType == PNGColorType_TrueColor) {
        bytesPerPixel = 3;
    } else if (imageHeader.colorType == PNGColorType_GrayscaleAlpha) {
        bytesPerPixel = 2;
    } else if (imageHeader.colorType == PNGColorTypeTrue_ColorAlpha) {
        bytesPerPixel = 4;
    } else {
        return result;
    }

    usize scanlineStride = 1 + (usize)imageHeader.width * bytesPerPixel;
    usize decompressedCapacity = scanlineStride * imageHeader.height;

    u8 *decompressedBuffer = MemoryArenaPushBytes(temporaryArena, decompressedCapacity);
    if (!decompressedBuffer) {
        return result;
    }

    bool succesfullyDecompressed = DecompressDeflate(idatChunks, idatChunkCount, decompressedBuffer, decompressedCapacity);
    if (!succesfullyDecompressed) {
        return result;
    }

    const u32 imageWidth = imageHeader.width;
    const u32 imageHeight = imageHeader.height;

    const usize rawScanlineByteCount = (usize)imageWidth * bytesPerPixel;
    if (imageWidth != 0 && rawScanlineByteCount / imageWidth != bytesPerPixel) {
        return result;
    }

    const usize filteredScanlineByteCount = rawScanlineByteCount + 1;
    const usize expectedDecompressedByteCount = filteredScanlineByteCount * imageHeight;

    if (decompressedCapacity < expectedDecompressedByteCount) {
        return result;
    }

    const usize totalRawPixelByteCount = rawScanlineByteCount * imageHeight;
    u8 *rawPixelDataBuffer = MemoryArenaPushBytes(permanentArena, totalRawPixelByteCount);
    if (!rawPixelDataBuffer) {
        return result;
    }

    usize sourceReadOffset = 0;
    usize destinationWriteOffset = 0;

    for (u32 currentScanlineIndex = 0; currentScanlineIndex < imageHeight; currentScanlineIndex++) {
        if (sourceReadOffset + filteredScanlineByteCount > decompressedCapacity) {
            return result;
        }

        const u8 scanlineFilterType = decompressedBuffer[sourceReadOffset++];
        const u8 *currentFilteredScanlineData = &decompressedBuffer[sourceReadOffset];
        u8 *currentRawScanlineData = &rawPixelDataBuffer[destinationWriteOffset];

        const u8 *previousRawScanlineData = (currentScanlineIndex > 0) ? &rawPixelDataBuffer[destinationWriteOffset - rawScanlineByteCount] : 0;

        if (scanlineFilterType > 4) {
            return result;
        }

        for (usize byteIndex = 0; byteIndex < rawScanlineByteCount; byteIndex++) {
            u8 rawLeft = (byteIndex >= bytesPerPixel) ? currentRawScanlineData[byteIndex - bytesPerPixel] : 0;
            u8 rawUp = previousRawScanlineData ? previousRawScanlineData[byteIndex] : 0;

            u8 prediction = 0;
            switch (scanlineFilterType) {
            case 1: {
                prediction = rawLeft;
                break;
            }
            case 2: {
                prediction = rawUp;
                break;
            }
            case 3: {
                prediction = (u8)(((u16)rawLeft + (u16)rawUp) / 2);
                break;
            }
            case 4: {
                u8 rawUpLeft = (previousRawScanlineData && byteIndex >= bytesPerPixel) ? previousRawScanlineData[byteIndex - bytesPerPixel] : 0;
                prediction = PaethPredictor(rawLeft, rawUp, rawUpLeft);
            } break;
            }

            currentRawScanlineData[byteIndex] = currentFilteredScanlineData[byteIndex] + prediction;
        }

        sourceReadOffset += rawScanlineByteCount;
        destinationWriteOffset += rawScanlineByteCount;
    }

    result.size.width = imageWidth;
    result.size.height = imageHeight;
    result.bytesPerPixel = bytesPerPixel;
    result.pixels = rawPixelDataBuffer;

    return result;
}
