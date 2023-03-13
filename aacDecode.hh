#ifndef __AACDECODE_H__
#define __AACDECODE_H__  

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

extern "C" {
    #include <libavformat/avformat.h>
}

// 解码后的 PCM 参数
typedef struct {
    int sampleRate;
    AVSampleFormat sampleFmt;
    int chLayout;
} AudioDecodeSpec;

class FFmpegUtils
{
public:
    FFmpegUtils();
    static void aacDecode(const char *inFilename, const char *outFilename, AudioDecodeSpec &out);
};
#endif // !__AACDECODE_H__ 