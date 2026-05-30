#include "game_ttf.h"
#include "game_png.h"
#include "game_types.h"
#include "game_platform.h"
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
    u16 MaxPoints;
    u16 MaxContours;
    u16 MaxComponentPoints;
    u16 MaxComponentContours;
    u16 MaxZones;
    u16 MaxTwilightPoints;
    u16 MaxStorage;
    u16 MaxFunctionDefs;
    u16 MaxInstructionDefs;
    u16 MaxStackElements;
    u16 MaxSizeOfInstructions;
    u16 MaxComponentElements;
    u16 MaxComponentDepth;
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
    TrueTypeTableTag_MaxP = FOURCC('m', 'a', 'x', 'p'),
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
    usize offset = 0;

    for (u32 index = 0; index < numberOfLongs; index++) {
        u8 byte0 = (offset < tableLength) ? tableMemory[offset++] : 0;
        u8 byte1 = (offset < tableLength) ? tableMemory[offset++] : 0;
        u8 byte2 = (offset < tableLength) ? tableMemory[offset++] : 0;
        u8 byte3 = (offset < tableLength) ? tableMemory[offset++] : 0;

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
    ZeroStruct(result);

    if (!memory) {
        return result;
    }

    const usize trueTypeOffsetSubtableLength = 12;
    if (length < trueTypeOffsetSubtableLength) {
        return result;
    }

    BinaryStream stream;
    BinaryStreamInitialize(&stream, memory, length);

    u32 scalerTypeValue = BinaryStreamReadUInt32BigEndian(&stream);

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

    result.numberOfTables = BinaryStreamReadUInt16BigEndian(&stream);
    result.searchRange = BinaryStreamReadUInt16BigEndian(&stream);
    result.entrySelector = BinaryStreamReadUInt16BigEndian(&stream);
    result.rangeShift = BinaryStreamReadUInt16BigEndian(&stream);

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
            const u8 *entryPointer = &stream.memory[stream.offset];
            stream.offset += trueTypeTableDirectoryEntryLength;

            u32 tag = ReadUInt32BigEndian(&entryPointer[0]);
            u32 expectedChecksum = ReadUInt32BigEndian(&entryPointer[4]);
            u32 tableOffset = ReadUInt32BigEndian(&entryPointer[8]);
            u32 tableLength = ReadUInt32BigEndian(&entryPointer[12]);

            if (tableOffset > length || tableLength > (length - tableOffset)) {
                ReturnZeroed(result);
            }

            const u8 *tableDataPointer = &stream.memory[tableOffset];
            bool isHeadTable = (tag == TrueTypeTableTag_HEAD);

            u32 calculatedChecksum = TrueTypeCalculateTableChecksum(tableDataPointer, tableLength, isHeadTable);

            if (calculatedChecksum != expectedChecksum) {
                ReturnZeroed(result);
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
    ZeroStruct(result);

    if (!headTableEntry || !headTableEntry->memory) {
        return result;
    }

    const usize trueTypeHeadTableExpectedLength = 54;
    if (headTableEntry->length < trueTypeHeadTableExpectedLength) {
        return result;
    }

    BinaryStream stream;
    BinaryStreamInitialize(&stream, headTableEntry->memory, headTableEntry->length);

    result.version = BinaryStreamReadUInt32BigEndian(&stream);
    result.fontRevision = BinaryStreamReadUInt32BigEndian(&stream);
    result.checkSumAdjustment = BinaryStreamReadUInt32BigEndian(&stream);
    result.magicNumber = BinaryStreamReadUInt32BigEndian(&stream);

    const u32 trueTypeHeadTableMagicNumber = 0x5F0F3CF5;
    if (result.magicNumber != trueTypeHeadTableMagicNumber) {
        ReturnZeroed(result);
    }

    result.flags = BinaryStreamReadUInt16BigEndian(&stream);
    result.unitsPerEm = BinaryStreamReadUInt16BigEndian(&stream);

    if (result.unitsPerEm < 64 || result.unitsPerEm > 16384) {
        ReturnZeroed(result);
    }

    result.created = BinaryStreamReadInt64BigEndian(&stream);
    result.modified = BinaryStreamReadInt64BigEndian(&stream);
    result.xMin = BinaryStreamReadInt16BigEndian(&stream);
    result.yMin = BinaryStreamReadInt16BigEndian(&stream);
    result.xMax = BinaryStreamReadInt16BigEndian(&stream);
    result.yMax = BinaryStreamReadInt16BigEndian(&stream);
    result.macStyle = BinaryStreamReadUInt16BigEndian(&stream);
    result.lowestRecPPEM = BinaryStreamReadUInt16BigEndian(&stream);
    result.fontDirectionHint = BinaryStreamReadInt16BigEndian(&stream);
    result.indexToLocFormat = BinaryStreamReadInt16BigEndian(&stream);
    result.glyphDataFormat = BinaryStreamReadInt16BigEndian(&stream);

    return result;
}

TrueTypeMaximumProfileTable TrueTypeMaximumProfileTableParse(const TrueTypeTableDirectoryEntry *MaximumProfileTableEntry) {
    TrueTypeMaximumProfileTable result;
    ZeroStruct(result);

    if (!MaximumProfileTableEntry || !MaximumProfileTableEntry->memory) {
        return result;
    }

    const usize trueTypeMaximumProfileTableVersion05Length = 6;
    const usize trueTypeMaximumProfileTableVersion10Length = 32;

    if (MaximumProfileTableEntry->length < trueTypeMaximumProfileTableVersion05Length) {
        return result;
    }

    BinaryStream stream;
    BinaryStreamInitialize(&stream, MaximumProfileTableEntry->memory, MaximumProfileTableEntry->length);

    result.version = BinaryStreamReadUInt32BigEndian(&stream);
    result.numGlyphs = BinaryStreamReadUInt16BigEndian(&stream);

    const u32 trueTypeMaximumProfileVersion05 = 0x00005000;
    const u32 trueTypeMaximumProfileVersion10 = 0x00010000;

    if (result.version == trueTypeMaximumProfileVersion05) {
        return result;
    } else if (result.version == trueTypeMaximumProfileVersion10) {
        if (MaximumProfileTableEntry->length < trueTypeMaximumProfileTableVersion10Length) {
            ReturnZeroed(result);
        }

        result.MaxPoints = BinaryStreamReadUInt16BigEndian(&stream);
        result.MaxContours = BinaryStreamReadUInt16BigEndian(&stream);
        result.MaxComponentPoints = BinaryStreamReadUInt16BigEndian(&stream);
        result.MaxComponentContours = BinaryStreamReadUInt16BigEndian(&stream);
        result.MaxZones = BinaryStreamReadUInt16BigEndian(&stream);
        result.MaxTwilightPoints = BinaryStreamReadUInt16BigEndian(&stream);
        result.MaxStorage = BinaryStreamReadUInt16BigEndian(&stream);
        result.MaxFunctionDefs = BinaryStreamReadUInt16BigEndian(&stream);
        result.MaxInstructionDefs = BinaryStreamReadUInt16BigEndian(&stream);
        result.MaxStackElements = BinaryStreamReadUInt16BigEndian(&stream);
        result.MaxSizeOfInstructions = BinaryStreamReadUInt16BigEndian(&stream);
        result.MaxComponentElements = BinaryStreamReadUInt16BigEndian(&stream);
        result.MaxComponentDepth = BinaryStreamReadUInt16BigEndian(&stream);
    } else {
        ReturnZeroed(result);
    }

    return result;
}

TrueTypeCmapFormat4 TrueTypeCmapTableParseFormat4(const TrueTypeTableDirectoryEntry *cmapTableEntry) {
    TrueTypeCmapFormat4 result;
    ZeroStruct(result);

    if (!cmapTableEntry || !cmapTableEntry->memory) {
        return result;
    }

    const usize TrueTypeCmapHeaderLength = 4;
    if (cmapTableEntry->length < TrueTypeCmapHeaderLength) {
        return result;
    }

    BinaryStream stream;
    BinaryStreamInitialize(&stream, cmapTableEntry->memory, cmapTableEntry->length);

    u16 version = BinaryStreamReadUInt16BigEndian(&stream);
    if (version != 0) {
        return result;
    }

    u16 numberSubtables = BinaryStreamReadUInt16BigEndian(&stream);

    const usize TrueTypeCmapEncodingRecordLength = 8;
    if (cmapTableEntry->length < TrueTypeCmapHeaderLength + (numberSubtables * TrueTypeCmapEncodingRecordLength)) {
        return result;
    }

    u32 selectedSubtableOffset = 0;

    for (u16 subtableIndex = 0; subtableIndex < numberSubtables; subtableIndex++) {
        u16 platformID = BinaryStreamReadUInt16BigEndian(&stream);
        u16 platformSpecificID = BinaryStreamReadUInt16BigEndian(&stream);
        u32 subtableOffset = BinaryStreamReadUInt32BigEndian(&stream);

        if (subtableOffset > cmapTableEntry->length || subtableOffset + 2 > cmapTableEntry->length) {
            continue;
        }

        bool isWindowsBMP = (platformID == 3 && platformSpecificID == 1);
        bool isUnicodeBMP = (platformID == 0 && (platformSpecificID == 3 || platformSpecificID == 4));

        if (isWindowsBMP || isUnicodeBMP) {
            u16 subtableFormat = ReadUInt16BigEndian(&stream.memory[subtableOffset]);
            if (subtableFormat == 4) {
                selectedSubtableOffset = subtableOffset;
                break;
            }
        }
    }

    if (selectedSubtableOffset == 0) {
        return result;
    }

    const u8 *subtablePointer = &stream.memory[selectedSubtableOffset];
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
    ZeroStruct(result);

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
    ZeroStruct(result);

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
    ZeroStruct(result);

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

    BinaryStream stream;
    BinaryStreamInitialize(&stream, &glyfTableEntry->memory[glyphLocation.offset], glyphLocation.length);

    const usize TrueTypeGlyphHeaderLength = 10;
    if (glyphLocation.length < TrueTypeGlyphHeaderLength) {
        return result;
    }

    result.numberOfContours = BinaryStreamReadInt16BigEndian(&stream);

    result.xMin = BinaryStreamReadInt16BigEndian(&stream);
    result.yMin = BinaryStreamReadInt16BigEndian(&stream);
    result.xMax = BinaryStreamReadInt16BigEndian(&stream);
    result.yMax = BinaryStreamReadInt16BigEndian(&stream);

    if (result.numberOfContours < 0) {
        result.isValid = true;
        result.isCompound = true;

        return result;
    } else if (result.numberOfContours == 0) {
        result.isValid = true;

        return result;
    }

    usize endPointsArrayByteLength = (usize)result.numberOfContours * 2;
    if (stream.offset + endPointsArrayByteLength > glyphLocation.length) {
        return result;
    }

    u16 lastPointIndex = ReadUInt16BigEndian(&stream.memory[stream.offset + endPointsArrayByteLength - 2]);
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
        result.endPointsOfContours[contourIndex] = BinaryStreamReadUInt16BigEndian(&stream);
    }

    if (stream.offset + 2 > glyphLocation.length) {
        ReturnZeroed(result);
    }

    result.instructionLength = BinaryStreamReadUInt16BigEndian(&stream);

    if (stream.offset + result.instructionLength > glyphLocation.length) {
        ReturnZeroed(result);
    }

    result.instructions = &stream.memory[stream.offset];
    stream.offset += result.instructionLength;

    u8 *unpackedFlags = MemoryArenaPushArray(arena, u8, result.numberOfPoints);
    if (!unpackedFlags) {
        ReturnZeroed(result);
    }

    u32 pointsProcessed = 0;
    while (pointsProcessed < result.numberOfPoints) {
        if (stream.offset + 1 > glyphLocation.length) {
            ReturnZeroed(result);
        }

        u8 currentFlag = stream.memory[stream.offset++];
        unpackedFlags[pointsProcessed++] = currentFlag;

        if (IsBitSet(currentFlag, TrueTypeGlyphFlag_Repeat)) {
            if (stream.offset + 1 > glyphLocation.length) {
                ReturnZeroed(result);
            }

            u8 repeatCount = stream.memory[stream.offset++];

            if (pointsProcessed + repeatCount > result.numberOfPoints) {
                ReturnZeroed(result);
            }

            for (u8 repeatIndex = 0; repeatIndex < repeatCount; repeatIndex++) {
                unpackedFlags[pointsProcessed++] = currentFlag;
            }
        }
    }

    i16 currentX = 0;
    for (u32 pointIndex = 0; pointIndex < result.numberOfPoints; pointIndex++) {
        u8 flag = unpackedFlags[pointIndex];
        result.points[pointIndex].isOnCurve = IsBitSet(flag, TrueTypeGlyphFlag_OnCurve);

        if (IsBitSet(flag, TrueTypeGlyphFlag_XShortVector)) {
            if (stream.offset + 1 > glyphLocation.length) {
                ReturnZeroed(result);
            }

            u8 deltaX = stream.memory[stream.offset++];
            if (IsBitSet(flag, TrueTypeGlyphFlag_XIsSameOrPositiveXShort)) {
                currentX += deltaX;
            } else {
                currentX -= deltaX;
            }
        } else {
            if (!IsBitSet(flag, TrueTypeGlyphFlag_XIsSameOrPositiveXShort)) {
                if (stream.offset + 2 > glyphLocation.length) {
                    ReturnZeroed(result);
                }

                i16 deltaX = BinaryStreamReadInt16BigEndian(&stream);
                currentX += deltaX;
            }
        }
        result.points[pointIndex].x = currentX;
    }

    i16 currentY = 0;
    for (u32 pointIndex = 0; pointIndex < result.numberOfPoints; pointIndex++) {
        u8 flag = unpackedFlags[pointIndex];

        if (IsBitSet(flag, TrueTypeGlyphFlag_YShortVector)) {
            if (stream.offset + 1 > glyphLocation.length) {
                ReturnZeroed(result);
            }

            u8 deltaY = stream.memory[stream.offset++];
            if (IsBitSet(flag, TrueTypeGlyphFlag_YIsSameOrPositiveYShort)) {
                currentY += deltaY;
            } else {
                currentY -= deltaY;
            }
        } else {
            if (!IsBitSet(flag, TrueTypeGlyphFlag_YIsSameOrPositiveYShort)) {
                if (stream.offset + 2 > glyphLocation.length) {
                    ReturnZeroed(result);
                }

                i16 deltaY = BinaryStreamReadInt16BigEndian(&stream);
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
    ZeroStruct(result);

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

    u32 MaxGeneratedPoints = glyph->numberOfPoints * 10;

    Vector2 *generatedPoints = MemoryArenaPushArray(arena, Vector2, MaxGeneratedPoints);
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

    const u32 MaxIntersections = 256;
    TrueTypeScanlineIntersection intersections[256];

    for (u32 y = 0; y < result.size.y; y++) {
        for (u32 subY = 0; subY < scanlineSubdivisions; subY++) {
            u32 intersectionCount = 0;
            f32 scanline = (f32)y + (f32)subY * stepPerScanline;

            for (u32 e = 0; e < edgeCount; e++) {
                TrueTypeEdge *edge = &edges[e];

                f32 biggerY = Max(edge->p1.y, edge->p2.y);
                f32 smallerY = Min(edge->p1.y, edge->p2.y);

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

                if (intersectionCount < MaxIntersections) {
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
                                coverageBuffer[startIndex + bufferIndexY] = (u8)Min(value, 255.0f);
                            } else {
                                f32 valueStart = (f32)coverageBuffer[startIndex + bufferIndexY] + (alphaWeight * startCovered);
                                coverageBuffer[startIndex + bufferIndexY] = (u8)Min(valueStart, 255.0f);

                                f32 valueEnd = (f32)coverageBuffer[endIndex + bufferIndexY] + (alphaWeight * endCovered);
                                coverageBuffer[endIndex + bufferIndexY] = (u8)Min(valueEnd, 255.0f);

                                for (i32 j = startIndex + 1; j < endIndex; j++) {
                                    f32 val = (f32)coverageBuffer[j + bufferIndexY] + alphaWeight;
                                    coverageBuffer[j + bufferIndexY] = (u8)Min(val, 255.0f);
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
    ZeroStruct(result);

    if (!font || atlasWidth == 0 || atlasHeight == 0 || characterCount == 0 || !outGlyphs) {
        return result;
    }

    ZeroItems(outGlyphs, characterCount);

    const TrueTypeTableDirectoryEntry *headEntry = TrueTypeFontGetTable(font, TrueTypeTableTag_HEAD);
    const TrueTypeTableDirectoryEntry *MaxpEntry = TrueTypeFontGetTable(font, TrueTypeTableTag_MaxP);
    const TrueTypeTableDirectoryEntry *cmapEntry = TrueTypeFontGetTable(font, TrueTypeTableTag_CMAP);
    const TrueTypeTableDirectoryEntry *locaEntry = TrueTypeFontGetTable(font, TrueTypeTableTag_LOCA);
    const TrueTypeTableDirectoryEntry *glyfEntry = TrueTypeFontGetTable(font, TrueTypeTableTag_GLYF);

    if (!headEntry || !MaxpEntry || !cmapEntry || !locaEntry || !glyfEntry) {
        return result;
    }

    TrueTypeHeadTable head = TrueTypeHeadTableParse(headEntry);
    TrueTypeMaximumProfileTable Maxp = TrueTypeMaximumProfileTableParse(MaxpEntry);
    TrueTypeCmapFormat4 cmap = TrueTypeCmapTableParseFormat4(cmapEntry);
    TrueTypeIndexToLocationTable loca = TrueTypeIndexToLocationTableParse(locaEntry, head.indexToLocFormat, Maxp.numGlyphs);

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
        ReturnZeroed(result);
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

                        MemoryCopyForwards(&result.pixels[destinationIndex], &image.pixels[sourceIndex], 4);
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
