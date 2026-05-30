#include "game_ttf.h"
#include "game_platform.h"
// TODO: Make this crossplatform and do not depend on this header.
#include "game_png.h"
#include "game_rectangle_pack.h"

// NOTE: You can find the spec here:  https://developer.apple.com/fonts/TrueType-Reference-Manual
//

// NOTE: Table parsing.

typedef struct {
    u32 version;
    u32 fontRevision;
    u32 checkSumAdjustment;
    u32 magicNumber;
    u16 flags;
    u16 unitsPerEm;
    i64 created;
    i64 modified;
    i16 xMin;
    i16 yMin;
    i16 xMax;
    i16 yMax;
    u16 macStyle;
    u16 lowestRecPPEM;
    i16 fontDirectionHint;
    i16 indexToLocFormat;
    i16 glyphDataFormat;
} TrueTypeHeadTable;

typedef struct {
    u32 version;
    u16 numGlyphs;
    // NOTE: the following fields are only valid if version is 0x00010000 (1.0).
    u16 maxPoints;
    u16 maxContours;
    u16 maxComponentPoints;
    u16 maxComponentContours;
    u16 maxZones;
    u16 maxTwilightPoints;
    u16 maxStorage;
    u16 maxFunctionDefs;
    u16 maxInstructionDefs;
    u16 maxStackElements;
    u16 maxSizeOfInstructions;
    u16 maxComponentElements;
    u16 maxComponentDepth;
} TrueTypeMaximumProfileTable;

typedef struct {
    bool isValid;
    u16 segCount;
    usize endCodeOffset;
    usize startCodeOffset;
    usize idDeltaOffset;
    usize idRangeOffsetOffset;
    const u8 *memory;
    usize length;
} TrueTypeCmapFormat4;

typedef struct {
    bool isValid;
    i16 indexToLocFormat;
    u16 numGlyphs;
    const u8 *memory;
    usize length;
} TrueTypeIndexToLocationTable;

typedef struct {
    // NOTE: this is relative to the start of the 'glyf' table
    u32 offset;
    u32 length;
} TrueTypeGlyphLocation;

typedef struct {
    i16 x;
    i16 y;
    bool isOnCurve;
} TrueTypeGlyphPoint;

typedef struct {
    bool isValid;
    // NOTE: they are rejected :)
    bool isCompound;

    i16 numberOfContours;
    i16 xMin;
    i16 yMin;
    i16 xMax;
    i16 yMax;

    u32 numberOfPoints;
    u16 *endPointsOfContours;

    u16 instructionLength;
    const u8 *instructions;

    TrueTypeGlyphPoint *points;
} TrueTypeSimpleGlyph;

// NOTE: Glyph outline flags
enum {
    TrueTypeGlyphFlag_OnCurve = (1 << 0),
    TrueTypeGlyphFlag_XShortVector = (1 << 1),
    TrueTypeGlyphFlag_YShortVector = (1 << 2),
    TrueTypeGlyphFlag_Repeat = (1 << 3),
    TrueTypeGlyphFlag_XIsSameOrPositiveXShort = (1 << 4),
    TrueTypeGlyphFlag_YIsSameOrPositiveYShort = (1 << 5),
};

// NOTE: Table tags
enum {
    TrueTypeTableTag_HEAD = FOURCC('h', 'e', 'a', 'd'),
    TrueTypeTableTag_MAXP = FOURCC('m', 'a', 'x', 'p'),
    TrueTypeTableTag_CMAP = FOURCC('c', 'm', 'a', 'p'),
    TrueTypeTableTag_LOCA = FOURCC('l', 'o', 'c', 'a'),
    TrueTypeTableTag_GLYF = FOURCC('g', 'l', 'y', 'f'),
};

typedef struct {
    Vector2 p1;
    Vector2 p2;
} TrueTypeEdge;

typedef struct {
    f32 x;
    i32 direction;
} TrueTypeScanlineIntersection;

static u32 TrueTypeCalculateTableChecksum(const u8 *tableMemory, u32 tableLength, bool isHeadTable) {
    u32 checksum = 0;
    u32 numberOfLongs = (tableLength + 3) / 4;
    usize currentReadOffset = 0;

    for (u32 index = 0; index < numberOfLongs; index++) {
        u8 byte0 = (currentReadOffset < tableLength) ? tableMemory[currentReadOffset++] : 0;
        u8 byte1 = (currentReadOffset < tableLength) ? tableMemory[currentReadOffset++] : 0;
        u8 byte2 = (currentReadOffset < tableLength) ? tableMemory[currentReadOffset++] : 0;
        u8 byte3 = (currentReadOffset < tableLength) ? tableMemory[currentReadOffset++] : 0;

        u32 value = ((u32)byte0 << 24) |
                    ((u32)byte1 << 16) |
                    ((u32)byte2 << 8) |
                    ((u32)byte3 << 0);

        if (isHeadTable && index == 2) {
            value = 0;
        }

        checksum += value;
    }

    return checksum;
}

TrueTypeFont TrueTypeFontLoadFromMemory(MemoryArena *arena, const void *memory, usize length) {
    TrueTypeFont result;
    MemoryZero(&result, sizeof(result));

    if (!memory) {
        return result;
    }

    const usize trueTypeOffsetSubtableLength = 12;
    if (length < trueTypeOffsetSubtableLength) {
        return result;
    }

    const u8 *fontBufferPointer = (const u8 *)memory;
    usize currentReadOffset = 0;

    u32 scalerTypeValue = ReadUInt32BigEndian(&fontBufferPointer[currentReadOffset]);
    currentReadOffset += 4;

    if (scalerTypeValue == 0x00010000) {
        result.scalerType = TrueTypeScalerTypeWindows;
    } else if (scalerTypeValue == FOURCC('t', 'r', 'u', 'e')) {
        result.scalerType = TrueTypeScalerTypeMac;
    } else if (scalerTypeValue == FOURCC('t', 'y', 'p', '1')) {
        result.scalerType = TrueTypeScalerTypePostScript;
    } else if (scalerTypeValue == FOURCC('O', 'T', 'T', 'O')) {
        result.scalerType = TrueTypeScalerTypeOpenType;
    } else {
        return result;
    }

    result.numberOfTables = ReadUInt16BigEndian(&fontBufferPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.searchRange = ReadUInt16BigEndian(&fontBufferPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.entrySelector = ReadUInt16BigEndian(&fontBufferPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.rangeShift = ReadUInt16BigEndian(&fontBufferPointer[currentReadOffset]);
    currentReadOffset += 2;

    const usize trueTypeTableDirectoryEntryLength = 16;
    usize expectedDirectorySize = trueTypeOffsetSubtableLength + ((usize)result.numberOfTables * trueTypeTableDirectoryEntryLength);

    if (length < expectedDirectorySize) {
        return result;
    }

    if (result.numberOfTables > 0) {
        result.tables = MemoryArenaPushArray(arena, TrueTypeTableDirectoryEntry, result.numberOfTables);
        if (!result.tables) {
            return result;
        }

        for (u16 tableIndex = 0; tableIndex < result.numberOfTables; tableIndex++) {
            const u8 *entryPointer = &fontBufferPointer[currentReadOffset];
            currentReadOffset += trueTypeTableDirectoryEntryLength;

            u32 tag = ReadUInt32BigEndian(&entryPointer[0]);
            u32 expectedChecksum = ReadUInt32BigEndian(&entryPointer[4]);
            u32 tableOffset = ReadUInt32BigEndian(&entryPointer[8]);
            u32 tableLength = ReadUInt32BigEndian(&entryPointer[12]);

            if (tableOffset > length || tableLength > (length - tableOffset)) {
                MemoryZero(&result, sizeof(result));

                return result;
            }

            const u8 *tableDataPointer = &fontBufferPointer[tableOffset];
            bool isHeadTable = (tag == TrueTypeTableTag_HEAD);

            u32 calculatedChecksum = TrueTypeCalculateTableChecksum(tableDataPointer, tableLength, isHeadTable);

            if (calculatedChecksum != expectedChecksum) {
                MemoryZero(&result, sizeof(result));

                return result;
            }

            result.tables[tableIndex].tag = tag;
            result.tables[tableIndex].checkSum = expectedChecksum;
            result.tables[tableIndex].offset = tableOffset;
            result.tables[tableIndex].length = tableLength;
            result.tables[tableIndex].memory = tableDataPointer;
        }
    }

    return result;
}

const TrueTypeTableDirectoryEntry *TrueTypeFontGetTable(const TrueTypeFont *font, u32 tableTag) {
    if (!font || !font->tables) {
        return 0;
    }

    for (u16 tableIndex = 0; tableIndex < font->numberOfTables; tableIndex++) {
        if (font->tables[tableIndex].tag == tableTag) {
            return &font->tables[tableIndex];
        }
    }

    return 0;
}

TrueTypeHeadTable TrueTypeHeadTableParse(const TrueTypeTableDirectoryEntry *headTableEntry) {
    TrueTypeHeadTable result;
    MemoryZero(&result, sizeof(result));

    if (!headTableEntry || !headTableEntry->memory) {
        return result;
    }

    const usize trueTypeHeadTableExpectedLength = 54;
    if (headTableEntry->length < trueTypeHeadTableExpectedLength) {
        return result;
    }

    const u8 *tableDataPointer = headTableEntry->memory;
    usize currentReadOffset = 0;

    result.version = ReadUInt32BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 4;

    result.fontRevision = ReadUInt32BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 4;

    result.checkSumAdjustment = ReadUInt32BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 4;

    result.magicNumber = ReadUInt32BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 4;

    const u32 trueTypeHeadTableMagicNumber = 0x5F0F3CF5;
    if (result.magicNumber != trueTypeHeadTableMagicNumber) {
        MemoryZero(&result, sizeof(result));
        return result;
    }

    result.flags = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.unitsPerEm = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    if (result.unitsPerEm < 64 || result.unitsPerEm > 16384) {
        MemoryZero(&result, sizeof(result));
        return result;
    }

    result.created = ReadInt64BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 8;

    result.modified = ReadInt64BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 8;

    result.xMin = ReadInt16BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.yMin = ReadInt16BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.xMax = ReadInt16BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.yMax = ReadInt16BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.macStyle = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.lowestRecPPEM = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.fontDirectionHint = ReadInt16BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.indexToLocFormat = ReadInt16BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.glyphDataFormat = ReadInt16BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    return result;
}

TrueTypeMaximumProfileTable TrueTypeMaximumProfileTableParse(const TrueTypeTableDirectoryEntry *maximumProfileTableEntry) {
    TrueTypeMaximumProfileTable result;
    MemoryZero(&result, sizeof(result));

    if (!maximumProfileTableEntry || !maximumProfileTableEntry->memory) {
        return result;
    }

    const usize trueTypeMaximumProfileTableVersion05Length = 6;
    const usize trueTypeMaximumProfileTableVersion10Length = 32;

    if (maximumProfileTableEntry->length < trueTypeMaximumProfileTableVersion05Length) {
        return result;
    }

    const u8 *tableDataPointer = maximumProfileTableEntry->memory;
    usize currentReadOffset = 0;

    result.version = ReadUInt32BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 4;

    result.numGlyphs = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    const u32 trueTypeMaximumProfileVersion05 = 0x00005000;
    const u32 trueTypeMaximumProfileVersion10 = 0x00010000;

    if (result.version == trueTypeMaximumProfileVersion05) {
        return result;
    } else if (result.version == trueTypeMaximumProfileVersion10) {
        if (maximumProfileTableEntry->length < trueTypeMaximumProfileTableVersion10Length) {
            MemoryZero(&result, sizeof(result));
            return result;
        }

        result.maxPoints = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;

        result.maxContours = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;

        result.maxComponentPoints = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;

        result.maxComponentContours = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;

        result.maxZones = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;

        result.maxTwilightPoints = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;

        result.maxStorage = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;

        result.maxFunctionDefs = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;

        result.maxInstructionDefs = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;

        result.maxStackElements = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;

        result.maxSizeOfInstructions = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;

        result.maxComponentElements = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;

        result.maxComponentDepth = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;
    } else {
        MemoryZero(&result, sizeof(result));

        return result;
    }

    return result;
}

TrueTypeCmapFormat4 TrueTypeCmapTableParseFormat4(const TrueTypeTableDirectoryEntry *cmapTableEntry) {
    TrueTypeCmapFormat4 result;
    MemoryZero(&result, sizeof(result));

    if (!cmapTableEntry || !cmapTableEntry->memory) {
        return result;
    }

    const usize TrueTypeCmapHeaderLength = 4;
    if (cmapTableEntry->length < TrueTypeCmapHeaderLength) {
        return result;
    }

    const u8 *tableDataPointer = cmapTableEntry->memory;
    usize currentReadOffset = 0;

    u16 version = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    if (version != 0) {
        return result;
    }

    u16 numberSubtables = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    const usize TrueTypeCmapEncodingRecordLength = 8;
    if (cmapTableEntry->length < TrueTypeCmapHeaderLength + (numberSubtables * TrueTypeCmapEncodingRecordLength)) {
        return result;
    }

    u32 selectedSubtableOffset = 0;

    for (u16 subtableIndex = 0; subtableIndex < numberSubtables; subtableIndex++) {
        u16 platformID = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;

        u16 platformSpecificID = ReadUInt16BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 2;

        u32 subtableOffset = ReadUInt32BigEndian(&tableDataPointer[currentReadOffset]);
        currentReadOffset += 4;

        if (subtableOffset > cmapTableEntry->length || subtableOffset + 2 > cmapTableEntry->length) {
            continue;
        }

        bool isWindowsBMP = (platformID == 3 && platformSpecificID == 1);
        bool isUnicodeBMP = (platformID == 0 && (platformSpecificID == 3 || platformSpecificID == 4));

        if (isWindowsBMP || isUnicodeBMP) {
            u16 subtableFormat = ReadUInt16BigEndian(&tableDataPointer[subtableOffset]);
            if (subtableFormat == 4) {
                selectedSubtableOffset = subtableOffset;
                break;
            }
        }
    }

    if (selectedSubtableOffset == 0) {
        return result;
    }

    const u8 *subtablePointer = &tableDataPointer[selectedSubtableOffset];
    usize subtableRemainingLength = cmapTableEntry->length - selectedSubtableOffset;

    const usize TrueTypeCmapFormat4HeaderLength = 14;
    if (subtableRemainingLength < TrueTypeCmapFormat4HeaderLength) {
        return result;
    }

    u16 format = ReadUInt16BigEndian(&subtablePointer[0]);
    if (format != 4) {
        return result;
    }

    u16 length = ReadUInt16BigEndian(&subtablePointer[2]);
    u16 segCountX2 = ReadUInt16BigEndian(&subtablePointer[6]);

    u16 segCount = segCountX2 / 2;

    usize expectedSubtableMinimumLength = TrueTypeCmapFormat4HeaderLength + (segCount * 8) + 2;

    if (length < expectedSubtableMinimumLength || length > subtableRemainingLength) {
        return result;
    }

    result.isValid = true;
    result.segCount = segCount;
    result.memory = subtablePointer;
    result.length = length;

    result.endCodeOffset = TrueTypeCmapFormat4HeaderLength;
    result.startCodeOffset = result.endCodeOffset + segCountX2 + 2;
    result.idDeltaOffset = result.startCodeOffset + segCountX2;
    result.idRangeOffsetOffset = result.idDeltaOffset + segCountX2;

    return result;
}

u32 TrueTypeCmapFormat4GetGlyphIndex(const TrueTypeCmapFormat4 *cmap, u32 characterCode) {
    if (!cmap || !cmap->isValid || !cmap->memory || characterCode > 0xFFFF) {
        return 0;
    }

    const u8 *subtablePointer = cmap->memory;
    u16 targetCharacterCode = (u16)characterCode;

    u16 segmentIndex = 0;
    bool segmentFound = false;

    for (u16 currentIndex = 0; currentIndex < cmap->segCount; currentIndex++) {
        u16 endCode = ReadUInt16BigEndian(&subtablePointer[cmap->endCodeOffset + (currentIndex * 2)]);
        if (endCode >= targetCharacterCode) {
            segmentIndex = currentIndex;
            segmentFound = true;
            break;
        }
    }

    if (!segmentFound) {
        return 0;
    }

    u16 startCode = ReadUInt16BigEndian(&subtablePointer[cmap->startCodeOffset + (segmentIndex * 2)]);
    if (startCode > targetCharacterCode) {
        // NOTE: the character code is greater than the previous segment's endCode but
        // less than this segment's startCode, so the poor guy has no mapping :(
        return 0;
    }

    i16 idDelta = ReadInt16BigEndian(&subtablePointer[cmap->idDeltaOffset + (segmentIndex * 2)]);
    u16 idRangeOffset = ReadUInt16BigEndian(&subtablePointer[cmap->idRangeOffsetOffset + (segmentIndex * 2)]);

    if (idRangeOffset == 0) {
        // NOTE: if idRangeOffset is 0, the mapping adds idDelta to the character code
        return (u32)((targetCharacterCode + idDelta) & 0xFFFF);
    } else {
        // NOTE: if idRangeOffset is non-zero, it relies on the glyphIndexArray
        // glyphIndexAddress = idRangeOffset[i] + 2 * (c - startCode[i]) + (Ptr) &idRangeOffset[i]

        usize idRangeOffsetLocation = cmap->idRangeOffsetOffset + (segmentIndex * 2);
        usize offsetToGlyphIndex = idRangeOffsetLocation + idRangeOffset + (2 * (targetCharacterCode - startCode));

        if (offsetToGlyphIndex + 2 > cmap->length) {
            return 0;
        }

        u16 glyphIndex = ReadUInt16BigEndian(&subtablePointer[offsetToGlyphIndex]);
        if (glyphIndex != 0) {
            return (u32)((glyphIndex + idDelta) & 0xFFFF);
        } else {
            return 0;
        }
    }
}
TrueTypeIndexToLocationTable TrueTypeIndexToLocationTableParse(const TrueTypeTableDirectoryEntry *indexToLocationTableEntry, i16 indexToLocFormat, u16 numGlyphs) {
    TrueTypeIndexToLocationTable result;
    MemoryZero(&result, sizeof(result));

    if (!indexToLocationTableEntry || !indexToLocationTableEntry->memory) {
        return result;
    }
    if (indexToLocFormat != 0 && indexToLocFormat != 1) {
        return result;
    }

    usize expectedEntryCount = (usize)numGlyphs + 1;
    usize expectedMinimumLength = 0;

    if (indexToLocFormat == 0) {
        expectedMinimumLength = expectedEntryCount * 2;
    } else {
        expectedMinimumLength = expectedEntryCount * 4;
    }

    if (indexToLocationTableEntry->length < expectedMinimumLength) {
        return result;
    }

    result.isValid = true;
    result.indexToLocFormat = indexToLocFormat;
    result.numGlyphs = numGlyphs;
    result.memory = indexToLocationTableEntry->memory;
    result.length = indexToLocationTableEntry->length;

    return result;
}

TrueTypeGlyphLocation TrueTypeIndexToLocationGetGlyphLocation(const TrueTypeIndexToLocationTable *loca, u16 glyphIndex) {
    TrueTypeGlyphLocation result;
    MemoryZero(&result, sizeof(result));

    if (!loca || !loca->isValid || !loca->memory) {
        return result;
    }

    if (glyphIndex >= loca->numGlyphs) {
        return result;
    }

    u32 currentGlyphOffset = 0;
    u32 nextGlyphOffset = 0;

    if (loca->indexToLocFormat == 0) {
        usize byteOffset = (usize)glyphIndex * 2;

        u16 rawCurrentOffset = ReadUInt16BigEndian(&loca->memory[byteOffset]);
        u16 rawNextOffset = ReadUInt16BigEndian(&loca->memory[byteOffset + 2]);

        currentGlyphOffset = (u32)rawCurrentOffset * 2;
        nextGlyphOffset = (u32)rawNextOffset * 2;
    } else {
        usize byteOffset = (usize)glyphIndex * 4;

        currentGlyphOffset = ReadUInt32BigEndian(&loca->memory[byteOffset]);
        nextGlyphOffset = ReadUInt32BigEndian(&loca->memory[byteOffset + 4]);
    }

    if (nextGlyphOffset >= currentGlyphOffset) {
        result.offset = currentGlyphOffset;
        result.length = nextGlyphOffset - currentGlyphOffset;
    }

    return result;
}

TrueTypeSimpleGlyph TrueTypeGlyfTableParseSimpleGlyph(MemoryArena *arena, const TrueTypeTableDirectoryEntry *glyfTableEntry, TrueTypeGlyphLocation glyphLocation) {
    TrueTypeSimpleGlyph result;
    MemoryZero(&result, sizeof(result));

    if (!glyfTableEntry || !glyfTableEntry->memory) {
        return result;
    }

    if (glyphLocation.offset > glyfTableEntry->length ||
        glyphLocation.length > (glyfTableEntry->length - glyphLocation.offset)) {
        return result;
    }

    if (glyphLocation.length == 0) {
        result.isValid = true;
        return result;
    }

    const u8 *glyphDataPointer = &glyfTableEntry->memory[glyphLocation.offset];
    usize currentReadOffset = 0;

    const usize TrueTypeGlyphHeaderLength = 10;
    if (glyphLocation.length < TrueTypeGlyphHeaderLength) {
        return result;
    }

    result.numberOfContours = ReadInt16BigEndian(&glyphDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.xMin = ReadInt16BigEndian(&glyphDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.yMin = ReadInt16BigEndian(&glyphDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.xMax = ReadInt16BigEndian(&glyphDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    result.yMax = ReadInt16BigEndian(&glyphDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    if (result.numberOfContours < 0) {
        result.isValid = true;
        result.isCompound = true;

        return result;
    } else if (result.numberOfContours == 0) {
        result.isValid = true;

        return result;
    }

    usize endPointsArrayByteLength = (usize)result.numberOfContours * 2;
    if (currentReadOffset + endPointsArrayByteLength > glyphLocation.length) {
        return result;
    }

    u16 lastPointIndex = ReadUInt16BigEndian(&glyphDataPointer[currentReadOffset + endPointsArrayByteLength - 2]);
    result.numberOfPoints = (u32)lastPointIndex + 1;

    usize contoursAllocationSize = sizeof(u16) * result.numberOfContours;
    usize pointsAllocationSize = sizeof(TrueTypeGlyphPoint) * result.numberOfPoints;
    usize totalAllocationSize = contoursAllocationSize + pointsAllocationSize;

    u8 *allocationBuffer = MemoryArenaPushBytes(arena, totalAllocationSize);
    if (!allocationBuffer) {
        return result;
    }

    result.endPointsOfContours = (u16 *)allocationBuffer;
    result.points = (TrueTypeGlyphPoint *)(allocationBuffer + contoursAllocationSize);

    for (i16 contourIndex = 0; contourIndex < result.numberOfContours; contourIndex++) {
        result.endPointsOfContours[contourIndex] = ReadUInt16BigEndian(&glyphDataPointer[currentReadOffset]);
        currentReadOffset += 2;
    }

    if (currentReadOffset + 2 > glyphLocation.length) {
        MemoryZero(&result, sizeof(TrueTypeSimpleGlyph));

        return result;
    }

    result.instructionLength = ReadUInt16BigEndian(&glyphDataPointer[currentReadOffset]);
    currentReadOffset += 2;

    if (currentReadOffset + result.instructionLength > glyphLocation.length) {
        MemoryZero(&result, sizeof(TrueTypeSimpleGlyph));

        return result;
    }

    result.instructions = &glyphDataPointer[currentReadOffset];
    currentReadOffset += result.instructionLength;

    u8 *unpackedFlags = MemoryArenaPushArray(arena, u8, result.numberOfPoints);
    if (!unpackedFlags) {
        MemoryZero(&result, sizeof(TrueTypeSimpleGlyph));

        return result;
    }

    u32 pointsProcessed = 0;
    while (pointsProcessed < result.numberOfPoints) {
        if (currentReadOffset + 1 > glyphLocation.length) {
            MemoryZero(&result, sizeof(TrueTypeSimpleGlyph));

            return result;
        }

        u8 currentFlag = glyphDataPointer[currentReadOffset++];
        unpackedFlags[pointsProcessed++] = currentFlag;

        if (currentFlag & TrueTypeGlyphFlag_Repeat) {
            if (currentReadOffset + 1 > glyphLocation.length) {
                MemoryZero(&result, sizeof(TrueTypeSimpleGlyph));

                return result;
            }

            u8 repeatCount = glyphDataPointer[currentReadOffset++];

            if (pointsProcessed + repeatCount > result.numberOfPoints) {
                MemoryZero(&result, sizeof(TrueTypeSimpleGlyph));

                return result;
            }

            for (u8 repeatIndex = 0; repeatIndex < repeatCount; repeatIndex++) {
                unpackedFlags[pointsProcessed++] = currentFlag;
            }
        }
    }

    i16 currentX = 0;
    for (u32 pointIndex = 0; pointIndex < result.numberOfPoints; pointIndex++) {
        u8 flag = unpackedFlags[pointIndex];
        result.points[pointIndex].isOnCurve = (flag & TrueTypeGlyphFlag_OnCurve) != 0;

        if (flag & TrueTypeGlyphFlag_XShortVector) {
            if (currentReadOffset + 1 > glyphLocation.length) {
                MemoryZero(&result, sizeof(TrueTypeSimpleGlyph));

                return result;
            }

            u8 deltaX = glyphDataPointer[currentReadOffset++];
            if (flag & TrueTypeGlyphFlag_XIsSameOrPositiveXShort) {
                currentX += deltaX;
            } else {
                currentX -= deltaX;
            }
        } else {
            if (!(flag & TrueTypeGlyphFlag_XIsSameOrPositiveXShort)) {
                if (currentReadOffset + 2 > glyphLocation.length) {
                    MemoryZero(&result, sizeof(TrueTypeSimpleGlyph));

                    return result;
                }

                i16 deltaX = ReadInt16BigEndian(&glyphDataPointer[currentReadOffset]);
                currentReadOffset += 2;
                currentX += deltaX;
            }
        }
        result.points[pointIndex].x = currentX;
    }

    i16 currentY = 0;
    for (u32 pointIndex = 0; pointIndex < result.numberOfPoints; pointIndex++) {
        u8 flag = unpackedFlags[pointIndex];

        if (flag & TrueTypeGlyphFlag_YShortVector) {
            if (currentReadOffset + 1 > glyphLocation.length) {
                MemoryZero(&result, sizeof(TrueTypeSimpleGlyph));

                return result;
            }

            u8 deltaY = glyphDataPointer[currentReadOffset++];
            if (flag & TrueTypeGlyphFlag_YIsSameOrPositiveYShort) {
                currentY += deltaY;
            } else {
                currentY -= deltaY;
            }
        } else {
            if (!(flag & TrueTypeGlyphFlag_YIsSameOrPositiveYShort)) {
                if (currentReadOffset + 2 > glyphLocation.length) {
                    MemoryZero(&result, sizeof(TrueTypeSimpleGlyph));

                    return result;
                }

                i16 deltaY = ReadInt16BigEndian(&glyphDataPointer[currentReadOffset]);
                currentReadOffset += 2;
                currentY += deltaY;
            }
        }
        result.points[pointIndex].y = currentY;
    }

    result.isValid = true;
    return result;
}

// NOTE: Rasterization. I stole and adapted it from here: https://handmade.network/forums/wip/t/7610-reading_ttf_files_and_rasterizing_them_using_a_handmade_approach,_part_2__rasterization
// Love you twin thanks!!!

static void TrueTypeTessellateBezier(Vector2 *outputPoints, u32 *outputSize, Vector2 p0, Vector2 p1, Vector2 p2) {
    const u32 subdivisionCount = 3;
    f32 step = 1.0f / (f32)subdivisionCount;

    for (u32 i = 1; i <= subdivisionCount; i++) {
        f32 t = (f32)i * step;
        f32 t1 = 1.0f - t;
        f32 t2 = t * t;

        f32 x = (t1 * t1 * p0.x) + (2.0f * t1 * t * p1.x) + (t2 * p2.x);
        f32 y = (t1 * t1 * p0.y) + (2.0f * t1 * t * p1.y) + (t2 * p2.y);

        outputPoints[*outputSize] = V2(x, y);
        (*outputSize)++;
    }
}

Image TrueTypeGlyphRasterize(MemoryArena *arena, const TrueTypeSimpleGlyph *glyph, f32 scale) {
    Image result;
    MemoryZero(&result, sizeof(result));

    if (!glyph || !glyph->isValid || glyph->isCompound || glyph->numberOfContours <= 0 || scale <= 0.0f) {
        return result;
    }

    if (glyph->yMax <= glyph->yMin || glyph->xMax <= glyph->xMin) {
        return result;
    }

    u32 targetPixelWidth = (u32)(((f32)(glyph->xMax - glyph->xMin) * scale) + 0.999f);
    u32 targetPixelHeight = (u32)(((f32)(glyph->yMax - glyph->yMin) * scale) + 0.999f);
    if (targetPixelWidth == 0) {
        targetPixelWidth = 1;
    }

    if (targetPixelHeight == 0) {
        targetPixelHeight = 1;
    }

    result.size.x = targetPixelWidth;
    result.size.y = targetPixelHeight;
    result.bytesPerPixel = 4;

    usize pixelsAllocationSize = (usize)result.size.x * (usize)result.size.y * result.bytesPerPixel;
    result.pixels = MemoryArenaPushBytes(arena, pixelsAllocationSize);

    if (!result.pixels) {
        return result;
    }

    u32 maxGeneratedPoints = glyph->numberOfPoints * 10;

    Vector2 *generatedPoints = MemoryArenaPushArray(arena, Vector2, maxGeneratedPoints);
    if (!generatedPoints) {
        return result;
    }

    u32 *contourEndIndices = MemoryArenaPushArray(arena, u32, glyph->numberOfContours);
    if (!contourEndIndices) {
        return result;
    }

    u32 generatedPointCount = 0;
    u32 currentPointIndex = 0;

    for (i16 contourIndex = 0; contourIndex < glyph->numberOfContours; contourIndex++) {
        u32 contourStartIndex = currentPointIndex;
        u32 contourEndIndex = glyph->endPointsOfContours[contourIndex];
        u32 contourLength = contourEndIndex - contourStartIndex + 1;

        Vector2 startAnchor;
        bool firstIsOnCurve = glyph->points[contourStartIndex].isOnCurve;
        bool lastIsOnCurve = glyph->points[contourEndIndex].isOnCurve;

        if (firstIsOnCurve) {
            startAnchor = V2((f32)glyph->points[contourStartIndex].x, (f32)glyph->points[contourStartIndex].y);
        } else if (lastIsOnCurve) {
            startAnchor = V2((f32)glyph->points[contourEndIndex].x, (f32)glyph->points[contourEndIndex].y);
        } else {
            startAnchor = V2(
                (f32)(glyph->points[contourStartIndex].x + glyph->points[contourEndIndex].x) / 2.0f,
                (f32)(glyph->points[contourStartIndex].y + glyph->points[contourEndIndex].y) / 2.0f);
        }

        generatedPoints[generatedPointCount++] = startAnchor;

        for (u32 i = 0; i <= contourLength; i++) {
            u32 currentIndex = contourStartIndex + (i % contourLength);
            u32 nextIndex = contourStartIndex + ((i + 1) % contourLength);

            bool currentIsOnCurve = glyph->points[currentIndex].isOnCurve;
            bool nextIsOnCurve = glyph->points[nextIndex].isOnCurve;

            Vector2 pointCurrent = V2((f32)glyph->points[currentIndex].x, (f32)glyph->points[currentIndex].y);
            Vector2 pointNext = V2((f32)glyph->points[nextIndex].x, (f32)glyph->points[nextIndex].y);

            if (currentIsOnCurve) {
                generatedPoints[generatedPointCount++] = pointCurrent;
            } else {
                Vector2 p0 = generatedPoints[generatedPointCount - 1];
                Vector2 p1 = pointCurrent;
                Vector2 p2;

                if (nextIsOnCurve) {
                    p2 = pointNext;
                } else {
                    p2 = V2((pointCurrent.x + pointNext.x) / 2.0f, (pointCurrent.y + pointNext.y) / 2.0f);
                }

                u32 addedPoints = 0;
                TrueTypeTessellateBezier(generatedPoints + generatedPointCount, &addedPoints, p0, p1, p2);
                generatedPointCount += addedPoints;
            }
        }

        generatedPoints[generatedPointCount++] = startAnchor;

        contourEndIndices[contourIndex] = generatedPointCount;
        currentPointIndex = contourEndIndex + 1;
    }

    for (u32 i = 0; i < generatedPointCount; i++) {
        generatedPoints[i].x = (generatedPoints[i].x - (f32)glyph->xMin) * scale;
        generatedPoints[i].y = ((f32)glyph->yMax - generatedPoints[i].y) * scale;
    }

    TrueTypeEdge *edges = MemoryArenaPushArray(arena, TrueTypeEdge, generatedPointCount);
    u32 edgeCount = 0;
    u32 readPointIndex = 0;

    for (i16 contourIndex = 0; contourIndex < glyph->numberOfContours; contourIndex++) {
        for (; readPointIndex < contourEndIndices[contourIndex] - 1; readPointIndex++) {
            edges[edgeCount].p1 = generatedPoints[readPointIndex];
            edges[edgeCount].p2 = generatedPoints[readPointIndex + 1];
            edgeCount++;
        }

        readPointIndex++;
    }

    u8 *coverageBuffer = MemoryArenaPushBytes(arena, result.size.x * result.size.y);

    const u32 scanlineSubdivisions = 5;
    f32 alphaWeight = 255.0f / (f32)scanlineSubdivisions;
    f32 stepPerScanline = 1.0f / (f32)scanlineSubdivisions;

    const u32 maxIntersections = 256;
    TrueTypeScanlineIntersection intersections[256];

    for (u32 y = 0; y < result.size.y; y++) {
        for (u32 subY = 0; subY < scanlineSubdivisions; subY++) {
            u32 intersectionCount = 0;
            f32 scanline = (f32)y + (f32)subY * stepPerScanline;

            for (u32 e = 0; e < edgeCount; e++) {
                TrueTypeEdge *edge = &edges[e];

                f32 biggerY = MAX(edge->p1.y, edge->p2.y);
                f32 smallerY = MIN(edge->p1.y, edge->p2.y);

                if (scanline <= smallerY || scanline > biggerY) {
                    continue;
                }

                f32 deltaX = edge->p2.x - edge->p1.x;
                f32 deltaY = edge->p2.y - edge->p1.y;

                if (deltaY == 0.0f) {
                    continue;
                }

                i32 direction = (deltaY > 0.0f) ? 1 : -1;

                f32 intersectionX = 0.0f;
                if (deltaX == 0.0f) {
                    intersectionX = edge->p1.x;
                } else {
                    intersectionX = (scanline - edge->p1.y) * (deltaX / deltaY) + edge->p1.x;

                    if (intersectionX < 0.0f) {
                        intersectionX = edge->p1.x;
                    }
                }

                if (intersectionCount < maxIntersections) {
                    intersections[intersectionCount].x = intersectionX;
                    intersections[intersectionCount].direction = direction;
                    intersectionCount++;
                }
            }

            for (u32 i = 1; i < intersectionCount; i++) {
                TrueTypeScanlineIntersection key = intersections[i];
                i32 j = (i32)i - 1;

                while (j >= 0 && intersections[j].x > key.x) {
                    intersections[j + 1] = intersections[j];
                    j--;
                }

                intersections[j + 1] = key;
            }

            i32 windingNumber = 0;
            f32 startX = 0.0f;

            for (u32 m = 0; m < intersectionCount; m++) {
                i32 nextWindingNumber = windingNumber + intersections[m].direction;

                if (windingNumber == 0 && nextWindingNumber != 0) {
                    startX = intersections[m].x;
                } else if (windingNumber != 0 && nextWindingNumber == 0) {
                    f32 endX = intersections[m].x;

                    if (endX > startX) {
                        i32 startIndex = (i32)startX;
                        f32 startCovered = ((f32)(startIndex + 1)) - startX;

                        i32 endIndex = (i32)endX;
                        f32 endCovered = endX - (f32)endIndex;

                        if (startIndex < 0) {
                            startIndex = 0;
                            startCovered = 1.0f;
                        }
                        if (endIndex >= (i32)result.size.x) {
                            endIndex = result.size.x - 1;
                            endCovered = 1.0f;
                        }

                        if (startIndex < (i32)result.size.x && endIndex >= 0) {
                            usize bufferIndexY = (usize)y * result.size.x;

                            if (startIndex == endIndex) {
                                f32 value = (f32)coverageBuffer[startIndex + bufferIndexY] + (alphaWeight * (startCovered + endCovered - 1.0f));
                                coverageBuffer[startIndex + bufferIndexY] = (u8)MIN(value, 255.0f);
                            } else {
                                f32 valueStart = (f32)coverageBuffer[startIndex + bufferIndexY] + (alphaWeight * startCovered);
                                coverageBuffer[startIndex + bufferIndexY] = (u8)MIN(valueStart, 255.0f);

                                f32 valueEnd = (f32)coverageBuffer[endIndex + bufferIndexY] + (alphaWeight * endCovered);
                                coverageBuffer[endIndex + bufferIndexY] = (u8)MIN(valueEnd, 255.0f);

                                for (i32 j = startIndex + 1; j < endIndex; j++) {
                                    f32 val = (f32)coverageBuffer[j + bufferIndexY] + alphaWeight;
                                    coverageBuffer[j + bufferIndexY] = (u8)MIN(val, 255.0f);
                                }
                            }
                        }
                    }
                }

                windingNumber = nextWindingNumber;
            }
        }
    }

    for (u32 i = 0; i < result.size.x * result.size.y; i++) {
        u8 alpha = coverageBuffer[i];

        result.pixels[i * 4 + 0] = 255;
        result.pixels[i * 4 + 1] = 255;
        result.pixels[i * 4 + 2] = 255;
        result.pixels[i * 4 + 3] = alpha;
    }

    return result;
}

Image TrueTypeFontBakeAtlas(MemoryArena *permanentArena, MemoryArena *temporaryArena, const TrueTypeFont *font, u32 targetPixelHeight, u32 atlasWidth, u32 atlasHeight, u32 firstCharacter, u32 characterCount, TrueTypeBakedGlyph *outGlyphs) {
    Image result;
    MemoryZero(&result, sizeof(result));

    if (!font || atlasWidth == 0 || atlasHeight == 0 || characterCount == 0 || !outGlyphs) {
        return result;
    }

    MemoryZero(outGlyphs, sizeof(TrueTypeBakedGlyph) * characterCount);

    const TrueTypeTableDirectoryEntry *headEntry = TrueTypeFontGetTable(font, TrueTypeTableTag_HEAD);
    const TrueTypeTableDirectoryEntry *maxpEntry = TrueTypeFontGetTable(font, TrueTypeTableTag_MAXP);
    const TrueTypeTableDirectoryEntry *cmapEntry = TrueTypeFontGetTable(font, TrueTypeTableTag_CMAP);
    const TrueTypeTableDirectoryEntry *locaEntry = TrueTypeFontGetTable(font, TrueTypeTableTag_LOCA);
    const TrueTypeTableDirectoryEntry *glyfEntry = TrueTypeFontGetTable(font, TrueTypeTableTag_GLYF);

    if (!headEntry || !maxpEntry || !cmapEntry || !locaEntry || !glyfEntry) {
        return result;
    }

    TrueTypeHeadTable head = TrueTypeHeadTableParse(headEntry);
    TrueTypeMaximumProfileTable maxp = TrueTypeMaximumProfileTableParse(maxpEntry);
    TrueTypeCmapFormat4 cmap = TrueTypeCmapTableParseFormat4(cmapEntry);
    TrueTypeIndexToLocationTable loca = TrueTypeIndexToLocationTableParse(locaEntry, head.indexToLocFormat, maxp.numGlyphs);

    if (!cmap.isValid || !loca.isValid) {
        return result;
    }

    f32 scale = (f32)targetPixelHeight / (f32)head.unitsPerEm;

    result.size.x = atlasWidth;
    result.size.y = atlasHeight;
    result.bytesPerPixel = 4;

    usize pixelsAllocationSize = (usize)result.size.x * (usize)result.size.y * result.bytesPerPixel;
    result.pixels = MemoryArenaPushBytes(permanentArena, pixelsAllocationSize);

    if (!result.pixels) {
        MemoryZero(&result, sizeof(result));

        return result;
    }

    Pack2D atlasPack2D = Pack2DCreate(temporaryArena, atlasWidth, atlasHeight);
    const u32 padding = 1;

    for (u32 index = 0; index < characterCount; index++) {
        MemoryArenaCheckpoint glyphCheckpoint = MemoryArenaCreateCheckpoint(temporaryArena);

        u32 characterCode = firstCharacter + index;
        outGlyphs[index].characterCode = characterCode;

        u32 glyphIndex = TrueTypeCmapFormat4GetGlyphIndex(&cmap, characterCode);
        if (glyphIndex == 0) {
            continue;
        }

        TrueTypeGlyphLocation glyphLocation = TrueTypeIndexToLocationGetGlyphLocation(&loca, (u16)glyphIndex);
        TrueTypeSimpleGlyph glyph = TrueTypeGlyfTableParseSimpleGlyph(temporaryArena, glyfEntry, glyphLocation);

        if (!glyph.isValid) {
            MemoryArenaRestoreCheckpoint(glyphCheckpoint);

            continue;
        }

        Image image = TrueTypeGlyphRasterize(temporaryArena, &glyph, scale);

        if (image.pixels && image.size.x > 0 && image.size.y > 0) {
            u32 positionX = 0;
            u32 positionY = 0;

            bool isPacked = Pack2DAdd(&atlasPack2D, image.size.x + padding, image.size.y + padding, &positionX, &positionY);

            if (isPacked) {
                for (u32 y = 0; y < (u32)image.size.y; y++) {
                    for (u32 x = 0; x < (u32)image.size.x; x++) {
                        u32 sourceIndex = (y * (u32)image.size.x + x) * result.bytesPerPixel;
                        u32 destinationIndex = ((positionY + y) * atlasWidth + (positionX + x)) * result.bytesPerPixel;

                        result.pixels[destinationIndex + 0] = image.pixels[sourceIndex + 0];
                        result.pixels[destinationIndex + 1] = image.pixels[sourceIndex + 1];
                        result.pixels[destinationIndex + 2] = image.pixels[sourceIndex + 2];
                        result.pixels[destinationIndex + 3] = image.pixels[sourceIndex + 3];
                    }
                }

                outGlyphs[index].isValid = true;
                outGlyphs[index].uvMin.x = (f32)positionX / (f32)atlasWidth;
                outGlyphs[index].uvMin.y = (f32)positionY / (f32)atlasHeight;
                outGlyphs[index].uvMax.x = (f32)(positionX + image.size.x) / (f32)atlasWidth;
                outGlyphs[index].uvMax.y = (f32)(positionY + image.size.y) / (f32)atlasHeight;
                outGlyphs[index].size.x = (f32)image.size.x;
                outGlyphs[index].size.y = (f32)image.size.y;
                outGlyphs[index].offset.x = (f32)glyph.xMin * scale;
                outGlyphs[index].offset.y = (f32)glyph.yMax * scale;
            }
        }

        MemoryArenaRestoreCheckpoint(glyphCheckpoint);
    }

    return result;
}
