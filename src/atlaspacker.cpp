#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image.h"
#include "stb_image_write.h"

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
    Uint32 x = 0;
    Uint32 y = 0;

    Uint32 maximum_height = 0;

    Uint32 items_capacity = 5;
    this->items = (PackedItem *)malloc(sizeof(PackedItem) * items_capacity);

    for (Uint32 i = 0; i < image_count; i++)
    {
        Image &image = images[i];

        PackedItem item;
        if (x + image.width <= this->image.width)
        {
            maximum_height = Max(image.height, maximum_height);

            item = this->PlaceImage(x, y, image);
            x += image.width;
        }
        else
        {
            x = 0;
            y += maximum_height;
            maximum_height = 0;

            item = this->PlaceImage(x, y, image);
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
