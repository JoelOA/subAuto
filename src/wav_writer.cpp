#include "wav_writer.hpp"
#include <fstream>
#include <cstdint>

// struct wav_header {
//     uint8_t riff_magic[4];
//     uint32_t chunk_size;
//     uint8_t wave_magic[4];

//     uint8_t fmt_magic[4];
//     uint32_t fmt_size;
//     uint16_t format;
//     uint16_t nchannels;
//     uint32_t sample_rate;
//     uint32_t byte_rate;
//     uint16_t block_align;
//     uint16_t bits_per_sample;

//     uint8_t data_magic[4];
//     uint32_t data_size;
// };

// struct wav_writer {
//     struct wav_header header;
//     FILE* file;
//     uint32_t data_size;
// };

bool write_wav_file(const std::string &out_path,
                            const std::vector<float> &samples,
                            int sample_rate,
                            int num_channels) {
    std::ofstream out(out_path, std::ios::binary);
    if (!out) {
        return false;
    }
 
    const uint16_t bits_per_sample = 16;
    const uint16_t audio_format = 1; // 1 = PCM (integer)
    const uint32_t num_samples = static_cast<uint32_t>(samples.size());
    const uint32_t data_bytes = num_samples * (bits_per_sample / 8);
    const uint32_t byte_rate = sample_rate * num_channels * (bits_per_sample / 8);
    const uint16_t block_align = static_cast<uint16_t>(num_channels * (bits_per_sample / 8));
    const uint32_t riff_chunk_size = 36 + data_bytes; // 36 = header size minus "RIFF"+size+"WAVE"
 
    // ---- RIFF chunk descriptor ----
    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char *>(&riff_chunk_size), 4);
    out.write("WAVE", 4);
 
    // ---- fmt subchunk ----
    out.write("fmt ", 4);
    uint32_t fmt_chunk_size = 16; // 16 for PCM
    out.write(reinterpret_cast<const char *>(&fmt_chunk_size), 4);
    out.write(reinterpret_cast<const char *>(&audio_format), 2);
    uint16_t channels16 = static_cast<uint16_t>(num_channels);
    out.write(reinterpret_cast<const char *>(&channels16), 2);
    uint32_t rate32 = static_cast<uint32_t>(sample_rate);
    out.write(reinterpret_cast<const char *>(&rate32), 4);
    out.write(reinterpret_cast<const char *>(&byte_rate), 4);
    out.write(reinterpret_cast<const char *>(&block_align), 2);
    out.write(reinterpret_cast<const char *>(&bits_per_sample), 2);
 
    // ---- data subchunk ----
    out.write("data", 4);
    out.write(reinterpret_cast<const char *>(&data_bytes), 4);
 
    // Convert float32 [-1.0, 1.0] -> int16 and write sample-by-sample.
    for (float f : samples) {
        // clamp to avoid overflow wraparound on out-of-range input
        if (f > 1.0f) f = 1.0f;
        if (f < -1.0f) f = -1.0f;
        int16_t s = static_cast<int16_t>(f * 32767.0f);
        out.write(reinterpret_cast<const char *>(&s), sizeof(s));
    }
 
    out.close();
    return true;
}