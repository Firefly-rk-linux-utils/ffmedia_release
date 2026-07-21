#include "module/vi/module_fileReader.hpp"
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
        ff_error("\nUsage: %s file\n", argv[0]);
        return -1;
    };

    // create file reader module
    auto f_reader = make_shared<ModuleFileReader>(argv[1]);
    /* Set the callback function for the reader */
    f_reader->setMediaBufferProduceHooker(dumpMediaBufferBrief);
    f_reader->setMediaStatusChangeHooker(status_change_callback);
    ret = f_reader->init();
    if (ret < 0) {
        ff_error("Failed to init file reader, %d\n", ret);
        return ret;
    }

    // Print stream information through the reader output channels.
    dumpOutputMediaChannels(*f_reader);

    ff_info("\n===========================================================================\n\n");

    // start
    f_reader->start();
    getchar();
    f_reader->stop();
    return 0;
}
