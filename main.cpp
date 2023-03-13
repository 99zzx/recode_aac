#include <stdio.h>
#include <iostream>
extern "C" {
    #include "libavcodec/avcodec.h"
    #include "libavdevice/avdevice.h"
    #include "libavutil/common.h"
    #include "libavutil/frame.h"
    #include "libavformat/avformat.h"
}
#include "aacDecode.hh"
using namespace std;

#define AAC_FILE "outfile.aac"
#define OUT_FILE "outfile.pcm"

void pcmLiftToLR(uint8_t *data, size_t size);

int encodec_frame_to_packet(AVCodecContext *cod_ctx, AVFrame *frame, AVPacket *packet)
{
    int send_ret = avcodec_send_frame(cod_ctx, frame);
    if (send_ret == AVERROR_EOF || send_ret == AVERROR(EAGAIN))
    {
        return EAGAIN;
    }
    else if (send_ret < 0)
    {
        printf("failed to send frame to encoder\n");
        return EINVAL;
    }

    int receive_ret = avcodec_receive_packet(cod_ctx, packet);
    if (receive_ret == AVERROR_EOF || receive_ret == AVERROR(EAGAIN))
    {
        return EAGAIN;
    }
    else if (receive_ret < 0)
    {
        printf("failed to receive frame frome encoder\n");
        return EINVAL;
    }
    return 0;
}

#define SOUND_CARD "default"

int main(int argc, char *argv[])
{
    // 初始化设备
    avdevice_register_all();
    AVInputFormat *in_fmt = av_find_input_format("alsa");
    AVFormatContext *fmt_ctx = avformat_alloc_context();
    FILE *aacfp = fopen("outfile.aac", "w+");
    AVFrame *frame = av_frame_alloc();
    do
    {

        if (avformat_open_input(&fmt_ctx, SOUND_CARD, in_fmt, NULL) < 0)
        {
            printf("failed to open input stream.default.\n");
            goto _Error;
        }
        if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
        {
            printf("failed to find stream info\n");
            goto _Error;
        }

        // 查找流信息
        int stream_index = -1;
        stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
        if (stream_index < 0)
        {
            printf("failed to find stream_index\n");
            goto _Error;
        }
        av_dump_format(fmt_ctx, stream_index, SOUND_CARD, 0);

        // 编码器初始化
        AVCodec *cod = avcodec_find_encoder_by_name("libfdk_aac");
        AVCodecContext *cod_ctx = avcodec_alloc_context3(cod);
        cod_ctx->profile = FF_PROFILE_AAC_HE_V2;
        cod_ctx->codec_type = AVMEDIA_TYPE_AUDIO;
        cod_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
        cod_ctx->sample_rate = 48000;
        cod_ctx->channels = 2;
        cod_ctx->channel_layout = av_get_channel_layout("stereo");
        if (avcodec_open2(cod_ctx, cod, NULL) < 0)
        {
            printf("failed to open codec\n");
            goto _Error;
        }
        printf("frame_size:%d\n", cod_ctx->frame_size);

        // 一帧音频采样数据
        frame->format = AV_SAMPLE_FMT_S16;
        frame->channels = 2;
        frame->sample_rate = 48000;
        frame->channel_layout = av_get_channel_layout("stereo");
        frame->nb_samples = cod_ctx->frame_size; // 1024

        int pcmbuf_size = cod_ctx->frame_size * 2 * cod_ctx->channels;
        const uint8_t *pcmbuffer = (const uint8_t *)malloc(pcmbuf_size);
        avcodec_fill_audio_frame(frame, cod_ctx->channels, cod_ctx->sample_fmt, pcmbuffer, pcmbuf_size, 1);

        AVPacket *sound_packet = av_packet_alloc();
        AVPacket *packet = av_packet_alloc();

        int i = 0, count = 0;
        while (i < 100)
        {
            // sound_packet->size = 64 字节
            if (av_read_frame(fmt_ctx, sound_packet) < 0)
            {
                printf("capture pcm data failed\n");
                break;
            }
            // 累积到一帧（nb_samples x 4 字节），再送到编码器进行编码
            if (count + sound_packet->size <= pcmbuf_size)
            {
                memcpy((void *)(pcmbuffer + count), sound_packet->data, sound_packet->size);
                count += sound_packet->size;
                av_packet_unref(sound_packet);
            }
            else
            {
                pcmLiftToLR((uint8_t *)pcmbuffer, count);
                if (encodec_frame_to_packet(cod_ctx, frame, packet) < 0)
                    break;
                frame->pts = i;
                i++;
                printf("encode %d frame, frame size:%d packet szie:%d\n", i, count, packet->size);
                fwrite(packet->data, 1, packet->size, aacfp);
                av_packet_unref(packet);
                count = 0;
            }
        }
    } while (0);

    AudioDecodeSpec spec;
    FFmpegUtils::aacDecode(AAC_FILE, OUT_FILE, spec);
    cout << "采样率：" << spec.sampleRate << endl;
    cout << "采样格式：" << av_get_sample_fmt_name(spec.sampleFmt) << endl;
    cout << "声道数：" << av_get_channel_layout_nb_channels(spec.chLayout) << endl;
_Error:

    if (aacfp)
    {
        fflush(aacfp);
        fclose(aacfp);
    }

    // 释放资源
    if (frame)
    {
        av_frame_free(&frame);
    }
    return 0;
}

void pcmLiftToLR(uint8_t *data, size_t size)
{
    for (auto i = 0; i <= size; i += 4)
    {
        memccpy(data + i + 2, data + i, 1, 2);
    }
}