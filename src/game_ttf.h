#pragma once

#include "game_platform.h"
#include "game_png.h"

typedef enum {
    TrueTypeScalerTypeUnknown = 0,
    TrueTypeScalerTypeWindows,
    TrueTypeScalerTypeMac,
    TrueTypeScalerTypePostScript,
    TrueTypeScalerTypeOpenType
} TrueTypeScalerType;

// NOTE: Table tags
enum {
    TrueTypeTableTag_HEAD = FOURCC('h', 'e', 'a', 'd'),
    TrueTypeTableTag_MAXP = FOURCC('m', 'a', 'x', 'p'),
    TrueTypeTableTag_CMAP = FOURCC('c', 'm', 'a', 'p'),
    TrueTypeTableTag_LOCA = FOURCC('l', 'o', 'c', 'a'),
    TrueTypeTableTag_GLYF = FOURCC('g', 'l', 'y', 'f'),
};

typedef struct {
    u32 tag;
    u32 checkSum;
    u32 offset;
    u32 length;
    const u8 *memory;
} TrueTypeTableDirectoryEntry;

typedef struct {
    TrueTypeScalerType scalerType;
    u16 numberOfTables;
    u16 searchRange;
    u16 entrySelector;
    u16 rangeShift;

    TrueTypeTableDirectoryEntry *tables;
} TrueTypeFont;

TrueTypeFont TrueTypeFontLoadFromMemory(const void *memory, usize length);
const TrueTypeTableDirectoryEntry *TrueTypeFontGetTable(const TrueTypeFont *font, u32 tableTag);

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

TrueTypeHeadTable TrueTypeHeadTableParse(const TrueTypeTableDirectoryEntry *headTableEntry);

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

TrueTypeMaximumProfileTable TrueTypeMaximumProfileTableParse(const TrueTypeTableDirectoryEntry *maximumProfileTableEntry);

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

TrueTypeCmapFormat4 TrueTypeCmapTableParseFormat4(const TrueTypeTableDirectoryEntry *cmapTableEntry);
u32 TrueTypeCmapFormat4GetGlyphIndex(const TrueTypeCmapFormat4 *cmap, u32 characterCode);

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

TrueTypeIndexToLocationTable TrueTypeIndexToLocationTableParse(const TrueTypeTableDirectoryEntry *indexToLocationTableEntry, i16 indexToLocFormat, u16 numGlyphs);
TrueTypeGlyphLocation TrueTypeIndexToLocationGetGlyphLocation(const TrueTypeIndexToLocationTable *loca, u16 glyphIndex);

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

TrueTypeSimpleGlyph TrueTypeGlyfTableParseSimpleGlyph(const TrueTypeTableDirectoryEntry *glyfTableEntry, TrueTypeGlyphLocation glyphLocation);
void TrueTypeSimpleGlyphFree(TrueTypeSimpleGlyph *glyph);

// NOTE: Rasterization

Image TrueTypeGlyphRasterize(const TrueTypeSimpleGlyph *glyph, u32 targetPixelHeight);
