//
// NOTE: Build tool. Packs multiple images into a single atlas.
// Alongside with the atlas image there's also a metadata generated with
// the following format:
//
// <image_path> <x_position> <y_position> <width> <height>
//

#include "STB.h"
#include "Public/Types.h"

#include <stdio.h>

// NOTE: The output image will be AtlasSize x AtlasSize pixels
#define AtlasSize 4096

typedef struct
{
    const char *Path;
    Uint8 *Pixels;
    Int32 Width;
    Int32 Height;
    Int32 Channels;
} Image;

// NOTE:
// Arguments:
// 1. Output prefix
// 2. ...N. Images to pack
Int32 main(Int32 Argc, char **Argv)
{
    if (Argc < 3)
    {
        printf("USAGE: %s <output_prefix>, <img1> [img2...]\n", Argv[0]);

        return 1;
    }

    const char *OutputPrefix = Argv[1];
    Int32 ImageCount = Argc - 2;
    char **ImagePaths = &Argv[2];

    // NOTE: The atlas image

    Image *Images = calloc(ImageCount, sizeof(Image));
    if (!Images)
    {
        return 1;
    }
    stbrp_rect *Rects = calloc(ImageCount, sizeof(stbrp_rect));
    if (!Rects)
    {
        return 1;
    }

    for (Int32 I = 0; I < ImageCount; ++I)
    {
        Images[I].Path = ImagePaths[I];
        Images[I].Pixels = stbi_load(Images[I].Path, &Images[I].Width, &Images[I].Height, &Images[I].Channels, 4);

        if (!Images[I].Pixels)
        {
            return 1;
        }

        Rects[I].id = I;
        Rects[I].w = Images[I].Width;
        Rects[I].h = Images[I].Height;
    }

    stbrp_context Context;
    stbrp_node *Nodes = calloc(AtlasSize, sizeof(stbrp_node));
    if (!Nodes)
    {
        return 1;
    }

    stbrp_init_target(&Context, AtlasSize, AtlasSize, Nodes, AtlasSize);
    stbrp_pack_rects(&Context, Rects, ImageCount);

    Uint8 *AtlasPixels = calloc(AtlasSize * AtlasSize * 4, 1);
    if (!AtlasPixels)
    {
        return 1;
    }

    for (Int32 I = 0; I < ImageCount; ++I)
    {
        if (Rects[I].was_packed)
        {
            Image *Image = &Images[I];
            Int32 DstX = Rects[I].x;
            Int32 DstY = Rects[I].y;

            for (Int32 Y = 0; Y < Image->Height; ++Y)
            {
                for (Int32 X = 0; X < Image->Width; ++X)
                {
                    Int32 Src = (Y * Image->Width + X) * 4;
                    Int32 Dst = ((DstY + Y) * AtlasSize + (DstX + X)) * 4;

                    AtlasPixels[Dst + 0] = Image->Pixels[Src + 0];
                    AtlasPixels[Dst + 1] = Image->Pixels[Src + 1];
                    AtlasPixels[Dst + 2] = Image->Pixels[Src + 2];
                    AtlasPixels[Dst + 3] = Image->Pixels[Src + 3];
                }
            }
        }
    }

    char OutputPngPath[512];
    snprintf(OutputPngPath, sizeof(OutputPngPath), "%s.png", OutputPrefix);
    if (!stbi_write_png(OutputPngPath, AtlasSize, AtlasSize, 4, AtlasPixels, AtlasSize * 4))
    {
        return 1;
    }

    char OutputTxtPath[512];
    snprintf(OutputTxtPath, sizeof(OutputTxtPath), "%s.txt", OutputPrefix);
    FILE *File = fopen(OutputTxtPath, "w");
    if (!File)
    {
        return 1;
    }

    for (Int32 I = 0; I < ImageCount; ++I)
    {
        fprintf(File, "%s %d %d %d %d\n", Images[I].Path, Rects[I].x, Rects[I].y, Rects[I].w, Rects[I].h);
    }

    fclose(File);

    return 0;
}
