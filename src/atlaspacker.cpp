#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

#include "SDK.h"

/* The atlas.dat format specification.
 * All multibyte integers are little endian.
 *
 * First 4 bytes contain the item count
 * After that all the items follow:
 *    Uint32 x
 *    Uint32 y
 *    Uint32 width
 *    Uint32 height
 *
 */

typedef struct
{
    Uint32 width;
    Uint32 height;
    Uint8 *pixels;
} Image;

typedef struct
{
    Uint32 x;
    Uint32 y;
    Uint32 width;
    Uint32 height;
} PackedItem;

typedef struct
{
    Uint32 item_count;
    PackedItem *items;
} AtlasData;

void WriteAtlasData(const char *file_name, AtlasData &data)
{
    FILE *file = (FILE *)fopen(file_name, "w");
    if (!file)
    {
        printf("error: failed to create %s\n", file_name);
        return;
    }

    fwrite(&data.item_count, sizeof(Uint32), 1, file);
    fwrite(data.items, sizeof(PackedItem), data.item_count, file);

    fclose(file);
}

PackedItem PlaceImage(Image &atlas, Uint32 current_x, Uint32 current_y, Image &image)
{
    Uint32 src_index = 0;

    PackedItem item = {
        .x = current_x,
        .y = current_y,
        .width = image.width,
        .height = image.height,
    };

    for (Uint32 y = current_y; y < current_y + image.height; y++)
    {
        for (Uint32 x = current_x; x < current_x + image.width; x++)
        {
            Uint8 *pixel = &atlas.pixels[(y * atlas.width + x) * 4];
            pixel[0] = image.pixels[src_index++];
            pixel[1] = image.pixels[src_index++];
            pixel[2] = image.pixels[src_index++];
            pixel[3] = image.pixels[src_index++];
        }
    }

    return item;
}

AtlasData ShelfPack(Image &atlas, Image *images, Uint32 image_count)
{
    Uint32 current_x = 0;
    Uint32 current_y = 0;

    Uint32 max_height = 0;

    AtlasData data = {};

    Uint32 items_capacity = 5;
    data.items = (PackedItem *)malloc(sizeof(PackedItem) * items_capacity);

    for (Uint32 i = 0; i < image_count; i++)
    {
        Image &image = images[i];

        PackedItem item;
        if (current_x + image.width <= atlas.width)
        {
            max_height = Max(image.height, max_height);

            item = PlaceImage(atlas, current_x, current_y, image);
        }
        else
        {
            current_x = 0;
            current_y += max_height;
            max_height = 0;

            item = PlaceImage(atlas, current_x, current_y, image);
        }

        if (data.item_count + 1 > items_capacity)
        {
            Uint32 new_capacity = items_capacity * 2;
            PackedItem *new_items = (PackedItem *)realloc(data.items, new_capacity * sizeof(PackedItem));
            if (!new_items)
            {
                printf("error: failed to reallocate atlas data\n");
                continue;
            }

            data.items = new_items;
            items_capacity = new_capacity;
        }

        data.items[data.item_count++] = item;
    }

    return data;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("USAGE: %s <image1> <image2>\n", argv[0]);
        printf("Outputs: atlas.png in cwd and atlas.dat\n");
        return 1;
    }

    Uint32 image_count = argc - 1;
    Image *images = (Image *)malloc(sizeof(Image) * image_count);

    Uint32 image_index = 0;
    for (Int32 i = 1; i < argc; i++)
    {
        char *path = argv[i];

        Int32 width, height, channels;
        Uint8 *data = stbi_load(path, &width, &height, &channels, 4);
        if (!data)
        {
            printf("error: failed to load %s\n", path);
            continue;
        }

        images[image_index++] = {
            .width = (Uint32)width,
            .height = (Uint32)height,
            .pixels = data,
        };
    }

    Image atlas = {
        .width = 4096,
        .height = 4096,
        .pixels = (Uint8 *)malloc(atlas.width * atlas.height * 4),
    };

    for (Uint32 i = 0; i < atlas.width * atlas.height * 4; i += 4)
    {
        Uint8 *pixel = &atlas.pixels[i];
        pixel[0] = 0;
        pixel[1] = 0;
        pixel[2] = 0;
        pixel[3] = 255;
    }

    AtlasData atlas_data = ShelfPack(atlas, images, image_count);
    stbi_write_png("atlas.png", atlas.width, atlas.height, 4, atlas.pixels, atlas.width * 4);

    WriteAtlasData("atlas.dat", atlas_data);

    free(images);
    return 0;
}
