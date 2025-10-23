#include "module/vi/module_fileReader.hpp"

void output_callback(void* _ctx, shared_ptr<MediaBuffer> buffer)
{
    auto buffer_type = buffer->getMediaBufferType();

    if (buffer_type == BUFFER_TYPE_VIDEO) {
        ff_info("Video[%d] pts %ld, dts %ld, data %p, bytes %ld\n", buffer->getMediaCodec(),
                buffer->getPUstimestamp(), buffer->getDUstimestamp(),
                buffer->getActiveData(), buffer->getActiveSize());

        auto param = buffer->getImagePara();
        ff_info("Video foramt %s, width %d, height %d\n\n", v4l2GetFmtName(param.v4l2Fmt),
                param.width, param.height);
    } else if (buffer_type == BUFFER_TYPE_AUDIO) {
        ff_info("Audio[%d] pts %ld, dts %ld, data %p, bytes %ld\n", buffer->getMediaCodec(),
                buffer->getPUstimestamp(), buffer->getDUstimestamp(),
                buffer->getActiveData(), buffer->getActiveSize());

        auto param = buffer->getSamplePara();
        ff_info("Audio foramt %d, channels %d, sample_rate %d, nb_samples %d\n\n", param.fmt,
                param.channels, param.sample_rate, param.nb_samples);

    } else {
        ff_info("ETC[%d] pts %ld, dts %ld, data %p, bytes %ld\n\n", buffer->getMediaCodec(),
                buffer->getPUstimestamp(), buffer->getDUstimestamp(),
                buffer->getActiveData(), buffer->getActiveSize());
    }
}

void status_change_callback(void* ctx, ModuleStatus status)
{
    ff_info("Module state has changed(%d)\n", status);
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
    f_reader->setOutputDataCallback(nullptr, output_callback);
    f_reader->setStatusChangeCallback(nullptr, status_change_callback);
    ret = f_reader->init();
    if (ret < 0) {
        ff_error("Failed to init file reader, %d\n", ret);
        return ret;
    }

    // Print the video stream information
    auto v_extra = f_reader->getExtraBuffer(BUFFER_TYPE_VIDEO);
    if (v_extra) {
        ff_info("Video: Extra codec %d , data %p, bytes %ld\n", v_extra->getMediaCodec(),
                v_extra->getActiveData(), v_extra->getActiveSize());

        auto param = v_extra->getImagePara();
        ff_info("foramt %s, width %d, height %d\n", v4l2GetFmtName(param.v4l2Fmt),
                param.width, param.height);
    }

    // Print the audio stream information
    auto a_extra = f_reader->getExtraBuffer(BUFFER_TYPE_AUDIO);
    if (a_extra) {
        ff_info("Audio: Extra codec %d , data %p, bytes %ld\n", a_extra->getMediaCodec(),
                a_extra->getActiveData(), a_extra->getActiveSize());
        auto param = a_extra->getSamplePara();
        ff_info("foramt %d, channels %d, sample_rate %d, nb_samples %d\n", param.fmt,
                param.channels, param.sample_rate, param.nb_samples);
    }

    ff_info("\n===========================================================================\n\n");

    // start
    f_reader->start();
    getchar();
    f_reader->stop();
    return 0;
}
