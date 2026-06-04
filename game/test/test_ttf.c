#include "utilities/init.h"
#include "utilities/file.h"
#include "utilities/ppm.h"

#include "game_ttf.h"

#if defined(FUZZING)

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    UtilitiesInitialize();

    TrueTypeFont font = TrueTypeFontLoadFromMemory(&permanentArena, &errorStream, data, size);

    if (font.numberOfTables > 0) {
        const u32 targetPixelHeight = 32;
        const u32 atlasWidth = 128;
        const u32 atlasHeight = 128;
        const u32 characterCount = 10;

        TrueTypeBakedGlyph glyphs[10];

        Image atlas = TrueTypeFontBakeAtlas(&permanentArena, &temporaryArena, &errorStream, &font, targetPixelHeight, atlasWidth, atlasHeight, ' ', characterCount, glyphs);
        Unused(atlas);
    }

    return 0;
}

#endif ///////////////////////////////////////////////////////

int main(void) {
    UtilitiesInitialize();

    usize fileSize = 0;
    void *fileData = ReadEntireFile("../../assets/fonts/arial.ttf", &fileSize);

    if (!fileData) {
        printf("game assets missing or running from the wrong directory\n");

        return 1;
    }

    TrueTypeFont font = TrueTypeFontLoadFromMemory(&permanentArena, &errorStream, fileData, fileSize);

    if (font.numberOfTables > 0) {
        u32 atlasWidth = 256;
        u32 atlasHeight = 256;
        u32 targetHeight = 32;

        TrueTypeBakedGlyph glyphs[TRUETYPE_CHARACTER_COUNT_FOR_ASCII];

        Image atlas = TrueTypeFontBakeAtlas(&permanentArena, &temporaryArena, &errorStream, &font, targetHeight, atlasWidth, atlasHeight, TRUETYPE_FIRST_CHARACTER_FOR_ASCII, TRUETYPE_CHARACTER_COUNT_FOR_ASCII, glyphs);

        if (atlas.pixels) {
            if (PPMWrite("ttf.ppm", atlas.size.x, atlas.size.y, atlas.bytesPerPixel, atlas.pixels)) {
                printf("wrote ttf.ppm\n");
            } else {
                printf("could not write ttf.ppm\n");
            }
        } else {
            printf("rasterization failed\n");
            printf("error steam: %.*s\n", (int)errorStream.offset, errorStream.memory);
        }
    } else {
        printf("table parsing failed\n");
        printf("error stream: %.*s\n", (int)errorStream.offset, errorStream.memory);
    }

    return 0;
}
