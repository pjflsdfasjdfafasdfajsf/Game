#include "game_png.h"
#include "game_platform.h"
#include "game_types.h"

// NOTE: CRC32.

static u32 crc32_table[256];
static bool crc32_table_initialized = false;

static void crc32_table_initialize(void)
{
    if (crc32_table_initialized)
    {
        return;
    }

    const u32 crc32_polynomial = 0xEDB88320;

    for (u32 i = 0; i < 256; i++)
    {
        u32 crc = i;
        for (u32 j = 0; j < 8; j++)
        {
            if (IS_BIT_SET(crc, 1))
            {
                crc = crc32_polynomial ^ (crc >> 1);
            }
            else
            {
                crc = (crc >> 1);
            }
        }
        crc32_table[i] = crc;
    }

    crc32_table_initialized = true;
}

static u32 crc32_calculate(const u8 *memory, usize length)
{
    crc32_table_initialize();

    u32 crc = 0xFFFFFFFF;

    for (usize i = 0; i < length; i++)
    {
        u32 table_index = (crc ^ memory[i]) & 0xFF;
        crc = crc32_table[table_index] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

// NOTE: primary PNG stuff.

static const u8 png_file_signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
static const usize png_file_signature_length = 8;
static const usize png_chunk_length_size = 4;
static const usize png_chunk_type_size = 4;
static const usize png_chunk_crc_size = 4;
static const u32 pngihdr_chunk_length = 13;
static const u32 png_max_idat_chunks = 1024;

typedef enum
{
    png_color_type_grayscale = 0,
    png_color_type_true_color = 2,
    png_color_type_indexed = 3,
    png_color_type_grayscale_alpha = 4,
    png_color_type_true_color_alpha = 6
} png_color_type;

typedef struct
{
    u32 width;
    u32 height;
    u8 bit_depth;
    u8 color_type;
    u8 compression_method;
    u8 filter_method;
    u8 interlace_method;
} png_header;

typedef struct
{
    const u8 *memory;
    u32 length;
} pngidat_chunk;

// NOTE: deflating.

static const u16 deflate_length_bases[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};

static const u8 deflate_length_extra_bits[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};

static const u16 deflate_distance_bases[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};

static const u8 deflate_distance_extra_bits[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static const u8 deflate_code_length_order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

typedef struct
{
    u16 counts[16];
    u16 symbols[288];
} huffman_tree;

typedef struct
{
    const pngidat_chunk *chunks;
    usize chunk_count;
    usize current_chunk_index;
    usize byte_offset_in_chunk;

    u32 bit_buffer;
    u32 bits_available;
    bool has_overflowed;
} bit_stream;

static inline u32 stream_read_bits(bit_stream *stream, u32 bit_count)
{
    while (stream->bits_available < bit_count)
    {
        if (stream->current_chunk_index >= stream->chunk_count)
        {
            stream->has_overflowed = true;
            return 0;
        }

        const pngidat_chunk *current_chunk = &stream->chunks[stream->current_chunk_index];

        u8 next_byte = current_chunk->memory[stream->byte_offset_in_chunk];
        stream->bit_buffer |= ((u32)next_byte << stream->bits_available);
        stream->bits_available += 8;

        stream->byte_offset_in_chunk++;

        if (stream->byte_offset_in_chunk >= current_chunk->length)
        {
            stream->current_chunk_index++;
            stream->byte_offset_in_chunk = 0;
        }
    }

    u32 result = stream->bit_buffer & ((1U << bit_count) - 1);
    stream->bit_buffer >>= bit_count;
    stream->bits_available -= bit_count;

    return result;
}

static u32 stream_decode_symbol(bit_stream *stream, const huffman_tree *tree)
{
    u32 code = 0;
    u32 first_code = 0;
    u32 first_symbol_index = 0;

    for (u32 length = 1; length <= 15; length++)
    {
        code = (code << 1) | stream_read_bits(stream, 1);

        u32 count = tree->counts[length];
        if (code - first_code < count)
        {
            return tree->symbols[first_symbol_index + (code - first_code)];
        }

        first_symbol_index += count;
        first_code = (first_code + count) << 1;
    }

    stream->has_overflowed = true;
    return 0;
}

static bool huffman_tree_initialize(huffman_tree *tree, const u8 *lengths, u32 count)
{
    ZERO_ARRAY(tree->counts);
    ZERO_ARRAY(tree->symbols);

    for (u32 i = 0; i < count; i++)
    {
        if (lengths[i] > 15)
        {
            return false;
        }

        tree->counts[lengths[i]]++;
    }
    tree->counts[0] = 0;

    u16 next_symbol_index[16];
    ZERO_ARRAY(next_symbol_index);

    u16 symbol_index = 0;
    for (u32 i = 1; i <= 15; i++)
    {
        next_symbol_index[i] = symbol_index;
        symbol_index += tree->counts[i];
    }

    for (u32 i = 0; i < count; i++)
    {
        u8 length = lengths[i];

        if (length != 0)
        {
            tree->symbols[next_symbol_index[length]++] = (u16)i;
        }
    }
    return true;
}

static bool decompress_deflate(const pngidat_chunk *chunks, usize chunk_count, u8 *output_buffer, usize output_capacity)
{
    bit_stream stream = {0};

    stream.chunks = chunks;
    stream.chunk_count = chunk_count;

    u32 zlib_method_flags = stream_read_bits(&stream, 8);
    u32 zlib_additional_flags = stream_read_bits(&stream, 8);

    if ((zlib_method_flags & 0x0F) != 8)
    {
        // NOTE: not DEFLATE
        return false;
    }
    if (((zlib_method_flags << 8) + zlib_additional_flags) % 31 != 0)
    {
        // NOTE: checksum failed
        return false;
    }
    if (IS_BIT_SET(zlib_additional_flags, 0x20))
    {
        // NOTE: preset dictionaries are not allowed in PNG
        return false;
    }

    usize output_position = 0;
    bool is_final_block = false;

    while (!is_final_block && !stream.has_overflowed)
    {
        is_final_block = stream_read_bits(&stream, 1);
        u32 block_type = stream_read_bits(&stream, 2);

        if (block_type == 0)
        {
            stream.bit_buffer = 0;
            stream.bits_available = 0;

            u32 length = stream_read_bits(&stream, 16);
            u32 inverted_length = stream_read_bits(&stream, 16);
            if (length != (~inverted_length & 0xFFFF))
            {
                return false;
            }

            for (u32 i = 0; i < length; i++)
            {
                if (output_position >= output_capacity)
                {
                    return false;
                }

                output_buffer[output_position++] = (u8)stream_read_bits(&stream, 8);
            }
        }
        else if (block_type == 1 || block_type == 2)
        {
            huffman_tree literal_tree = {0};

            huffman_tree distance_tree = {0};

            // NOTE: fixed huffman
            if (block_type == 1)
            {
                u8 lengths[288];
                for (u32 i = 0; i <= 143; i++)
                {
                    lengths[i] = 8;
                }
                for (u32 i = 144; i <= 255; i++)
                {
                    lengths[i] = 9;
                }
                for (u32 i = 256; i <= 279; i++)
                {
                    lengths[i] = 7;
                }
                for (u32 i = 280; i <= 287; i++)
                {
                    lengths[i] = 8;
                }

                huffman_tree_initialize(&literal_tree, lengths, 288);

                u8 dist_lengths[32];
                for (u32 i = 0; i < 32; i++)
                {
                    dist_lengths[i] = 5;
                }

                huffman_tree_initialize(&distance_tree, dist_lengths, 32);
            }
            else
            { // NOTE: dynamic huffman
                u32 hlit = stream_read_bits(&stream, 5) + 257;
                u32 hdist = stream_read_bits(&stream, 5) + 1;
                u32 hclen = stream_read_bits(&stream, 4) + 4;

                u8 code_length_lengths[19];
                ZERO_ARRAY(code_length_lengths);

                for (u32 i = 0; i < hclen; i++)
                {
                    code_length_lengths[deflate_code_length_order[i]] = (u8)stream_read_bits(&stream, 3);
                }

                huffman_tree code_length_tree = {0};

                huffman_tree_initialize(&code_length_tree, code_length_lengths, 19);

                u8 lengths[320];
                ZERO_ARRAY(lengths);

                u32 total_codes = hlit + hdist;
                u32 index = 0;

                while (index < total_codes)
                {
                    u32 symbol = stream_decode_symbol(&stream, &code_length_tree);
                    if (symbol <= 15)
                    {
                        lengths[index++] = (u8)symbol;
                    }
                    else if (symbol == 16)
                    {
                        u32 repeat_count = stream_read_bits(&stream, 2) + 3;

                        if (index == 0)
                        {
                            return false;
                        }

                        if (index + repeat_count > total_codes)
                        {
                            return false;
                        }

                        u8 repeat_value = lengths[index - 1];
                        while (repeat_count--)
                        {
                            lengths[index++] = repeat_value;
                        }
                    }
                    else if (symbol == 17)
                    {
                        u32 repeat_count = stream_read_bits(&stream, 3) + 3;

                        if (index + repeat_count > total_codes)
                        {
                            return false;
                        }

                        while (repeat_count--)
                        {
                            lengths[index++] = 0;
                        }
                    }
                    else if (symbol == 18)
                    {
                        u32 repeat_count = stream_read_bits(&stream, 7) + 11;

                        if (index + repeat_count > total_codes)
                        {
                            return false;
                        }

                        while (repeat_count--)
                        {
                            lengths[index++] = 0;
                        }
                    }
                    else
                    {
                        return false;
                    }
                }

                huffman_tree_initialize(&literal_tree, lengths, hlit);
                huffman_tree_initialize(&distance_tree, lengths + hlit, hdist);
            }

            while (!stream.has_overflowed)
            {
                u32 symbol = stream_decode_symbol(&stream, &literal_tree);

                if (symbol < 256)
                {
                    if (output_position >= output_capacity)
                    {
                        return false;
                    }

                    output_buffer[output_position++] = (u8)symbol;
                }
                else if (symbol == 256)
                {
                    break;
                }
                else
                {
                    if (symbol > 285 || symbol < 257)
                    {
                        return false;
                    }
                    symbol -= 257;
                    u32 length = deflate_length_bases[symbol] + stream_read_bits(&stream, deflate_length_extra_bits[symbol]);

                    u32 distance_symbol = stream_decode_symbol(&stream, &distance_tree);
                    if (distance_symbol >= 30)
                    {
                        return false;
                    }

                    u32 distance = deflate_distance_bases[distance_symbol] + stream_read_bits(&stream, deflate_distance_extra_bits[distance_symbol]);

                    for (u32 i = 0; i < length; i++)
                    {
                        if (output_position >= output_capacity || output_position < distance)
                        {
                            return false;
                        }

                        output_buffer[output_position] = output_buffer[output_position - distance];
                        output_position++;
                    }
                }
            }
        }
        else
        {
            return false;
        }
    }

    return !stream.has_overflowed;
}

static inline u8 paeth_predictor(u8 left_byte, u8 up_byte, u8 up_left_byte)
{
    const int a = (int)left_byte;
    const int b = (int)up_byte;
    const int c = (int)up_left_byte;

    const int initial_estimate = a + b - c;
    const int distance_a = ABS(initial_estimate - a);
    const int distance_b = ABS(initial_estimate - b);
    const int distance_c = ABS(initial_estimate - c);

    if (distance_a <= distance_b && distance_a <= distance_c)
    {
        return (u8)a;
    }
    else if (distance_b <= distance_c)
    {
        return (u8)b;
    }
    else
    {
        return (u8)c;
    }
}

image image_load_from_png(memory_arena *permanent_arena, memory_arena *temporary_arena, memory_stream *error_stream, const void *memory, usize length)
{
    image result = {0};

    if (!memory)
    {
        memory_stream_write_string(error_stream, "error: invalid parameter: memory.\n");

        return result;
    }

    if (length < png_file_signature_length)
    {
        memory_stream_write_string(error_stream, "error: most likely corrupted PNG file as it is "
                                                 "too small to even contain a signature.\n");

        return result;
    }

    const u8 *image_buffer_pointer = (const u8 *)memory;

    bool has_valid_signature = memory_equals(image_buffer_pointer, png_file_signature, png_file_signature_length);
    if (!has_valid_signature)
    {
        memory_stream_write_string(error_stream, "error: corrupted PNG file.\n");

        return result;
    }

    memory_stream stream;
    memory_stream_initialize_read_only(&stream, memory, length);
    stream.offset = png_file_signature_length;

    bool has_parsed_ihdr = false;

    png_header image_header = {0};

    pngidat_chunk *idat_chunks = MEMORY_ARENA_PUSH_ARRAY(temporary_arena, pngidat_chunk, png_max_idat_chunks);
    if (!idat_chunks)
    {
        memory_stream_write_string(error_stream, "error: out of memory.\n");

        return result;
    }

    usize idat_chunk_count = 0;
    usize total_idat_data_size = 0;

    bool has_seen_idat = false;
    bool is_previous_chunk_idat = false;

    while (stream.offset < stream.capacity)
    {
        if (!memory_stream_has_space(&stream, png_chunk_length_size + png_chunk_type_size))
        {
            break;
        }

        u32 chunk_length = memory_stream_read_uint32_big_endian(&stream);

        const u8 *chunk_type_pointer = &stream.memory[stream.offset];
        stream.offset += png_chunk_type_size;

        if (!memory_stream_has_space(&stream, chunk_length + png_chunk_crc_size))
        {
            break;
        }

        const u8 *chunk_data_pointer = &stream.memory[stream.offset];
        stream.offset += chunk_length;

        // NOTE' UNUSED when fuzzing
        u32 expected_crc = memory_stream_read_uint32_big_endian(&stream);
        UNUSED(expected_crc);

        usize crc_data_length = png_chunk_type_size + chunk_length;
        u32 calculated_crc = crc32_calculate(chunk_type_pointer, crc_data_length);
        UNUSED(calculated_crc);

#if !defined(FUZZING)
        if (calculated_crc != expected_crc)
        {
            memory_stream_write_string(error_stream, "error: CRC mismatch in PNG chunk.\n");

            break;
        }
#endif

        if (memory_equals(chunk_type_pointer, "IHDR", 4))
        {
            if (chunk_length != pngihdr_chunk_length)
            {
                break;
            }

            if (has_parsed_ihdr)
            {
                break;
            }

            image_header.width = read_uint32_big_endian(&chunk_data_pointer[0]);
            image_header.height = read_uint32_big_endian(&chunk_data_pointer[4]);
#if defined(FUZZING)
            if (image_header.width > 128 || image_header.height > 128)
            {
                break;
            }
#endif
            image_header.bit_depth = chunk_data_pointer[8];
            image_header.color_type = chunk_data_pointer[9];
            image_header.compression_method = chunk_data_pointer[10];
            image_header.filter_method = chunk_data_pointer[11];
            image_header.interlace_method = chunk_data_pointer[12];

            if (image_header.width == 0 || image_header.height == 0)
            {
                break;
            }

            if (image_header.compression_method != 0 || image_header.filter_method != 0 || image_header.bit_depth != 8 || image_header.interlace_method != 0)
            {
                break;
            }

            has_parsed_ihdr = true;
            is_previous_chunk_idat = false;
        }
        else if (!has_parsed_ihdr)
        {
            break;
        }
        else if (memory_equals(chunk_type_pointer, "IDAT", 4))
        {
            if (has_seen_idat && !is_previous_chunk_idat)
            {
                break;
            }

            if (idat_chunk_count >= png_max_idat_chunks)
            {
                break;
            }

            idat_chunks[idat_chunk_count].memory = chunk_data_pointer;
            idat_chunks[idat_chunk_count].length = chunk_length;

            idat_chunk_count++;
            total_idat_data_size += chunk_length;

            has_seen_idat = true;
            is_previous_chunk_idat = true;
        }
        else if (memory_equals(chunk_type_pointer, "IEND", 4))
        {
            is_previous_chunk_idat = false;

            break;
        }
        else
        {
            bool is_critical_chunk = !IS_BIT_SET(chunk_type_pointer[0], 0x20);
            if (is_critical_chunk)
            {
                break;
            }

            is_previous_chunk_idat = false;
        }
    }

    if (!has_parsed_ihdr || total_idat_data_size == 0)
    {
        memory_stream_write_string(error_stream, "error: missing IHDR or no IDAT data.\n");

        return result;
    }

    u32 bytes_per_pixel = 0;

    if (image_header.color_type == png_color_type_grayscale)
    {
        bytes_per_pixel = 1;
    }
    else if (image_header.color_type == png_color_type_true_color)
    {
        bytes_per_pixel = 3;
    }
    else if (image_header.color_type == png_color_type_grayscale_alpha)
    {
        bytes_per_pixel = 2;
    }
    else if (image_header.color_type == png_color_type_true_color_alpha)
    {
        bytes_per_pixel = 4;
    }
    else
    {
        memory_stream_write_string(error_stream, "error: unsupported color type.\n");

        return result;
    }

    usize scanline_stride = 1 + (usize)image_header.width * bytes_per_pixel;
    usize decompressed_capacity = scanline_stride * image_header.height;

    u8 *decompressed_buffer = MEMORY_ARENA_PUSH_BYTES(temporary_arena, decompressed_capacity);
    if (!decompressed_buffer)
    {
        memory_stream_write_string(error_stream, "error: out of memory.\n");

        return result;
    }

    bool succesfully_decompressed = decompress_deflate(idat_chunks, idat_chunk_count, decompressed_buffer, decompressed_capacity);
    if (!succesfully_decompressed)
    {
        memory_stream_write_string(error_stream, "error: could not decompress.\n");

        return result;
    }

    const u32 image_width = image_header.width;
    const u32 image_height = image_header.height;

    const usize raw_scanline_byte_count = (usize)image_width * bytes_per_pixel;
    if (image_width != 0 && raw_scanline_byte_count / image_width != bytes_per_pixel)
    {
        memory_stream_write_string(error_stream, "error: scanline byte count overflow.\n");

        return result;
    }

    const usize filtered_scanline_byte_count = raw_scanline_byte_count + 1;
    const usize expected_decompressed_byte_count = filtered_scanline_byte_count * image_height;

    if (decompressed_capacity < expected_decompressed_byte_count)
    {
        memory_stream_write_string(error_stream, "error: decompressed PNG data is smaller than expected.\n");

        return result;
    }

    const usize total_raw_pixel_byte_count = raw_scanline_byte_count * image_height;
    u8 *raw_pixels = MEMORY_ARENA_PUSH_BYTES(temporary_arena, total_raw_pixel_byte_count);
    if (!raw_pixels)
    {
        memory_stream_write_string(error_stream, "error: out of memory.\n");

        return result;
    }

    usize source_read_offset = 0;
    usize destination_write_offset = 0;

    for (u32 current_scanline_index = 0; current_scanline_index < image_height; current_scanline_index++)
    {
        if (source_read_offset + filtered_scanline_byte_count > decompressed_capacity)
        {
            memory_stream_write_string(error_stream, "error: scanline data overruns decompressed buffer.\n");

            return result;
        }

        const u8 scanline_filter_type = decompressed_buffer[source_read_offset++];
        const u8 *current_filtered_scanline_data = &decompressed_buffer[source_read_offset];
        u8 *current_raw_scanline_data = &raw_pixels[destination_write_offset];

        const u8 *previous_raw_scanline_data = (current_scanline_index > 0) ? &raw_pixels[destination_write_offset - raw_scanline_byte_count] : 0;

        if (scanline_filter_type > 4)
        {
            memory_stream_write_string(error_stream, "error: unknown PNG filter type.\n");

            return result;
        }

        for (usize byte_index = 0; byte_index < raw_scanline_byte_count; byte_index++)
        {
            u8 raw_left = (byte_index >= bytes_per_pixel) ? current_raw_scanline_data[byte_index - bytes_per_pixel] : 0;
            u8 raw_up = previous_raw_scanline_data ? previous_raw_scanline_data[byte_index] : 0;

            u8 prediction = 0;
            switch (scanline_filter_type)
            {
            case 1:
            {
                prediction = raw_left;
                break;
            }
            case 2:
            {
                prediction = raw_up;
                break;
            }
            case 3:
            {
                prediction = (u8)(((u16)raw_left + (u16)raw_up) / 2);
                break;
            }
            case 4:
            {
                u8 raw_up_left = (previous_raw_scanline_data && byte_index >= bytes_per_pixel) ? previous_raw_scanline_data[byte_index - bytes_per_pixel] : 0;
                prediction = paeth_predictor(raw_left, raw_up, raw_up_left);
            }
            break;
            }

            current_raw_scanline_data[byte_index] = current_filtered_scanline_data[byte_index] + prediction;
        }

        source_read_offset += raw_scanline_byte_count;
        destination_write_offset += raw_scanline_byte_count;
    }

    const usize pixel_byte_count = (usize)image_width * image_height * 4;
    u8 *pixels = MEMORY_ARENA_PUSH_BYTES(permanent_arena, pixel_byte_count);

    if (!pixels)
    {
        memory_stream_write_string(error_stream, "error: out of memory.\n");
        return result;
    }

    usize pixel_count = (usize)image_width * image_height;
    for (usize i = 0; i < pixel_count; i++)
    {
        if (bytes_per_pixel == 4)
        {
            pixels[i * 4 + 0] = raw_pixels[i * 4 + 0];
            pixels[i * 4 + 1] = raw_pixels[i * 4 + 1];
            pixels[i * 4 + 2] = raw_pixels[i * 4 + 2];
            pixels[i * 4 + 3] = raw_pixels[i * 4 + 3];
        }
        else if (bytes_per_pixel == 3)
        {
            pixels[i * 4 + 0] = raw_pixels[i * 3 + 0];
            pixels[i * 4 + 1] = raw_pixels[i * 3 + 1];
            pixels[i * 4 + 2] = raw_pixels[i * 3 + 2];
            pixels[i * 4 + 3] = 255;
        }
        else if (bytes_per_pixel == 2)
        {
            pixels[i * 4 + 0] = raw_pixels[i * 2 + 0];
            pixels[i * 4 + 1] = raw_pixels[i * 2 + 0];
            pixels[i * 4 + 2] = raw_pixels[i * 2 + 0];
            pixels[i * 4 + 3] = raw_pixels[i * 2 + 1];
        }
        else if (bytes_per_pixel == 1)
        {
            pixels[i * 4 + 0] = raw_pixels[i];
            pixels[i * 4 + 1] = raw_pixels[i];
            pixels[i * 4 + 2] = raw_pixels[i];
            pixels[i * 4 + 3] = 255;
        }
    }

    result.size.width = image_width;
    result.size.height = image_height;
    result.pixels = pixels;

    return result;
}
