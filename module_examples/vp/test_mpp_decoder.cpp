#include "module/vi/module_fileReader.hpp"
#include "module/vp/module_mppdec.hpp"

std::atomic_bool is_exit(false);

void output_callback(void* _ctx, shared_ptr<MediaBuffer> buffer)
{
    static uint64_t frame_count;
    static auto start_time = std::chrono::high_resolution_clock::now();
    if (++frame_count % 100 == 0) {
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now() - start_time)
                        .count();
        ff_info("decode %ld frames time %ld ms fps %ld \n",
                frame_count, diff, frame_count * 1000 / diff);
    }
}

void status_change_callback(void* ctx, ModuleStatus status)
{
    ff_info("Module state has changed(%d)\n", status);
    if (status == ModuleStatus::STATUS_EOS)
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
    f_reader->setStatusChangeCallback(nullptr, status_change_callback);
    ret = f_reader->init();
    if (ret < 0) {
        ff_error("Failed to init file reader, %d\n", ret);
        return ret;
    }

    // Print the video stream information
    auto v_extra = f_reader->getExtraBuffer(BUFFER_TYPE_VIDEO);
    if (v_extra) {
        ff_info("Video: Extra codec %d , data %p, bytes %ld\n",
                v_extra->getMediaCodec(), v_extra->getActiveData(),
                v_extra->getActiveSize());

        auto param = v_extra->getImagePara();
        ff_info("foramt %s, width %d, height %d\n",
                v4l2GetFmtName(param.v4l2Fmt), param.width,
                param.height);
    }

    // Print the audio stream information
    auto a_extra = f_reader->getExtraBuffer(BUFFER_TYPE_AUDIO);
    if (a_extra) {
        ff_info("Audio: Extra codec %d , data %p, bytes %ld\n",
                a_extra->getMediaCodec(), a_extra->getActiveData(),
                a_extra->getActiveSize());
        auto param = a_extra->getSamplePara();
        ff_info("foramt %d, channels %d, sample_rate %d, nb_samples %d\n",
                param.fmt, param.channels, param.sample_rate,
                param.nb_samples);
    }

    /* Create a mpp decoder module. */
    auto v_dec = make_shared<ModuleMppDec>();
    v_dec->setProductor(f_reader);
    v_dec->setBufferCount(20);
    v_dec->setOutputDataCallback(nullptr, output_callback);
    ret = v_dec->init();
    if (ret < 0) {
        ff_error("Failed to init mpp decoder, %d\n", ret);
        return ret;
    }

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
