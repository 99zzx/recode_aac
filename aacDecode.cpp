#include <iostream>
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
}
#include "aacDecode.hh"
using namespace std;

#define ERRBUF(ret) \
    char errbuf[1024]; \
    av_strerror(ret, errbuf, sizeof (errbuf))

// 输入缓冲区大小
#define AUDIO_INBUF_SIZE 20480

// 需要再次读取输入文件数据的阈值
#define AUDIO_REFILL_THRESH 4096

FFmpegUtils::FFmpegUtils()
{

}

static int decode(AVCodecContext *ctx, AVPacket *pkt, AVFrame *frame, FILE *outFile)
{
    int ret = 0;
    ret = avcodec_send_packet(ctx, pkt);
    if (ret < 0) {
        ERRBUF(ret);
        cout << "avcodec_send_packet error:" << errbuf;
        return ret;
    }

    while (true) {
        ret = avcodec_receive_frame(ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        } else if (ret < 0) {
            ERRBUF(ret);
            cout << "avcodec_receive_frame error:" << errbuf;
            return ret;
        }

        // 如果 avcodec_send_packet 的数据是planar格式，但是希望写入文件的数据不是planar格式，此处需要修改
		fwrite((const char *)frame->data[0], 1, frame->linesize[0], outFile);
    }
}

void FFmpegUtils::aacDecode(const char *inFilename, const char *outFilename, AudioDecodeSpec &out)
{
    // 返回值
    int ret = 0;

	FILE *inFile = fopen(inFilename, "rb");
	FILE *outFile = fopen(outFilename, "wb");

    // 解码器
    AVCodec *codec = nullptr;
    // 解码上下文
    AVCodecContext *ctx = nullptr;
    // 解析器上下文  1、和编码不太一样，多了一个解析器上下文
    AVCodecParserContext *parserCtx = nullptr;

    // 存放解码前的数据(aac)
    AVPacket *pkt = nullptr;
    // 存放解码后的数据(pcm)
    AVFrame *frame = nullptr;

    // 存放读取的输入文件数据(aac)
    // AV_INPUT_BUFFER_PADDING_SIZE
    char inDataArray[AUDIO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    char *inData = inDataArray;

    // 读取的数据长度(aac)
    int inLen = 0;

    // 是否读取到了输入文件的尾部
    int inEnd = 0;

    // 获取解码器
    codec = avcodec_find_decoder_by_name("libfdk_aac");
    if (!codec) {
        cout << "decoder libfdk_aac not found" << endl;
        return;
    }

    // 初始化解析器上下文
    parserCtx = av_parser_init(codec->id);
    if (!parserCtx) {
        cout << "av_parser_init error" << endl;
        return;
    }

    // 创建上下文
    ctx = avcodec_alloc_context3(codec);
    if (!ctx) {
        cout << "avcodec_alloc_context3 error" << endl;
        goto end;
    }

    // 创建AVPacket
    pkt = av_packet_alloc();
    if (!pkt) {
        cout << "av_packet_alloc error" << endl;
        goto end;
    }

    // 创建AVFrame  2、和编码不太一样，编码的时候我们设置了一些 frame 参数，让 frame 有一个缓冲区的大小
    frame = av_frame_alloc();
    if (!frame) {
        cout << "av_frame_alloc error" << endl;
        goto end;
    }

    // 打开解码器
    // options 打开解码器的时候我们可以传递一些解码器参数或者解码器特有参数
    ret = avcodec_open2(ctx, codec, nullptr);

    if (ret < 0) {
        ERRBUF(ret);
        cout << "open decoder error:" << errbuf << endl;
        goto end;
    }

    // 打开文件
    if (!inFile) {
        cout << "open file failure:" << inFilename << endl;
        goto end;
    }

    if (!outFile) {
        cout << "open file failure:" << outFilename << endl;
    }

    // 读取数据
	inLen = fread(inData, 1, AUDIO_INBUF_SIZE, inFile);

    while (inLen > 0) {
        ret = av_parser_parse2(parserCtx, ctx, &pkt->data, &pkt->size, (const uint8_t *)inData, inLen, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

        if (ret < 0) {
            ERRBUF(ret);
            cout << "av_parser_parse2 error:" << errbuf << endl;
            goto end;
        }

        inData += ret;
        inLen -= ret;

        if (pkt->size > 0 && decode(ctx, pkt, frame, outFile) < 0) {
            goto end;
        }

        if (inLen < AUDIO_REFILL_THRESH && !inEnd) {
            memmove(inDataArray, inData, inLen);
            inData = inDataArray;
            int len = fread(inData + inLen, 1, AUDIO_INBUF_SIZE - inLen, inFile);
            if (len > 0) {
                inLen += len;
            } else {
                inEnd = 1;
            }
        }
    }

    decode(ctx, nullptr, frame, outFile);

    out.sampleRate = ctx->sample_rate;
    out.sampleFmt = ctx->sample_fmt;
    out.chLayout = ctx->channel_layout;

end:
	fclose(outFile);
	fclose(inFile);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_parser_close(parserCtx);
    avcodec_free_context(&ctx);
}
