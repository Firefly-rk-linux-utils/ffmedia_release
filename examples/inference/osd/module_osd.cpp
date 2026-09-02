#include "module_osd.hpp"
#include <opencv2/imgproc.hpp>

namespace FFMedia
{

ModuleOsd::ModuleOsd(const std::string& name)
    : ModuleMedia(name.c_str())
{
    setBufferCount(2);

    MediaChannelRequirement input_requirement;
    input_requirement.input_id = 0;
    input_requirement.name = "video/x-raw";
    input_requirement.media_type = BUFFER_TYPE_VIDEO;
    input_requirement.codecs = {MEDIA_CODEC_VIDEO_RAW};
    setInputMediaChannelRequirements({input_requirement});

    _converter = std::make_shared<ModuleRga>();
    _converter->setBufferCount(0);
}

ModuleOsd::~ModuleOsd()
{
    stop();
}

int ModuleOsd::init()
{
    if (isInitialized())
        return 0;

    const ImagePara input = getInputImagePara();
    ImagePara output;
    output.width = input.width;
    output.height = input.height;
    output.hstride = ALIGN(output.width, 16);
    output.vstride = ALIGN(output.height, 16);
    output.v4l2Fmt = V4L2_PIX_FMT_BGR24;
    setOutputImagePara(output);
    auto ret = initBuffer();
    if (ret != 0) {
        ff_error_m("Failed to initialize buffer");
        return ret;
    }

    // init converter
    // 初始化转换器
    _converter->setSrcPara(input.v4l2Fmt, 0, 0, input.width,
                           input.height, input.hstride, input.vstride);
    _converter->setDstPara(output.v4l2Fmt, 0, 0, output.width,
                           output.height, output.hstride, output.vstride);
    setInitialized();
    return 0;
}

int ModuleOsd::initBuffer()
{
    OutputBufferPool pool;
    const uint16_t count = getBufferCount();
    const ImagePara output = getOutputImagePara();
    pool.buffers.reserve(count);
    pool.rotation_buffers.reserve(count);

    for (uint16_t i = 0; i < count; ++i) {
        std::shared_ptr<InferBuffer> buffer = std::make_shared<InferBuffer>(VideoBuffer::DRM_BUFFER_NONCACHEABLE);
        buffer->setIndex(i);
        if (output.width != 0 && output.height != 0) {
            buffer->allocBuffer(output);
            if (buffer->getBufFd() < 0)
                return -ENOMEM;
        }
        buffer->setMediaBufferType(BUFFER_TYPE_VIDEO);
        buffer->setMediaCodec(MEDIA_CODEC_VIDEO_RAW);
        buffer->setMediaChannelId(0);
        pool.buffers.push_back(buffer);
        pool.rotation_buffers.push_back(std::move(buffer));
    }
    return commitOutputBufferPool(std::move(pool));
}

ModuleMedia::ConsumeResult ModuleOsd::doConsume(
    const MediaBufferContext& context,
    std::shared_ptr<MediaBuffer>& output_buffer)
{
    const auto& input_buffer = context.buffer;
    if (input_buffer->getActiveSize() == 0) {
        return ConsumeResult::CONSUME_SKIP;
    }

    auto* in_buf = dynamic_cast<InferBuffer*>(input_buffer.get());
    if (!in_buf) {
        ff_error_m("Invalid input buffer type");
        return ConsumeResult::CONSUME_FAILED;
    }
    std::shared_ptr<InferBuffer> out_buf = std::static_pointer_cast<InferBuffer>(output_buffer);

    // 检查输入图像和输出图像的尺寸是否一致。
    // Check whether the dimensions of the input image and the output image are the same.
    const ImagePara input = in_buf->getImagePara();
    ImagePara output = out_buf->getImagePara();
    if (output.width != input.width || output.height != input.height) {
        output.width = input.width;
        output.height = input.height;
        output.hstride = ALIGN(output.width, 16);
        output.vstride = ALIGN(output.height, 16);
        output.v4l2Fmt = V4L2_PIX_FMT_BGR24;
        setInputImagePara(input);
        setOutputImagePara(output);
        out_buf->allocBuffer(output);
        if (out_buf->getBufFd() < 0) {
            ff_error_m("Failed to allocate output buffer\n");
            return ConsumeResult::CONSUME_FAILED;
        }

        // 重新设置转换器的输出参数。
        // Re-set the output parameters of the converter.
        _converter->setDstPara(output.v4l2Fmt, 0, 0, output.width,
                               output.height, output.hstride,
                               output.vstride);
    }

    // 转换拷贝输入缓冲区数据到输出缓冲区。
    // Convert and copy the input buffer data to the output buffer.
    if (_converter->doConsume(context, output_buffer)) {
        ff_error_m("Failed to convert input buffer\n");
        return ConsumeResult::CONSUME_FAILED;
    }

    out_buf->copyMetadata(*in_buf);
    out_buf->setImagePara(output);

    auto start_time = std::chrono::high_resolution_clock::now();
    osd(out_buf);
    ff_debug_m("Osd time: %lld us\n", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time).count());
    return ConsumeResult::CONSUME_SUCCESS;
}

void ModuleOsd::osd(std::shared_ptr<InferBuffer>& buffer)
{
    auto image_param = buffer->getImagePara();
    cv::Mat img(image_param.height, image_param.width, CV_8UC3, buffer->getActiveData(), image_param.hstride * 3);

    for (auto& it : buffer->targets) {
        auto id = std::to_string(it.track_id);
        auto labels_display = "#" + id + " " + it.label;

        auto tracks_size = it.tracks.size();
        if (tracks_size > 1) {
            for (size_t i = 0; i < tracks_size - 1; i++) {
                auto& track1 = it.tracks[i];
                auto& track2 = it.tracks[i + 1];
                auto p1 = cv::Point(track1.x + track1.width / 2, track1.y + track1.height);
                auto p2 = cv::Point(track2.x + track2.width / 2, track2.y + track2.height);
                cv::line(img, p1, p2, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
            }
        }

        cv::rectangle(img, cv::Rect(it.x, it.y, it.width, it.height), cv::Scalar(255, 255, 0), 2);
        cv::putText(img, labels_display, cv::Point(it.x, it.y - 5), cv::FONT_HERSHEY_PLAIN, 1.6, cv::Scalar(255, 0, 255), 2);
    }
}


}  // namespace FFMedia
