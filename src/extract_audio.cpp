#include <stdio.h>
#include <iostream>
#include <vector>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}


AVFormatContext* format_context = nullptr;
const AVCodec *codec = NULL;
AVCodecContext *codec_context = nullptr;
AVStream *stream = nullptr;
AVPacket *packet = av_packet_alloc();
AVFrame *frame = av_frame_alloc();
AVSampleFormat src_sample_format, destination_sample_format;
AVChannelLayout src_channel_layout, destination_channel_layout;

SwrContext *swr_context = NULL; 

int destination_nb_samples = 0, max_destination_nb_samples = 0;
int src_rate = 0, destination_rate = 0;
int src_nb_channels = 0, destination_nb_channels = 0;
int desination_linesize = 0;

int ret = 0;
uint8_t *scratch_buf = NULL;

int stream_idx = 0; // Find a way to determine the correct stream for audio
const double TIME_BASE_CUTOFF = 90.0;
int64_t first_pts = AV_NOPTS_VALUE;

std::vector<float> accumulated_audio_data; // Vector to accumulate audio data

int main() {
    // opening a stream to the media file to be processed
    ret = avformat_open_input(&format_context, "url://to/media.file", NULL, NULL);
    if (ret < 0) {
        char error_buffer[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(error_buffer, AV_ERROR_MAX_STRING_SIZE, ret);
        std::cerr << "Could not open input file: " << error_buffer << std::endl;
    }

    // read packets of the media file to get stream information
    ret = avformat_find_stream_info(format_context, NULL);
    if (ret < 0) {
        char error_buffer[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(error_buffer, AV_ERROR_MAX_STRING_SIZE, ret);
        std::cerr << "Could not find stream information: " << error_buffer << std::endl;
    }


    for (int index = 0; index < format_context->nb_streams; index++) {
        stream = format_context->streams[index];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            std::cout << "Found audio stream at index: " << index << std::endl;
            stream_idx = index;
            break;
        }
    }

    // find registered decoder for the audio stream with a matching codec ID
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        std::cerr << "No suitable decoder found for %s", avcodec_get_name(stream->codecpar->codec_id);
    }

    // setting the context for the codec, gives "room" to give codec more information on how to operate
    codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        std::cerr << "Failed to allocate codec context" << std::endl;
    }

    // copy codec parameters from input stream to output codec context
    ret = avcodec_parameters_to_context(codec_context, stream->codecpar);
    if (ret < 0) {
        std::cerr << "Failed to copy codec parameters" << std::endl;
    }

   //  codec_context->request_sample_fmt = AV_SAMPLE_FMT_FLTP;

    ret = avcodec_open2(codec_context, codec, NULL);
    if (ret < 0) {
        std::cerr << "Failed to open codec" << std::endl;
    }
    return 0;

    // setting parameters for extracted audio

    src_sample_format = codec_context->sample_fmt;
    destination_sample_format = AV_SAMPLE_FMT_FLT;
    src_channel_layout = codec_context->ch_layout;
    destination_channel_layout = AV_CHANNEL_LAYOUT_MONO;
    src_nb_channels = codec_context->ch_layout.nb_channels;
    destination_nb_channels = destination_channel_layout.nb_channels;
    src_rate = codec_context->sample_rate;
    destination_rate = 16000;

    accumulated_audio_data.reserve(TIME_BASE_CUTOFF * destination_rate); // Reserve space for 10 seconds of audio data

    ret = swr_alloc_set_opts2(
        &swr_context,
        &destination_channel_layout, destination_sample_format, destination_rate,
        &src_channel_layout, src_sample_format, src_rate,
        0, nullptr
    );

    if (ret < 0) {
        char error_buffer[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(error_buffer, AV_ERROR_MAX_STRING_SIZE, ret);
        std::cerr << "Failed to set SwrContext options: " << error_buffer << std::endl;
    }

    if ((ret = swr_init(swr_context)) < 0) {
        char error_buffer[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(error_buffer, AV_ERROR_MAX_STRING_SIZE, ret);
        std::cerr << "Failed to initialize SwrContext: " << error_buffer << std::endl;
    }

    while(av_read_frame(format_context, packet) >= 0) {
        // focus only on audio packets
        if (packet->stream_index != stream_idx) {
            av_packet_unref(packet);
            continue;
        }

        // packets with no PTS for audio is rare so ignore
        if (packet->pts == AV_NOPTS_VALUE) {
            std::cerr << "Packet has no PTS value. Skipping." << std::endl;
            av_packet_unref(packet);
            continue;
        }

        /*
        Some media files do not have a timestamp starting at zero
        since we are trying to read the first few seconds from when
        it starts we need to set an offset
        */
        if (first_pts == AV_NOPTS_VALUE) {
            first_pts = packet->pts;
        }

        double timestamp_sec = (packet->pts - first_pts) * av_q2d(stream->time_base);
        if (timestamp_sec > TIME_BASE_CUTOFF) {
            std::cout << "Reached time base cutoff of " << TIME_BASE_CUTOFF << " seconds. Stopping extraction." << std::endl;
            break;
        }

        // process the audio packet
        if ((ret = avcodec_send_packet(codec_context, packet)) < 0) {
            char error_buffer[AV_ERROR_MAX_STRING_SIZE];
            av_make_error_string(error_buffer, AV_ERROR_MAX_STRING_SIZE, ret);
            std::cerr << "Failed to send packet to decoder: " << error_buffer << std::endl;
            break; 
        }

        while ((ret = avcodec_receive_frame(codec_context, frame)) >= 0) {
            // process the decoded audio frame
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                char error_buffer[AV_ERROR_MAX_STRING_SIZE];
                av_make_error_string(error_buffer, AV_ERROR_MAX_STRING_SIZE, ret);
                std::cerr << "Error during decoding: " << error_buffer << std::endl;
                break;
            }
            // For example, you can resample or convert the audio data here
            destination_nb_samples = swr_get_out_samples(swr_context, frame->nb_samples);
            int scratch_linesize = 0;
            int ret =av_samples_alloc(&scratch_buf, &scratch_linesize, destination_nb_channels, destination_nb_samples, destination_sample_format, 0);

            if (ret < 0) {
                char error_buffer[AV_ERROR_MAX_STRING_SIZE];
                av_make_error_string(error_buffer, AV_ERROR_MAX_STRING_SIZE, ret);
                std::cerr << "Failed to allocate scratch buffer: " << error_buffer << std::endl;
                continue;; // or continue ?
            }

            int converted_samples = swr_convert(swr_context, &scratch_buf, destination_nb_samples, (const uint8_t **)frame->extended_data, frame->nb_samples);
            if (converted_samples < 0) {
                char error_buffer[AV_ERROR_MAX_STRING_SIZE];
                av_make_error_string(error_buffer, AV_ERROR_MAX_STRING_SIZE, converted_samples);
                std::cerr << "Error during resampling: " << error_buffer << std::endl;
                continue; // or continue ?
            } else {
                float *samples_ptr = (float *)scratch_buf;
                accumulated_audio_data.insert(accumulated_audio_data.end(), samples_ptr, samples_ptr + converted_samples * destination_nb_channels);
                av_freep(&scratch_buf); // Free the scratch buffer after use
            }
        }

        av_packet_unref(packet);
    }

}