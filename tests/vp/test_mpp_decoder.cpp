#include "module/vi/module_fileReader.hpp"
#include "module/vp/module_mppdec.hpp"
#include "tests/media_channel_dump.hpp"

#include <atomic>
#include <chrono>
#include <unistd.h>

using namespace FFMedia;
using namespace std;

std::atomic_bool is_exit(false);

void output_callback(const string& name, int queue_size,
                     shared_ptr<MediaBuffer> buffer)
{
    static uint64_t frame_count;
    static auto start_time = std::chrono::high_resolution_clock::now();
    if (++frame_count % 100 == 0) {
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now() - start_time)
                        .count();
        ff_info("%s: queue %d, decoded %ld frames, %ld ms, %ld fps\n",
                name.c_str(), queue_size, frame_count, diff,
                frame_count * 1000 / diff);
    }
}

void status_change_callback(const string& name, MediaStatus status)
{
    ff_info("%s status changed to %d\n", name.c_str(), static_cast<int>(status));
    if (status == MediaStatus::EOS)
        is_exit = true;
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
    f_reader->setMediaStatusChangeHooker(status_change_callback);
    ret = f_reader->init();
    if (ret < 0) {
        ff_error("Failed to init file reader, %d\n", ret);
        return ret;
    }

    // Print stream information through the reader output channels.
    dumpOutputMediaChannels(*f_reader);

    /* Create a mpp decoder module. */
    auto v_dec = make_shared<ModuleMppDec>();
    ret = v_dec->connectProducer(f_reader);
    if (ret < 0) {
        ff_error("Failed to connect file reader to mpp decoder, %d\n", ret);
        return ret;
    }
    v_dec->setBufferCount(20);
    v_dec->setMediaBufferProduceHooker(output_callback);
    ret = v_dec->init();
    if (ret < 0) {
        ff_error("Failed to init mpp decoder, %d\n", ret);
        return ret;
    }
    dumpOutputMediaChannels(*v_dec);

    ff_info("\n========================================================\n\n");

    // start
    f_reader->start();
    while (!is_exit) {
        sleep(1);
    }
    f_reader->stop();
    ff_info("Exit.\n");
    return 0;
}
