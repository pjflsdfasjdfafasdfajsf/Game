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

struct Image
{
    Uint32 width;
    Uint32 height;
    Uint8 *pixels;
};

struct PackedItem
{
    Uint32 x;
    Uint32 y;
    Uint32 width;
    Uint32 height;
};

struct Atlas
{
    Uint32 item_count;
    PackedItem *items;

    Image image;

    void WriteData(const char *file_name);
    PackedItem PlaceImage(Uint32 current_x, Uint32 current_y, Image &image);
    void Pack(Image *images, Uint32 image_count);
};

void Atlas::WriteData(const char *file_name)
{
    FILE *file = (FILE *)fopen(file_name, "w");
    if (!file)
    {
        printf("error: failed to create %s\n", file_name);
        return;
    }

    fwrite(&this->item_count, sizeof(Uint32), 1, file);
    fwrite(this->items, sizeof(PackedItem), this->item_count, file);

    fclose(file);
}

PackedItem Atlas::PlaceImage(Uint32 current_x, Uint32 current_y, Image &image)
{
    Uint32 source_index = 0;

    PackedItem item;
    item.x = current_x;
    item.y = current_y;
    item.width = image.width;
    item.height = image.height;

    for (Uint32 y = current_y; y < current_y + image.height; y++)
    {
        for (Uint32 x = current_x; x < current_x + image.width; x++)
        {
            Uint8 *pixel = &this->image.pixels[(y * this->image.width + x) * 4];
            pixel[0] = image.pixels[source_index++];
            pixel[1] = image.pixels[source_index++];
            pixel[2] = image.pixels[source_index++];
            pixel[3] = image.pixels[source_index++];
        }
    }

    return item;
}

void Atlas::Pack(Image *images, Uint32 image_count)
{
    Uint32 current_x = 0;
    Uint32 current_y = 0;

    Uint32 max_height = 0;

    Uint32 items_capacity = 5;
    this->items = (PackedItem *)malloc(sizeof(PackedItem) * items_capacity);

    for (Uint32 i = 0; i < image_count; i++)
    {
        Image &image = images[i];

        PackedItem item;
        if (current_x + image.width <= this->image.width)
        {
            max_height = Max(image.height, max_height);

            item = this->PlaceImage(current_x, current_y, image);
        }
        else
        {
            current_x = 0;
            current_y += max_height;
            max_height = 0;

            item = this->PlaceImage(current_x, current_y, image);
        }

        if (this->item_count + 1 > items_capacity)
        {
            Uint32 capacity = items_capacity * 2;
            PackedItem *items = (PackedItem *)realloc(this->items, capacity * sizeof(PackedItem));
            if (!items)
            {
                printf("error: failed to reallocate atlas data\n");
                continue;
            }

            this->items = items;
            items_capacity = capacity;
        }

        this->items[this->item_count++] = item;
    }
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

        images[image_index++].width = (Uint32)width;
        images[image_index++].height = (Uint32)height;
        images[image_index++].pixels = data;
    }

    Atlas atlas = {};
    atlas.image.width = 4096,
    atlas.image.height = 4096,
    atlas.image.pixels = (Uint8 *)malloc(atlas.image.width * atlas.image.height * 4);

    for (Uint32 i = 0; i < atlas.image.width * atlas.image.height * 4; i += 4)
    {
        Uint8 *pixel = &atlas.image.pixels[i];
        pixel[0] = 0;
        pixel[1] = 0;
        pixel[2] = 0;
        pixel[3] = 255;
    }

    stbi_write_png("atlas.png", atlas.image.width, atlas.image.height, 4, atlas.image.pixels, atlas.image.width * 4);

    atlas.Pack(images, image_count);
    atlas.WriteData("atlas.dat");

    free(images);
    return 0;
}
