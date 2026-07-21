#include "module/vi/module_ffmpegDemux.hpp"
#include "tests/media_channel_dump.hpp"

using namespace FFMedia;
using namespace std;

void status_change_callback(const string& name, MediaStatus status)
{
    ff_info("%s status changed to %d\n", name.c_str(), static_cast<int>(status));
}

int main(int argc, char** argv)
{
    int ret;

    if (argc < 2) {
        ff_error("\nUsage: %s rtsp://xxx\n", argv[0]);
        return -1;
    };

    // create FFmpeg demux module
    auto f_demux = make_shared<ModuleFFmpegDemux>(argv[1]);
    // f_demux->setInputFormat("rtsp");
    /* Reducing probesize can speed up the opening of streams.*/
    f_demux->setFormatOption("probesize", "200K", 0);
    /* Set the callback function for the demuxer */
    f_demux->setMediaBufferProduceHooker(dumpMediaBufferBrief);
    f_demux->setMediaStatusChangeHooker(status_change_callback);
    ret = f_demux->init();
    if (ret < 0) {
        ff_error("Failed to init ffmpeg demux, %d\n", ret);
        return ret;
    }

    // Print stream information through the demuxer output channels.
    dumpOutputMediaChannels(*f_demux);

    ff_info("\n===========================================================================\n\n");

    // start
    f_demux->start();
    getchar();
    f_demux->stop();
    return 0;
}
