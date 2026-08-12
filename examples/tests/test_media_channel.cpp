#include <cassert>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cerrno>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "module/module_media.hpp"

using namespace FFMedia;

namespace
{

class TestProducer : public ModuleMedia
{
public:
    explicit TestProducer(const std::string& name = "test-producer")
        : ModuleMedia(name)
    {
        setModuleType(ModuleType::SRC);
    }

    void emit(MediaChannelId channel_id)
    {
        auto buffer = std::make_shared<MediaBuffer>();
        buffer->setMediaChannelId(channel_id);
        pushMediaBuffer(buffer, channel_id);
    }

    void reclaimSnapshots()
    {
        reclaimRetiredDispatchSnapshots();
    }
};

class TestConsumer : public ModuleMedia
{
public:
    explicit TestConsumer(const std::string& name)
        : ModuleMedia(name)
    {
    }

    bool waitForContexts(size_t count)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(
            lock, std::chrono::seconds(2),
            [this, count] { return contexts_.size() >= count; });
    }

    std::vector<MediaBufferContext> contexts() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return contexts_;
    }

protected:
    ConsumeResult doConsume(const MediaBufferContext& context,
                            std::shared_ptr<MediaBuffer>&) override
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            contexts_.push_back(context);
        }
        condition_.notify_one();
        return CONSUME_SKIP;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<MediaBufferContext> contexts_;
};

class CountingConsumer : public MediaConsumer
{
public:
    explicit CountingConsumer(const std::string&)
    {
    }

    void receiveMediaBuffer(MediaBufferContext&&) override
    {
        received_.fetch_add(1, std::memory_order_relaxed);
    }

    uint64_t received() const
    {
        return received_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<uint64_t> received_{0};
};

MediaChannelInfo makeVideoChannel(MediaChannelId id, media_codec_t codec,
                                  uint32_t pixel_format)
{
    MediaChannelInfo channel;
    channel.id = id;
    channel.name = "video";
    channel.media_type = BUFFER_TYPE_VIDEO;
    channel.codec = codec;
    channel.image_para = ImagePara(1920, 1080, 1920, 1080, pixel_format);
    return channel;
}

MediaChannelInfo makeAudioChannel(MediaChannelId id)
{
    MediaChannelInfo channel;
    channel.id = id;
    channel.name = "audio";
    channel.media_type = BUFFER_TYPE_AUDIO;
    channel.codec = MEDIA_CODEC_AUDIO_AAC;
    channel.sample_info.fmt = SAMPLE_FMT_S16;
    channel.sample_info.channels = 2;
    channel.sample_info.sample_rate = 48000;
    return channel;
}

}  // namespace

int main()
{
    auto producer = std::make_shared<TestProducer>();
    producer->setOutputMediaChannels({
        makeVideoChannel(4, MEDIA_CODEC_VIDEO_H264, V4L2_PIX_FMT_H264),
        makeVideoChannel(5, MEDIA_CODEC_VIDEO_RAW, V4L2_PIX_FMT_NV12),
        makeAudioChannel(8),
    });

    auto all_consumer = std::make_shared<TestConsumer>("all-consumer");
    assert(all_consumer->connectProducer(producer) == 0);
    assert(all_consumer->getInputMediaChannels().size() == 3);

    auto audio_consumer = std::make_shared<TestConsumer>("audio-consumer");
    assert(audio_consumer->connectProducer(producer, MediaChannelSelection({8})) == 0);
    assert(audio_consumer->getInputMediaChannels().size() == 1);
    assert(audio_consumer->getInputMediaChannels().front().media.sample_info.sample_rate
           == 48000);

    MediaChannelRequirement decoder_input;
    decoder_input.input_id = 0;
    decoder_input.name = "compressed-video";
    decoder_input.media_type = BUFFER_TYPE_VIDEO;
    decoder_input.codecs = {
        MEDIA_CODEC_VIDEO_VP8,
        MEDIA_CODEC_VIDEO_VP9,
        MEDIA_CODEC_VIDEO_H264,
        MEDIA_CODEC_VIDEO_H265,
    };

    auto decoder = std::make_shared<TestConsumer>("decoder");
    decoder->setInputMediaChannelRequirements({decoder_input});
    assert(decoder->connectProducer(producer) == 0);
    assert(decoder->getInputMediaChannels().size() == 1);
    assert(decoder->getInputMediaChannels().front().producer_channel_id == 4);
    assert(decoder->getInputImagePara().v4l2Fmt == V4L2_PIX_FMT_H264);

    auto second_producer = std::make_shared<TestProducer>("second-producer");
    second_producer->setOutputMediaChannels({
        makeVideoChannel(6, MEDIA_CODEC_VIDEO_H265, V4L2_PIX_FMT_HEVC),
    });

    assert(decoder->connectProducer(producer) == 0);
    assert(decoder->getInputMediaChannels().size() == 1);
    assert(decoder->connectProducer(second_producer) == -EBUSY);
    assert(decoder->getInputMediaChannels().size() == 1);
    assert(decoder->getInputMediaChannels().front().producer.lock() == producer);

    MediaChannelRequirement multi_decoder_input = decoder_input;
    multi_decoder_input.allow_multiple = true;
    auto multi_decoder = std::make_shared<TestConsumer>("multi-decoder");
    multi_decoder->setInputMediaChannelRequirements({multi_decoder_input});
    assert(multi_decoder->connectProducer(producer) == 0);
    assert(multi_decoder->connectProducer(second_producer) == 0);
    assert(multi_decoder->getInputMediaChannels().size() == 2);

    auto manual_consumer = std::make_shared<TestConsumer>("manual-consumer");
    ImagePara manual_para(640, 360, 640, 360, V4L2_PIX_FMT_NV12);
    manual_consumer->setInputImagePara(manual_para);
    assert(manual_consumer->connectProducer(producer) == 0);
    assert(manual_consumer->getInputImagePara().width == manual_para.width);
    assert(manual_consumer->getInputImagePara().height == manual_para.height);
    assert(manual_consumer->getInputImagePara().v4l2Fmt == manual_para.v4l2Fmt);

    auto mismatch = std::make_shared<TestConsumer>("mismatch");
    MediaChannelRequirement opus_input;
    opus_input.media_type = BUFFER_TYPE_AUDIO;
    opus_input.codecs = {MEDIA_CODEC_AUDIO_OPUS};
    mismatch->setInputMediaChannelRequirements({opus_input});
    assert(mismatch->connectProducer(producer) == -ENOTSUP);
    assert(mismatch->getInputMediaChannels().empty());

    assert(audio_consumer->connectProducer(producer, MediaChannelSelection({99}))
           == -ENOENT);

    audio_consumer->start();
    decoder->start();
    multi_decoder->start();

    producer->emit(4);
    producer->emit(8);
    second_producer->emit(6);

    assert(audio_consumer->waitForContexts(1));
    assert(decoder->waitForContexts(1));
    assert(multi_decoder->waitForContexts(2));

    const auto audio_contexts = audio_consumer->contexts();
    const auto decoder_contexts = decoder->contexts();
    const auto multi_decoder_contexts = multi_decoder->contexts();
    assert(audio_contexts.size() == 1);
    const auto& context = audio_contexts.front();
    assert(context.input_id == 8);

    assert(decoder_contexts.size() == 1);
    assert(decoder_contexts.front().input_id == 0);

    assert(multi_decoder_contexts.size() == 2);
    assert(multi_decoder_contexts[0].input_id == 0);
    assert(multi_decoder_contexts[1].input_id == 0);

    audio_consumer->stop();
    decoder->stop();
    multi_decoder->stop();

    auto concurrent_producer = std::make_shared<TestProducer>(
        "concurrent-producer");
    auto stable_consumer = std::make_shared<CountingConsumer>(
        "stable-consumer");
    concurrent_producer->addConsumer(stable_consumer);

    constexpr int kConcurrentFrames = 50000;
    std::atomic<bool> start_dispatch{false};
    std::thread dispatch_thread([&] {
        while (!start_dispatch.load(std::memory_order_acquire))
            std::this_thread::yield();
        for (int i = 0; i < kConcurrentFrames; ++i) {
            concurrent_producer->emit(0);
            if ((i & 0xff) == 0)
                std::this_thread::yield();
        }
    });

    std::weak_ptr<CountingConsumer> retired_consumer;
    start_dispatch.store(true, std::memory_order_release);
    for (int i = 0; i < 256; ++i) {
        auto transient = std::make_shared<CountingConsumer>(
            "transient-consumer");
        concurrent_producer->addConsumer(transient);
        std::this_thread::yield();
        concurrent_producer->removeConsumer(transient);
        if (i == 255)
            retired_consumer = transient;
    }

    dispatch_thread.join();
    assert(stable_consumer->received() == kConcurrentFrames);
    assert(!retired_consumer.expired());
    concurrent_producer->reclaimSnapshots();
    assert(retired_consumer.expired());

    return 0;
}
