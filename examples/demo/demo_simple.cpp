#include <cstdio>
#include <memory>

#include "module/vi/module_rtspClient.hpp"
#include "module/vo/module_drmDisplay.hpp"
#include "module/vp/module_mppdec.hpp"

using namespace FFMedia;

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "Usage: %s rtsp://user:password@host/path\n", argv[0]);
        return 1;
    }

    // 1. The source publishes every media channel found in the RTSP session.
    auto source = std::make_shared<ModuleRtspClient>(
        argv[1], RTSP_STREAM_TYPE_TCP, true, true);
    int ret = source->init();
    if (ret < 0) {
        std::printf("rtsp client init failed, ret=%d\n", ret);
        return ret;
    }

    for (const auto& channel : source->getOutputMediaChannels()) {
        std::printf("source channel=%u name=%s type=%d codec=%d\n",
                    channel.id, channel.name.c_str(), channel.media_type,
                    channel.codec);
    }

    // 2. ModuleMppDec accepts compressed video formats. connectProducer()
    // automatically ignores unrelated channels such as RTSP audio.
    auto decoder = std::make_shared<ModuleMppDec>();
    ret = decoder->connectProducer(source);
    if (ret < 0) {
        std::printf("failed to connect RTSP source to decoder, ret=%d\n", ret);
        return ret;
    }
    ret = decoder->init();
    if (ret < 0) {
        std::printf("failed to initialize decoder, ret=%d\n", ret);
        return ret;
    }

    MediaInputChannel decoder_input;
    if (decoder->getInputMediaChannel(0, decoder_input)) {
        std::printf("decoder input <- producer=%s channel=%u\n",
                    decoder_input.producer_name.c_str(),
                    decoder_input.producer_channel_id);
    }

    // 3. The display obtains image parameters from the decoder's matched raw
    // video channel during init(), so no manual ImagePara copy is required.
    auto display = std::make_shared<ModuleDrmDisplay>();
    ret = display->connectProducer(decoder);
    if (ret < 0) {
        std::printf("failed to connect decoder to DRM display, ret=%d\n", ret);
        return ret;
    }
    ret = display->init();
    if (ret < 0) {
        std::printf("failed to initialize DRM display, ret=%d\n", ret);
        return ret;
    }

    // Starting the source starts the connected processing pipeline.
    source->start();
    std::printf("Pipeline started. Press Enter to stop.\n");
    std::getchar();
    return 0;
}
