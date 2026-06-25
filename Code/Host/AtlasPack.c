//
// NOTE: Build tool. Packs multiple images into a single atlas.
// Alongside with the atlas image there's also a metadata generated with
// the following format:
//
// <image_path> <x_position> <y_position> <width> <height>
//

// TODO: Don't emit a header file, it's probably better to emit just raw files
// as it was before, but now it would make more sense as:
// 1) Game is packaged into a .zip anyways
// 2) You could WAY more easily inspect the output

#include "STB.h"
#include "Types.h"

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

typedef struct
{
    FILE *File;
    Int32 Col;
    Uint32 TotalBytes;
} WriteCtx;

static void WriteCallback(void *Context, void *Data, int Size)
{
    WriteCtx *Ctx = (WriteCtx *)Context;

    const Uint8 *Bytes = (const Uint8 *)Data;
    for (int I = 0; I < Size; ++I)
    {
        fprintf(Ctx->File, "0x%02X,", Bytes[I]);
        Ctx->TotalBytes++;
        Ctx->Col++;

        if (Ctx->Col >= 16)
        {
            fprintf(Ctx->File, "\n    ");
            Ctx->Col = 0;
        }
        else
        {
            fprintf(Ctx->File, " ");
        }
    }
}

static void ToValidCIdentifier(char *Dest, const char *Src, Usize DestSize)
{
    const char *Base = Src;
    for (const char *Path = Src; *Path != '\0'; ++Path)
    {
        if (*Path == '/' || *Path == '\\')
        {
            Base = Path + 1;
        }
    }

    Usize J = 0;
    if (Base[0] >= '0' && Base[0] <= '9' && DestSize > 1)
    {
        Dest[J++] = '_';
    }
    for (Usize I = 0; Base[I] != '\0' && J < DestSize - 1; ++I)
    {
        char C = Base[I];
        if ((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9'))
        {
            Dest[J++] = C;
        }
        else
        {
            Dest[J++] = '_';
        }
    }
    Dest[J] = '\0';
}

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

    char OutputHeaderPath[512];
    snprintf(OutputHeaderPath, sizeof(OutputHeaderPath), "%s.h", OutputPrefix);

    FILE *File = fopen(OutputHeaderPath, "w");
    if (!File)
    {
        return 1;
    }

    char CIdentifier[256];
    ToValidCIdentifier(CIdentifier, OutputPrefix, sizeof(CIdentifier));

    fprintf(File, "#if !defined(%s)\n", CIdentifier);
    fprintf(File, "#define %s\n\n", CIdentifier);
    fprintf(File, "const unsigned char Gen%s[] = {\n    ", CIdentifier);

    WriteCtx Ctx = {File, 0, 0};
    if (!stbi_write_png_to_func(WriteCallback, &Ctx, AtlasSize, AtlasSize, 4, AtlasPixels, AtlasSize * 4))
    {
        fclose(File);
        return 1;
    }

    fprintf(File, "\n};\n");
    fprintf(File, "const unsigned int Gen%sLen = %u;\n\n", CIdentifier, Ctx.TotalBytes);

    fprintf(File, "const char Gen%sMeta[] = {\n    ", CIdentifier);

    Int32 MetaCol = 0;
    Uint32 MetaLen = 0;
    for (Int32 I = 0; I < ImageCount; ++I)
    {
        char Line[1024];
        Int32 LineLen = snprintf(Line, sizeof(Line), "%s %d %d %d %d\n", Images[I].Path, Rects[I].x, Rects[I].y, Rects[I].w, Rects[I].h);

        for (Int32 J = 0; J < LineLen; ++J)
        {
            fprintf(File, "0x%02X,", (Uint8)Line[J]);

            MetaLen++;
            MetaCol++;

            if (MetaCol >= 16)
            {
                fprintf(File, "\n    ");
                MetaCol = 0;
            }
            else
            {
                fprintf(File, " ");
            }
        }
    }
    fprintf(File, "0x00\n};\n");
    fprintf(File, "const unsigned int Gen%sMetaLen = %u;\n", CIdentifier, MetaLen);
    fprintf(File, "#endif");

    fclose(File);

    return 0;
}
