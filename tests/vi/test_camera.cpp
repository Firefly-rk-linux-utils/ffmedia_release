#include "module/vi/module_cam.hpp"
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
        ff_error("\nUsage: %s /dev/videoX nv12 1920 1080\n", argv[0]);
        return -1;
    };

    ImagePara param;
    if (argc > 4) {
        auto width = atoi(argv[3]);
        auto height = atoi(argv[4]);
        param = ImagePara(width, height, width, height, v4l2GetFmtByName(argv[2]));
    }

    // create camera module
    auto cam = make_shared<ModuleCam>(argv[1]);
    // try
    cam->setOutputImagePara(param);
    /* Set the callback function for the camera */
    cam->setMediaBufferProduceHooker(dumpMediaBufferBrief);
    cam->setMediaStatusChangeHooker(status_change_callback);
    ret = cam->init();
    if (ret < 0) {
        ff_error("Failed to init camera, %d\n", ret);
        return ret;
    }

    // Print stream information through the camera output channels.
    dumpOutputMediaChannels(*cam);

    ff_info("\n===========================================================================\n\n");

    // start
    cam->start();
    getchar();
    cam->stop();
    return 0;
}
