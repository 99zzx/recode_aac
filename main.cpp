#include <iostream>
#include <string> 

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavutil/common.h"
#include "libavutil/frame.h"
#include "libavformat/avformat.h"

};

using namespace std;



#define AUDIO_FORMAT_PCM 1
#define AUDIO_FORMAT_FLOAT 3

typedef struct {
    // RIFF Chunk
    uint8_t riffChunkID[4] = {'R', 'I', 'F', 'F'};
    uint32_t riffChunkSize;

    // DATA
    uint8_t format[4] = {'W', 'A', 'V', 'E'};

    // FMT Chunk
    uint8_t fmtChunkID[4] = {'f', 'm', 't', ' '};
    uint32_t fmtChunkSize = 16;

    //编码格式(音频编码)
    uint16_t audioFormat = AUDIO_FORMAT_PCM;
    //声道数
    uint16_t numChannel;
    //采样率
    uint32_t sampleRate;
    //字节率
    uint32_t byteRate;
    //一个样本的字节数
    uint16_t blockAlign;
    // 位深度
    uint16_t bitsPerSample;


    // DATA Chunk
    uint8_t dataChunkID[4] = {'d', 'a', 't', 'a'};
    uint32_t dataChunkSize;
} WAVHeader;

static void showSpec(AVFormatContext *ctx);
static void pcm2wav(uint16_t numChannle, uint32_t sampRate, uint16_t bitPerSample, const char *pcmFile, const char *wavFile);




int main(int argc, char **argv) {
    // register device
    avdevice_register_all();
    // set logger level
    av_log_set_level(AV_LOG_DEBUG);
    // get input device format
    AVInputFormat *format = av_find_input_format("alsa");
    // init context
    AVFormatContext *fmt_ctx = nullptr;
    std::string deviceName = "default";
    AVDictionary *options = nullptr;
    // open audio device
    int ret = avformat_open_input(&fmt_ctx, deviceName.c_str(), format, &options);
    if (ret < 0) {
        std::cout << "Failed to open audio device!" << std::endl;
    }
    showSpec(fmt_ctx);
    AVPacket ptk;
    av_init_packet(&ptk);
    string pcmPath = "audio.pcm";
    if (argc >= 2)
    {
        pcmPath = argv[1];
    }

    FILE *out = fopen(pcmPath.c_str(), "wb");
    int count = 0;
    while (!ret && count++ < 5000) {
        ret = av_read_frame(fmt_ctx, &ptk);
        // write file
        fwrite(ptk.data,ptk.size,1,out);
        // std::cout << "ptk.size:" << ptk.size << std::endl;
        // std::cout << "count:" << count << std::endl;
        // release ptk
        av_packet_unref(&ptk);
    }
    fflush(out);
    fclose(out);
    // close device
    avformat_close_input(&fmt_ctx);

    std::cout << "succeed recode "<< pcmPath << std::endl;

    if(argc == 3)
    {
        string outFile = argv[2];
        pcm2wav(2, 48000, 16, pcmPath.c_str(),outFile.c_str());
    }

    return 0;
}

void showSpec(AVFormatContext *ctx) {
    // 获取输入流
    AVStream *stream = ctx->streams[0];
    // 获取音频参数
    AVCodecParameters *params = stream->codecpar;
    // 声道数
    std::cout << "params->channels :" << params->channels << std::endl;
    // 采样率
    std::cout << "params->sample_rate :" << params->sample_rate << std::endl;
    // 采样格式
    std::cout << "params->format :" << params->format << std::endl;
    std::cout << "params->channel_layout :" << params->channel_layout << std::endl;
    std::cout << "params->codec_id :" << av_get_bits_per_sample(params->codec_id) << std::endl;
    // 每一个样本的一个声道占用多少个字节
    std::cout << av_get_bytes_per_sample((AVSampleFormat) params->format) << std::endl;
}

#include <sys/stat.h> 

void pcm2wav(uint16_t numChannle, uint32_t sampRate, uint16_t bitPerSample, const char *pcmFile, const char *wavFile)
{
    WAVHeader header;
    struct stat statbuf;
    stat(pcmFile, &statbuf);
    header.numChannel = numChannle;
    header.sampleRate = sampRate;
    header.bitsPerSample = bitPerSample;
    header.blockAlign = header.bitsPerSample * header.numChannel >> 3;
    header.byteRate = header.sampleRate * header.blockAlign;
    header.dataChunkSize = statbuf.st_size;
    header.riffChunkSize = header.dataChunkSize + sizeof(WAVHeader) - 8;

    FILE *infp = fopen(pcmFile, "rb");
    FILE *outfp = fopen(wavFile, "wb+");

    fwrite((const char*)&header, sizeof(WAVHeader), 1, outfp);

    char buf[1024];
    int size;

    while((size = fread(buf, 1, sizeof(buf), infp)) != 0)
    {
        fwrite((const char*)buf, 1, size, outfp);
        //std::cout << "write buf " << size << "byte" << endl;
    }

    fclose(infp);
    fflush(outfp);
    fclose(outfp);
    std::cout << "pcmFile to wavFile ok!" << std::endl;
}

