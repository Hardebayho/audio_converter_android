//
// Created by adebayo on 3/23/21.
//

#ifndef AUDIOCONVERTER_AUDIOCONVERTER_H
#define AUDIOCONVERTER_AUDIOCONVERTER_H

#include <utility>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
}
#include <string>
#include <memory>
#include <map>
#include <algorithm>
#include <android/log.h>

namespace audio_convert {
    class AudioConverter {
    public:
        explicit AudioConverter(std::string input) : input(std::move(input)) {
            __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "%s", "Codecs:");
            const AVCodec* codec{nullptr};
            void* opaque{nullptr};
            while ((codec = av_codec_iterate(&opaque))) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "Name: %s, Long name: %s, Profile name: %s", codec->name, codec->long_name, codec->profiles->name);
            }
        }
        bool initialize() {
            reset();
            initialized = false;
            auto* val = input.c_str();
            int ret = avformat_open_input(&format_ctx, val, nullptr, nullptr);
            if (ret < 0) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "avformat_open_input failed! Path: %s", val);
                return false;
            }
            ret = avformat_find_stream_info(format_ctx, nullptr);
            if (ret < 0) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "avformat_find_stream_info failed!");
                return false;
            }

            // Find the decoder for this input format.
            AVCodec* decoder{nullptr};
            stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
            if (stream_index < 0) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "No stream found in file!");
                return false;
            }

            AVCodecContext* decoder_ctx = avcodec_alloc_context3(decoder);
            if (!decoder_ctx) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "avcodec_alloc_context3 failed");
                return false;
            }

            ret = avcodec_parameters_to_context(decoder_ctx, format_ctx->streams[stream_index]->codecpar);
            if (ret < 0) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "avcodec_parameters_to_context failed!");
                return false;
            }

            ret = avcodec_open2(decoder_ctx, decoder, nullptr);
            if (ret < 0) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "avcodec_open2 failed!");
                return false;
            } else {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "Opened the codec!");
            }

            input_decoder.reset(decoder_ctx, [](AVCodecContext* ctx) { avcodec_free_context(&ctx); });

            initialized = true;
            return initialized;
        }

        bool is_initialized() { return initialized; }

        int get_percentage() const {
            return percentage;
        }

        bool convert(AVCodecID output_codec, const std::string& output_path, const std::map<std::string, std::string>& metadata = std::map<std::string, std::string>()) {
            if (!initialized) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "Not initialized!");
                return false;
            }
            percentage = 0;
            AVFormatContext* output_format{nullptr};
            int ret = avformat_alloc_output_context2(&output_format, nullptr, nullptr, output_path.c_str());
            if (ret < 0) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "Unable to allocate output context!");
                return false;
            }
            AVCodec* encoder{nullptr};
            if (output_codec == AV_CODEC_ID_OPUS) {
                encoder = avcodec_find_encoder_by_name("libopus");
            }
            else encoder = avcodec_find_encoder(output_codec);
            if (!encoder) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "Unable to find requested encoder!");
                return false;
            } else {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "Found the encoder");
            }
            if (!encoder->sample_fmts) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "No supported sample format found!");
                return false;
            }
            AVSampleFormat fmt = *encoder->sample_fmts;
            int out_sample_rate = input_decoder->sample_rate;

            resampler_ctx.reset(swr_alloc_set_opts(nullptr, input_decoder->channel_layout, fmt, out_sample_rate, input_decoder->channel_layout, input_decoder->sample_fmt, input_decoder->sample_rate, 0, nullptr), [](SwrContext* ctx) { swr_free(&ctx); });

            fifo.reset(av_audio_fifo_alloc(fmt, input_decoder->channels, 20000), av_audio_fifo_free);
            if (!fifo) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "av_audio_fifo_alloc failed!");
                return false;
            }

            std::shared_ptr<AVCodecContext> output_encoder{avcodec_alloc_context3(encoder), [](AVCodecContext* ctx) { avcodec_free_context(&ctx); }};
            if (!output_encoder) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "avcodec_alloc_context3 failed!");
                return false;
            } else {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "Got the output encoder");
            }

            AVStream* new_stream = avformat_new_stream(output_format, encoder);
            new_stream->time_base = {1, AV_TIME_BASE};

            output_encoder->bit_rate = input_decoder->bit_rate;
            output_encoder->sample_fmt = fmt;
            output_encoder->sample_rate = out_sample_rate;
            output_encoder->channel_layout = input_decoder->channel_layout;
            output_encoder->channels = input_decoder->channels;
            output_encoder->time_base = input_decoder->time_base;
            if (avcodec_open2(output_encoder.get(), encoder, nullptr) < 0) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "avcodec_open2 failed!");
                return false;
            }

            if (output_format->oformat->flags & AVFMT_GLOBALHEADER) {
                output_encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }

            if (avcodec_parameters_from_context(new_stream->codecpar, output_encoder.get()) < 0) {
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "avcodec_parameters_from_context failed!");
                return false;
            }

            av_dict_copy(&output_format->metadata, format_ctx->metadata, 0);
            new_stream->time_base = output_encoder->time_base;
            if (!(output_format->oformat->flags & AVFMT_NOFILE)) {
                ret = avio_open(&output_format->pb, output_path.c_str(), AVIO_FLAG_WRITE);
                if (ret < 0) {
                    __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "Couldn't open output file!");
                    return false;
                }
            }

            std::for_each(metadata.begin(), metadata.end(), [&](std::pair<std::string, std::string> data) {
                av_dict_set(&output_format->metadata, data.first.c_str(), data.second.c_str(), 0);
            });

            if ((ret = avformat_write_header(output_format, nullptr)) < 0) {
                fprintf(stderr, "Unable to write header! Error: %d\n", ret);
                __android_log_print(ANDROID_LOG_DEBUG, "AudioConverterNative", "avformat_write_header failed!");
                return false;
            }

            // 1. Decode all the audio packets into memory
            AVPacket* packet = av_packet_alloc();
            AVFrame* frame = av_frame_alloc();

            while (av_read_frame(format_ctx, packet) >= 0) {
                if (packet->stream_index != stream_index) continue;
                avcodec_send_packet(input_decoder.get(), packet);
                while (avcodec_receive_frame(input_decoder.get(), frame) >= 0) {
                    AVFrame* resampled_frame = av_frame_alloc();
                    resampled_frame->pts = frame->pts;
                    resampled_frame->format = fmt;
                    resampled_frame->channels = frame->channels;
                    resampled_frame->channel_layout = frame->channel_layout;
                    resampled_frame->nb_samples = frame->nb_samples;
                    resampled_frame->sample_rate = frame->sample_rate;

                    swr_convert_frame(resampler_ctx.get(), resampled_frame, frame);
                    av_audio_fifo_write(fifo.get(), (void**)resampled_frame->data, resampled_frame->nb_samples);
                    av_frame_free(&resampled_frame);
                }
            }
            // Try to flush the decoder here
            if (avcodec_send_packet(input_decoder.get(), nullptr) >= 0) {
                while (av_read_frame(format_ctx, packet) >= 0) {
                    avcodec_send_packet(input_decoder.get(), packet);
                    while (avcodec_receive_frame(input_decoder.get(), frame) >= 0) {
                        AVFrame* resampled_frame = av_frame_alloc();
                        resampled_frame->pts = frame->pts;
                        resampled_frame->format = fmt;
                        resampled_frame->channels = frame->channels;
                        resampled_frame->channel_layout = frame->channel_layout;
                        resampled_frame->nb_samples = frame->nb_samples;
                        resampled_frame->sample_rate = frame->sample_rate;

                        swr_convert_frame(resampler_ctx.get(), resampled_frame, frame);
                        av_audio_fifo_write(fifo.get(), (void**)resampled_frame->data, resampled_frame->nb_samples);
                        av_frame_free(&resampled_frame);
                    }
                }
            }

            int pts = 0;

            // 2. Send frames to the encoder
            frame->sample_rate = output_encoder->sample_rate;
            frame->format = fmt;
            frame->nb_samples = output_encoder->frame_size > 0 ? output_encoder->frame_size : 10000;
            frame->channel_layout = output_encoder->channel_layout;
            av_frame_get_buffer(frame, 0);
            while ((frame->nb_samples = av_audio_fifo_read(fifo.get(), (void**)frame->data, frame->nb_samples)) > 0) {
                frame->pts = pts;
                pts += (int) ((frame->nb_samples / (double)frame->sample_rate) * new_stream->time_base.den);
                // Send frame to encoder
                while (avcodec_send_frame(output_encoder.get(), frame) < 0) {
                    // Try to retrieve packets
                    while (avcodec_receive_packet(output_encoder.get(), packet) >= 0) {
                        av_interleaved_write_frame(output_format, packet);
                    }
                }
            }
            if (avcodec_send_frame(output_encoder.get(), nullptr) >= 0) {
                while (avcodec_receive_packet(output_encoder.get(), packet) >= 0) {
                    av_interleaved_write_frame(output_format, packet);
                }
            }
            // Flush the output
            av_interleaved_write_frame(output_format, nullptr);

            // Write the trailer
            if (av_write_trailer(output_format) < 0) return false;
            avformat_free_context(output_format);

            av_packet_free(&packet);
            av_frame_free(&frame);
            return true;
        }

        void reset() {
            initialized = false;
            avformat_close_input(&format_ctx);
        }

        ~AudioConverter() {
            reset();
        }

    private:
        AVFormatContext* format_ctx{nullptr};
        std::shared_ptr<AVCodecContext> input_decoder{nullptr};
        std::shared_ptr<SwrContext> resampler_ctx{nullptr};
        bool initialized{false};
        int stream_index{-1};
        std::string input;
        std::shared_ptr<AVAudioFifo> fifo{nullptr};
        int percentage = 0;
    };
}

#endif //AUDIOCONVERTER_AUDIOCONVERTER_H
