#include "module/vi/module_cam.hpp"

void output_callback(void* _ctx, shared_ptr<MediaBuffer> buffer)
{
    auto v_buf = dynamic_pointer_cast<VideoBuffer>(buffer);
    if (v_buf) {
        ff_info("Video[%d] pts %ld, dts %ld, fd %d, data %p, bytes %ld\n",
                v_buf->getMediaCodec(), v_buf->getPUstimestamp(),
                v_buf->getDUstimestamp(), v_buf->getBufFd(),
                v_buf->getActiveData(), v_buf->getActiveSize());
    }

    auto param = buffer->getImagePara();
    ff_info("Video foramt %s, width %d, height %d\n\n", v4l2GetFmtName(param.v4l2Fmt),
            param.width, param.height);
}

void status_change_callback(void* ctx, ModuleStatus status)
{
    ff_info("Module state has changed(%d)\n", status);
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
    cam->setOutputDataCallback(nullptr, output_callback);
    cam->setStatusChangeCallback(nullptr, status_change_callback);
    ret = cam->init();
    if (ret < 0) {
        ff_error("Failed to init camera, %d\n", ret);
        return ret;
    }

    // Print the video stream information
    param = cam->getOutputImagePara();
    ff_info("foramt %s, width %d, height %d\n", v4l2GetFmtName(param.v4l2Fmt),
            param.width, param.height);


    ff_info("\n===========================================================================\n\n");

    // start
    cam->start();
    getchar();
    cam->stop();
    return 0;
}
