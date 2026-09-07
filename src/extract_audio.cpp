#include <stdio.h>
#include <iostream>
#include <libavutil/error.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>


AVFormatContext* format_context = nullptr;
const AVCodec *codec = NULL;
AVCodecContext *codec_context = nullptr;
AVStream *stream = nullptr;
AVPacket *packet = av_packet_alloc();
AVFrame *frame = av_frame_alloc();


int ret = 0;
const int STREAM_IDX = 0; // Find a way to determine the correct stream for audio
const int TIME_BASE_CUTOFF = 90000;

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

    codec_context->request_sample_fmt = AV_SAMPLE_FMT_S16;

    ret = avcodec_open2(codec_context, codec, NULL);
    if (ret < 0) {
        std::cerr << "Failed to open codec" << std::endl;
    }
    return 0;
}