#include "module/vi/module_rtspClient.hpp"
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

    // create rtsp client module
    auto rtsp_c = make_shared<ModuleRtspClient>(argv[1]);
    /* Set the callback function for the rtsp */
    rtsp_c->setMediaBufferProduceHooker(dumpMediaBufferBrief);
    rtsp_c->setMediaStatusChangeHooker(status_change_callback);
    ret = rtsp_c->init();
    if (ret < 0) {
        ff_error("Failed to init rtsp client, %d\n", ret);
        return ret;
    }

    // Print stream information through the RTSP client output channels.
    dumpOutputMediaChannels(*rtsp_c);

    ff_info("\n===========================================================================\n\n");

    // start
    rtsp_c->start();
    getchar();
    rtsp_c->stop();
    return 0;
}
