#include "module_track.hpp"

namespace FFMedia
{

ModuleTrack::ModuleTrack(const std::string& module_name, ModuleTrackFor track_for)
    : ModuleMedia(module_name.c_str()), track_for(track_for)
{
    setBufferCount(2);

    MediaChannelRequirement input_requirement;
    input_requirement.input_id = 0;
    input_requirement.name = "video/x-raw";
    input_requirement.media_type = BUFFER_TYPE_VIDEO;
    input_requirement.codecs = {MEDIA_CODEC_VIDEO_RAW};
    setInputMediaChannelRequirements({input_requirement});
}

ModuleTrack::~ModuleTrack()
{
}

int ModuleTrack::init()
{
    if (isInitialized())
        return 0;

    setOutputImagePara(getInputImagePara());
    auto ret = initBuffer();
    if (ret != 0) {
        ff_error_m("Failed to initialize buffer");
        return ret;
    }

    setInitialized();
    return 0;
}

int ModuleTrack::initBuffer()
{
    OutputBufferPool pool;
    const uint16_t count = getBufferCount();
    pool.buffers.reserve(count);
    pool.rotation_buffers.reserve(count);
    pool.recycle_handler = [](const std::shared_ptr<MediaBuffer>& buffer) {
        auto* infer_buffer = static_cast<InferBuffer*>(buffer.get());
        infer_buffer->setBufFd(-1);
        infer_buffer->setActiveData(nullptr);
        infer_buffer->setActiveSize(0);
        infer_buffer->private_buffer.reset();  // Release the reference to the input buffer
    };
    // Allocate buffers for the module memory pool,
    // but do not allocate actual memory. This is done by external input.
    // 为模块内存池分配缓冲区，但不分配实际内存，由外部输入
    for (uint16_t i = 0; i < count; ++i) {
        std::shared_ptr<InferBuffer> buffer = std::make_shared<InferBuffer>(VideoBuffer::EXTERNAL_BUFFER);
        buffer->setIndex(i);
        buffer->setMediaCodec(MEDIA_CODEC_VIDEO_RAW);
        buffer->setMediaBufferType(BUFFER_TYPE_VIDEO);
        pool.buffers.push_back(buffer);
        pool.rotation_buffers.push_back(std::move(buffer));
    }
    return commitOutputBufferPool(std::move(pool));
}

ModuleMedia::ConsumeResult ModuleTrack::doConsume(
    const MediaBufferContext& input,
    std::shared_ptr<MediaBuffer>& output_buffer)
{
    const auto& input_buffer = input.buffer;
    if (input_buffer->getActiveSize() == 0) {
        return ConsumeResult::CONSUME_SKIP;
    }

    auto* in_buf = dynamic_cast<InferBuffer*>(input_buffer.get());
    if (!in_buf) {
        ff_error_m("Invalid input buffer type");
        return ConsumeResult::CONSUME_FAILED;
    }

    std::shared_ptr<InferBuffer> out_buf = std::static_pointer_cast<InferBuffer>(output_buffer);
    // Use input memory directly, without copying
    // 直接使用输入内存，不需要拷贝
    out_buf->setBufFd(in_buf->getBufFd());
    out_buf->setActiveData(in_buf->getActiveData());
    out_buf->setActiveSize(in_buf->getActiveSize());
    out_buf->copyMetadata(*in_buf);

    auto start_time = std::chrono::high_resolution_clock::now();
    track(out_buf);
    ff_debug_m("Track time: %lld us\n", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time).count());

    // Hold input buffer until output buffer is recycled
    // 保留输入缓冲区，直到输出缓冲区被回收
    out_buf->private_buffer = input_buffer;
    return ConsumeResult::CONSUME_SUCCESS;
}


}  // namespace FFMedia
