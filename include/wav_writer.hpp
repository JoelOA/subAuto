#pragma once
#include <string>
#include <vector>

bool write_wav_file(const std::string &output_filename,
                    const std::vector<float> &samples,
                    int sample_rate,
                    int num_channels);