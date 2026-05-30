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

typedef struct {
    u32 characterCode;
    bool isValid;
    Vector2 uvMin;
    Vector2 uvMax;
    Vector2 size;
    Vector2 offset;
} TrueTypeBakedGlyph;

TrueTypeFont TrueTypeFontLoadFromMemory(const void *memory, usize length);

Image TrueTypeFontBakeAtlas(const TrueTypeFont *font, u32 targetPixelHeight, u32 atlasWidth, u32 atlasHeight, u32 firstCharacter, u32 characterCount, TrueTypeBakedGlyph *outGlyphs);
