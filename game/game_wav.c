#include "game_wav.h"
#include "game_platform.h"
#include "game_types.h"

// NOTE: unlike PNG/TTF this goat of a format stores signatures in little endian, so we can use arrays directly instead of
// FOURCC
static const u8 riff_file_signature[4] = {'R', 'I', 'F', 'F'};
static const u8 wave_format_signature[4] = {'W', 'A', 'V', 'E'};
static const usize riff_header_length = 12;

static const u16 pcm_audio_format = 1;

typedef struct {
    u16 audio_format;
    u16 channel_count;
    u32 sample_rate;
    u32 byte_rate;
    u16 block_align;
    u16 bits_per_sample;
} wav_format_header;

sound_clip sound_load_from_wav(memory_arena *permanent_arena, memory_stream *error_stream, const void *memory, usize length) {
    sound_clip result = {0};

    if (!memory) {
        memory_stream_write_line(error_stream, "error: invalid parameter: memory.");
        return result;
    }

    if (length < riff_header_length) {
        memory_stream_write_line(error_stream, "error: file too small to contain a valid RIFF/WAVE header.");
        return result;
    }

    const u8 *file_buffer_pointer = (const u8 *)memory;

    bool is_valid_riff = memory_equals(&file_buffer_pointer[0], riff_file_signature, 4);
    bool is_valid_wave = memory_equals(&file_buffer_pointer[8], wave_format_signature, 4);

    if (!is_valid_riff || !is_valid_wave) {
        memory_stream_write_line(error_stream, "error: corrupted WAV file");
        return result;
    }

    ///

    memory_stream stream;
    memory_stream_initialize_read_only(&stream, memory, length);
    stream.offset = riff_header_length;

    bool has_parsed_fmt = false;
    bool has_parsed_data = false;

    wav_format_header format = {0};
    const u8 *audio_data_pointer = 0;
    u32 audio_data_length = 0;

    while (stream.offset < stream.capacity) {
        if (!memory_stream_has_space(&stream, 8)) {
            break;
        }

        const u8 *chunk_id_pointer = &stream.memory[stream.offset];
        stream.offset += 4;

        u32 chunk_length = memory_stream_read_u_int32_little_endian(&stream);

        if (!memory_stream_has_space(&stream, chunk_length)) {
            memory_stream_write_line(error_stream, "error: WAV chunk declares a size that exceeds file bounds.");
            return result;
        }

        const u8 *chunk_data_pointer = &stream.memory[stream.offset];
        stream.offset += chunk_length;

        bool needs_padding_byte = (chunk_length % 2 != 0);
        if (needs_padding_byte && memory_stream_has_space(&stream, 1)) {
            stream.offset += 1;
        }

        if (memory_equals(chunk_id_pointer, "fmt ", 4)) {
            if (has_parsed_fmt) {
                continue;
            }

            if (chunk_length < 16) {
                memory_stream_write_line(error_stream, "error: WAV fmt chunk is smaller than the required 16 bytes.");
                return result;
            }

            format.audio_format = read_u_int16_little_endian(&chunk_data_pointer[0]);
            format.channel_count = read_u_int16_little_endian(&chunk_data_pointer[2]);
            format.sample_rate = read_u_int32_little_endian(&chunk_data_pointer[4]);
            format.byte_rate = read_u_int32_little_endian(&chunk_data_pointer[8]);
            format.block_align = read_u_int16_little_endian(&chunk_data_pointer[12]);
            format.bits_per_sample = read_u_int16_little_endian(&chunk_data_pointer[14]);

            has_parsed_fmt = true;
        } else if (memory_equals(chunk_id_pointer, "data", 4)) {
            if (has_parsed_data) {
                continue;
            }

            audio_data_pointer = chunk_data_pointer;
            audio_data_length = chunk_length;

            has_parsed_data = true;
        }
    }

    ///////////////////////////////////////////////
    /// NOTE: validation
    ///////////////////////////////////////////////

    bool has_required_chunks = (has_parsed_fmt && has_parsed_data);
    if (!has_required_chunks) {
        memory_stream_write_line(error_stream, "error: missing required 'fmt ' or 'data' chunks.");
        return result;
    }

    bool is_pcm_format = (format.audio_format == pcm_audio_format);
    if (!is_pcm_format) {
        memory_stream_write_line(error_stream, "error: only uncompressed PCM WAV files are supported.");
        return result;
    }

    bool is_sane_channel_count = (format.channel_count == 1 || format.channel_count == 2);
    if (!is_sane_channel_count) {
        memory_stream_write_line(error_stream, "error: unsupported channel count (expected 1 or 2).");
        return result;
    }

    bool is_valid_bit_depth = (format.bits_per_sample == 8 || format.bits_per_sample == 16 ||
                               format.bits_per_sample == 24 || format.bits_per_sample == 32);
    if (!is_valid_bit_depth) {
        memory_stream_write_line(error_stream, "error: unsupported bit depth (expected 8, 16, 24, or 32).");
        return result;
    }

    u32 expected_block_align = (format.channel_count * format.bits_per_sample) / 8;
    if (format.block_align != expected_block_align) {
        memory_stream_write_line(error_stream, "error: block align does not match bits per sample and channels.");
        return result;
    }

    ///

    void *raw_sample_buffer = MEMORY_ARENA_PUSH_BYTES(permanent_arena, audio_data_length);
    if (!raw_sample_buffer) {
        memory_stream_write_line(error_stream, "error: out of memory.");
        return result;
    }

    memory_copy_forwards(raw_sample_buffer, audio_data_pointer, audio_data_length);

    u32 bytes_per_sample = format.bits_per_sample / 8;
    u32 total_samples = audio_data_length / (format.channel_count * bytes_per_sample);

    result.sample_rate = format.sample_rate;
    result.channel_count = format.channel_count;
    result.bits_per_sample = format.bits_per_sample;
    result.sample_count = total_samples;
    result.samples = raw_sample_buffer;

    return result;
}
