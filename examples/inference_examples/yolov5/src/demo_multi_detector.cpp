#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "inferencePool.hpp"
#include "yolov5s.hpp"

#include "module/vi/module_ffmpegDemux.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vp/module_rga.hpp"

#include "module/vi/module_memReader.hpp"
#include "module/vo/module_rendererVideo.hpp"
#include "module/vo/module_drmDisplay.hpp"

using namespace std;

class MultiDetectorDemo
{
private:
    /* input link */
    std::shared_ptr<ModuleFFmpegDemux> _src;
    int _detector_num;
    InferencePool<Yolov5s, std::shared_ptr<VideoBuffer>, std::shared_ptr<VideoBuffer>> _detect_pool;
    std::shared_ptr<ModuleMemReader> _dst;
    ImagePara _output_para;

    int64_t _frames;
    std::chrono::_V2::system_clock::time_point _before_time;

private:
    int inputModuleLinkCreate(const std::string media_path);
    int outputModuleLinkCreate();

    static void inputHanderCallback(void* _ctx, shared_ptr<MediaBuffer> buffer);
    void inputHanderCallback1(const shared_ptr<VideoBuffer>& buffer);
    void reset();

public:
    MultiDetectorDemo(int detector_num, const std::string& model_path, const std::string& model_abel_path);
    ~MultiDetectorDemo();

    int init(const std::string media_path);
    void start();
    void stop();
};

MultiDetectorDemo::MultiDetectorDemo(int detector_num, const std::string& model_path, const std::string& model_abel_path)
    : _detector_num(detector_num), _detect_pool(model_path, detector_num, model_abel_path), _frames(0)
{
}

MultiDetectorDemo::~MultiDetectorDemo()
{
    reset();
}

void MultiDetectorDemo::inputHanderCallback(void* ctx, shared_ptr<MediaBuffer> buffer)
{
    MultiDetectorDemo* detector = (MultiDetectorDemo*)ctx;
    shared_ptr<VideoBuffer> buf = static_pointer_cast<VideoBuffer>(buffer);
    detector->inputHanderCallback1(buf);
}
void MultiDetectorDemo::inputHanderCallback1(const shared_ptr<VideoBuffer>& buffer)
{
    int ret;
    ret = _detect_pool.put(buffer);
    if (ret) {
        ff_warn_m("Failed to put input_buffer for detect_pool\n");
        return;
    }

    if (++_frames < _detector_num)
        return;

    shared_ptr<VideoBuffer> output_buffer;
    ret = _detect_pool.get(output_buffer);
    if (ret) {
        ff_warn_m("Failed to get output_buffer for detect_pool\n");
        return;
    }

    ret = _dst->setInputBuffer(output_buffer);
    if (ret) {
        ff_error_m("Failed to set the input bufer\n");
        return;
    }
    ret = _dst->waitProcess(1000);
    if (ret) {
        ff_error_m("Wait timeout\n");
        return;
    }

    if (_frames % 120 == 0) {
        auto current_time = std::chrono::high_resolution_clock::now();
        ff_info_m("Average frame rate within 120 frames: %f\n",
                  120.0 * 1000.0 / std::chrono::duration_cast<std::chrono::milliseconds>(current_time - _before_time).count());
        _before_time = current_time;
    }
}

int MultiDetectorDemo::inputModuleLinkCreate(const std::string media_path)
{
    int ret;
    shared_ptr<ModuleMedia> last_module;
    // Create the media capturer
    auto source = make_shared<ModuleFFmpegDemux>(media_path, -1);
    ret = source->init();
    if (ret < 0) {
        ff_error("source init failed\n");
        return ret;
    }

    last_module = source;
    // Create the video decoder
    auto source_para = source->getOutputImagePara();
    if (v4l2fmtIsCompressed(source_para.v4l2Fmt)) {
        auto dec = make_shared<ModuleMppDec>(source_para);
        ret = dec->connectProducer(source);
        if (ret < 0) {
            ff_error("Failed to connect source to decoder, %d\n", ret);
            return ret;
        }
        ret = dec->init();
        if (ret < 0) {
            ff_error("Dec init failed\n");
            return ret;
        }
        last_module = dec;
    }

    // Create the image converter. Convert the image to BGR24 format.
    auto input_para = last_module->getOutputImagePara();
    auto output_para = input_para;
    output_para.v4l2Fmt = V4L2_PIX_FMT_BGR24;
    auto rga = make_shared<ModuleRga>(output_para, RGA_ROTATE_NONE);
    ret = rga->connectProducer(last_module);
    if (ret < 0) {
        ff_error("Failed to connect video producer to rga, %d\n", ret);
        return ret;
    }
    rga->setBufferCount(_detector_num + 1);
    ret = rga->init();
    if (ret < 0) {
        ff_error("rga init failed\n");
        return ret;
    }

    // Set the output callback function to perform inference processing on the converted image.
    rga->setMediaBufferProduceHooker(std::bind(MultiDetectorDemo::inputHanderCallback, this, std::placeholders::_3));

    _src = source;
    _output_para = rga->getOutputImagePara();
    _frames = 0;
    return ret;
}

int MultiDetectorDemo::outputModuleLinkCreate()
{
    int ret;
    // Create a memory reader to receive images after inference post-processing.
    auto mem_r = make_shared<ModuleMemReader>(_output_para);
    ret = mem_r->init();
    if (ret < 0) {
        ff_error_m("Failed to init memreader\n");
        return ret;
    }

    // Convert the annotated BGR image to NV12 before DRM display.
    auto display_para = _output_para;
    display_para.v4l2Fmt = V4L2_PIX_FMT_NV12;
    ModuleRga::alignStride(display_para.v4l2Fmt,
                           display_para.hstride, display_para.vstride);
    auto display_rga = make_shared<ModuleRga>(display_para, RGA_ROTATE_NONE);
    ret = display_rga->connectProducer(mem_r);
    if (ret < 0) {
        ff_error_m("Failed to connect memory reader to display rga, %d\n", ret);
        return ret;
    }
    ret = display_rga->init();
    if (ret < 0) {
        ff_error_m("Failed to init display rga, %d\n", ret);
        return ret;
    }

    // Create a DRM display for the converted NV12 images.
    auto display = make_shared<ModuleDrmDisplay>(display_rga->getOutputImagePara());
    display->setPlanePara(V4L2_PIX_FMT_NV12);
    ret = display->connectProducer(display_rga);
    if (ret < 0) {
        ff_error_m("Failed to connect display rga to drm display, %d\n", ret);
        return ret;
    }
    ret = display->init();
    if (ret < 0) {
        ff_error_m("Failde to init renderer\n");
        return ret;
    }
    _dst = mem_r;
    return ret;
}

void MultiDetectorDemo::reset()
{
    // Stop the video frame input
    if (_src) {
        _src->stop();
        _src.reset();
    }

    // Clear the detect pool
    while (true) {
        shared_ptr<VideoBuffer> buffer;
        if (_detect_pool.get(buffer) != 0)
            break;

        if (_dst->setInputBuffer(buffer) != 0)
            break;

        if (_dst->waitProcess(1000) != 0)
            break;
    }

    // Stop the video frame output
    if (_dst) {
        _dst->stop();
        _dst.reset();
    }
}

int MultiDetectorDemo::init(const std::string media_path)
{
    int ret;
    reset();
    ret = _detect_pool.init();
    if (ret < 0) {
        ff_error_m("Faild to init detect pool\n");
        return ret;
    }
    // Create the input module link.
    ret = inputModuleLinkCreate(media_path);
    if (ret < 0) {
        ff_error_m("Failed to create input link\n");
        return ret;
    }

    // Create the output module link.
    ret = outputModuleLinkCreate();
    if (ret < 0) {
        ff_error_m("Failed to create ouput link\n");
        return ret;
    }

    return ret;
}

void MultiDetectorDemo::start()
{
    if (_dst && _src) {
        _dst->start();
        _src->start();
    }
}

void MultiDetectorDemo::stop()
{
    reset();
}

int main(int argc, char** argv)
{
    if (argc < 4) {
        ff_error("\nUsage: %s input.mp4 model_path model_label_path\n", argv[0]);
        return -1;
    };

    // The path where the media file is located.
    char* media_path = argv[1];
    // The path where the model is located.
    char* model_path = argv[2];
    // The path where the model label is located.
    char* model_label_path = argv[3];

    int detector_num = 3;
    // Initialize the multi detector demo
    MultiDetectorDemo multi_detect_demo(detector_num, model_path, model_label_path);
    if (multi_detect_demo.init(media_path) != 0) {
        ff_error("Failed to init MultiDetectorDemo\n");
        return -1;
    }

    multi_detect_demo.start();
    getchar();
    multi_detect_demo.stop();
    return 0;
}
