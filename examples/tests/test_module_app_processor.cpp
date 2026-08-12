/*
 * ModuleAppProcessor public API usage test.
 *
 * This file intentionally uses only FFMedia public headers and links only
 * libff_media. It demonstrates an application callback inserted into a normal
 * FFMedia pipeline without requiring access to module implementation code.
 */
#include "module/module_app.hpp"

#include <cassert>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

using namespace FFMedia;

namespace
{

constexpr MediaChannelId kSourceVideoChannel = 10;
constexpr MediaChannelId kSourceAudioChannel = 20;
constexpr MediaChannelId kProcessedVideoChannel = 30;

MediaChannelInfo makeVideoChannel(MediaChannelId id)
{
    MediaChannelInfo channel;
    channel.id = id;
    channel.name = "raw-video";
    channel.media_type = BUFFER_TYPE_VIDEO;
    channel.codec = MEDIA_CODEC_VIDEO_RAW;
    channel.image_para = ImagePara(16, 8, 16, 8, V4L2_PIX_FMT_NV12);
    return channel;
}

MediaChannelInfo makeAudioChannel(MediaChannelId id)
{
    MediaChannelInfo channel;
    channel.id = id;
    channel.name = "pcm-audio";
    channel.media_type = BUFFER_TYPE_AUDIO;
    channel.codec = MEDIA_CODEC_AUDIO_PCM_S16;
    channel.sample_info.fmt = SAMPLE_FMT_S16;
    channel.sample_info.channels = 2;
    channel.sample_info.sample_rate = 48000;
    channel.sample_info.nb_samples = 16;
    return channel;
}

MediaChannelRequirement rawVideoInput(MediaChannelId input_id = 0)
{
    MediaChannelRequirement input;
    input.input_id = input_id;
    input.name = "application-video-input";
    input.media_type = BUFFER_TYPE_VIDEO;
    input.codecs = {MEDIA_CODEC_VIDEO_RAW};
    return input;
}

MediaChannelRequirement pcmAudioInput(MediaChannelId input_id)
{
    MediaChannelRequirement input;
    input.input_id = input_id;
    input.name = "application-audio-input";
    input.media_type = BUFFER_TYPE_AUDIO;
    input.codecs = {MEDIA_CODEC_AUDIO_PCM_S16};
    input.sample_formats = {SAMPLE_FMT_S16};
    return input;
}

AppFrame copyInput(std::vector<uint8_t>& data, MediaChannelId channel_id,
                   int64_t pts_us)
{
    AppFrame frame;
    frame.channel_id = channel_id;
    frame.data = data.data();
    frame.size = data.size();
    frame.pts_us = pts_us;
    frame.dts_us = pts_us;
    frame.memory_mode = AppMemoryMode::COPY;
    return frame;
}

struct FrameSnapshot {
    MediaChannelId channel_id = MEDIA_CHANNEL_ID_ANY;
    MEDIA_BUFFER_TYPE media_type = BUFFER_TYPE_ETC;
    media_codec_t codec = MEDIA_CODEC_UNKNOWN;
    ImagePara image_para;
    SampleInfo sample_info;
    int64_t pts_us = 0;
    int64_t dts_us = 0;
    int flags = 0;
    int dmabuf_fd = -1;
    size_t active_size = 0;
    std::vector<uint8_t> payload;
};

class CallbackCollector
{
public:
    void onBuffer(const std::shared_ptr<MediaBuffer>& buffer)
    {
        FrameSnapshot snapshot;
        snapshot.channel_id = buffer->getMediaChannelId();
        snapshot.media_type = buffer->getMediaBufferType();
        snapshot.codec = buffer->getMediaCodec();
        if (snapshot.media_type == BUFFER_TYPE_VIDEO)
            snapshot.image_para = buffer->getImagePara();
        else if (snapshot.media_type == BUFFER_TYPE_AUDIO)
            snapshot.sample_info = buffer->getSamplePara();
        snapshot.pts_us = buffer->getPUstimestamp();
        snapshot.dts_us = buffer->getDUstimestamp();
        snapshot.flags = buffer->getFlags();
        snapshot.active_size = buffer->getActiveSize();

        auto video = std::dynamic_pointer_cast<VideoBuffer>(buffer);
        if (video)
            snapshot.dmabuf_fd = video->getBufFd();

        const auto* data = static_cast<const uint8_t*>(buffer->getActiveData());
        if (data && snapshot.active_size > 0)
            snapshot.payload.assign(data, data + snapshot.active_size);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            frames_.push_back(std::move(snapshot));
        }
        condition_.notify_all();
    }

    bool waitForCount(size_t count, int timeout_ms = 2000)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                   [this, count] {
                                       return frames_.size() >= count;
                                   });
    }

    std::vector<FrameSnapshot> frames() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return frames_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::vector<FrameSnapshot> frames_;
};

class BlockingSink : public ModuleMedia
{
public:
    BlockingSink()
        : ModuleMedia("blocking-application-sink")
    {
        setInputMediaChannelRequirements({rawVideoInput()});
    }

    bool waitUntilEntered(int timeout_ms = 2000)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                   [this] { return entered_; });
    }

    void release()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            released_ = true;
        }
        condition_.notify_all();
    }

protected:
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>&) override
    {
        assert(input.buffer->getMediaChannelId() == kProcessedVideoChannel);
        std::unique_lock<std::mutex> lock(mutex_);
        entered_ = true;
        condition_.notify_all();
        condition_.wait(lock, [this] { return released_; });
        return CONSUME_SKIP;
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool entered_ = false;
    bool released_ = false;
};

class StatusObserver
{
public:
    void onStatus(MediaStatus status)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            status_ = status;
        }
        condition_.notify_all();
    }

    bool waitFor(MediaStatus status, int timeout_ms = 2000)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                   [this, status] { return status_ == status; });
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    MediaStatus status_ = MediaStatus::CREATED;
};

struct LifetimeObserver {
    void markDestroyed()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            destroyed = true;
        }
        condition.notify_all();
    }

    bool waitUntilDestroyed(int timeout_ms = 2000)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return condition.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                  [this] { return destroyed; });
    }

    std::mutex mutex;
    std::condition_variable condition;
    bool destroyed = false;
};

struct OwnedBytes {
    OwnedBytes(std::shared_ptr<LifetimeObserver> observer_, size_t size,
               uint8_t value)
        : observer(std::move(observer_)), bytes(size, value)
    {
    }

    ~OwnedBytes()
    {
        observer->markDestroyed();
    }

    std::shared_ptr<LifetimeObserver> observer;
    std::vector<uint8_t> bytes;
};

void waitSuccessfully(ModuleAppSource& source, uint64_t ticket)
{
    assert(ticket != 0);
    assert(source.wait(ticket, 2000) == 0);
}

// Quick-start pipeline demonstrating every AppProcessAction and replacement
// input form.
void testForwardReplaceDropAndExternalConsumer()
{
    const auto source_channel = makeVideoChannel(kSourceVideoChannel);
    const auto output_channel = makeVideoChannel(kProcessedVideoChannel);
    auto source = std::make_shared<ModuleAppSource>(
        std::vector<MediaChannelInfo>{source_channel});

    auto copied_replacement = std::make_shared<std::vector<uint8_t>>(7, 0x5e);
    std::weak_ptr<std::vector<uint8_t>> held_replacement;
    auto processor = std::make_shared<ModuleAppProcessor>(
        std::vector<MediaChannelRequirement>{rawVideoInput()},
        std::vector<MediaChannelInfo>{output_channel},
        [copied_replacement, &held_replacement](const MediaBufferContext& input) {
            const int64_t pts = input.buffer->getPUstimestamp();
            if (pts == 1)
                return AppProcessResult::forward(kProcessedVideoChannel);
            if (pts == 3)
                return AppProcessResult::drop();

            if (pts == 2) {
                auto replacement = std::make_shared<MediaBuffer>(4);
                std::memset(replacement->getData(), 0x2c, 4);
                replacement->setActiveSize(4);
                replacement->setPUstimestamp(20);
                replacement->setDUstimestamp(19);
                replacement->setFlags(0x22);
                return AppProcessResult::replace(
                    replacement, kProcessedVideoChannel);
            }

            AppFrame replacement;
            replacement.channel_id = kProcessedVideoChannel;
            if (pts == 4) {
                auto owner = std::make_shared<std::vector<uint8_t>>(6, 0x4d);
                held_replacement = owner;
                replacement.data = owner->data();
                replacement.size = owner->size();
                replacement.pts_us = 40;
                replacement.memory_mode = AppMemoryMode::HOLD_OWNER;
                replacement.owner = owner;
            } else if (pts == 5) {
                replacement.data = copied_replacement->data();
                replacement.size = copied_replacement->size();
                replacement.pts_us = 50;
                replacement.memory_mode = AppMemoryMode::COPY;
            } else {
                replacement.dmabuf_fd = 321;
                replacement.size = 4096;
                replacement.pts_us = 60;
                replacement.memory_mode = AppMemoryMode::HOLD_OWNER;
                replacement.owner = std::make_shared<int>(1);
            }
            return AppProcessResult::replace(replacement);
        });

    assert(source->init() == 0);
    assert(processor->connectProducer(source) == 0);
    assert(processor->init() == 0);

    CallbackCollector collector;
    auto external_consumer = processor->addExternalConsumer(
        "processed-frames",
        [&collector](const std::string&, int,
                     std::shared_ptr<MediaBuffer> buffer) {
            collector.onBuffer(buffer);
        });
    assert(external_consumer);
    source->start();

    for (int64_t pts = 1; pts <= 6; ++pts) {
        std::vector<uint8_t> input_data(32, static_cast<uint8_t>(pts));
        uint64_t ticket = 0;
        assert(source->submit(
                   copyInput(input_data, source_channel.id, pts), 1000, &ticket)
               == 0);
        waitSuccessfully(*source, ticket);
    }

    assert(collector.waitForCount(5));
    const auto frames = collector.frames();
    assert(frames.size() == 5);
    assert(frames[0].channel_id == output_channel.id);
    assert(frames[0].pts_us == 1);
    assert(frames[0].payload.front() == 1);
    assert(frames[1].pts_us == 20);
    assert(frames[1].dts_us == 19);
    assert(frames[1].flags == 0x22);
    assert(frames[1].payload.front() == 0x2c);
    assert(frames[2].pts_us == 40);
    assert(frames[2].payload.front() == 0x4d);
    assert(frames[3].pts_us == 50);
    assert(frames[3].payload.front() == 0x5e);
    assert(frames[4].pts_us == 60);
    assert(frames[4].dmabuf_fd == 321);
    assert(frames[4].active_size == 4096);
    assert(processor->getModuleStatus() != MediaStatus::ABNORMAL);
    source->stop();
    assert(held_replacement.expired());
}

// Passing an empty output list makes the processor derive output channels from
// the connected inputs. input_id becomes the derived output channel id.
void testAutomaticOutputsAndMultiChannelRouting()
{
    const auto video = makeVideoChannel(kSourceVideoChannel);
    const auto audio = makeAudioChannel(kSourceAudioChannel);
    auto source = std::make_shared<ModuleAppSource>(
        std::vector<MediaChannelInfo>{video, audio});

    constexpr MediaChannelId kVideoInput = 101;
    constexpr MediaChannelId kAudioInput = 102;
    std::vector<MediaChannelId> callback_inputs;
    std::mutex callback_mutex;
    auto processor = std::make_shared<ModuleAppProcessor>(
        std::vector<MediaChannelRequirement>{
            rawVideoInput(kVideoInput),
            pcmAudioInput(kAudioInput),
        },
        std::vector<MediaChannelInfo>{},
        [&callback_inputs, &callback_mutex](const MediaBufferContext& input) {
            {
                std::lock_guard<std::mutex> lock(callback_mutex);
                callback_inputs.push_back(input.input_id);
            }
            return AppProcessResult::forward(input.input_id);
        });

    assert(source->init() == 0);
    assert(processor->connectProducer(source) == 0);
    assert(processor->init() == 0);
    const auto outputs = processor->getOutputMediaChannels();
    assert(outputs.size() == 2);
    assert(outputs[0].id == kVideoInput);
    assert(outputs[1].id == kAudioInput);

    CallbackCollector collector;
    auto external_consumer = processor->addExternalConsumer(
        "multi-channel-processed-frames",
        [&collector](const std::string&, int,
                     std::shared_ptr<MediaBuffer> buffer) {
            collector.onBuffer(buffer);
        });
    assert(external_consumer);
    source->start();

    std::vector<uint8_t> video_data(32, 0x71);
    std::vector<uint8_t> audio_data(64, 0x82);
    uint64_t video_ticket = 0;
    uint64_t audio_ticket = 0;
    assert(source->submit(copyInput(video_data, video.id, 1), 1000,
                          &video_ticket)
           == 0);
    assert(source->submit(copyInput(audio_data, audio.id, 2), 1000,
                          &audio_ticket)
           == 0);
    waitSuccessfully(*source, video_ticket);
    waitSuccessfully(*source, audio_ticket);
    assert(collector.waitForCount(2));

    const auto frames = collector.frames();
    assert(frames[0].channel_id == kVideoInput);
    assert(frames[0].media_type == BUFFER_TYPE_VIDEO);
    assert(frames[0].image_para == video.image_para);
    assert(frames[1].channel_id == kAudioInput);
    assert(frames[1].media_type == BUFFER_TYPE_AUDIO);
    assert(frames[1].sample_info.fmt == SAMPLE_FMT_S16);
    assert(frames[1].sample_info.channels == 2);
    assert(frames[1].sample_info.sample_rate == 48000);
    {
        std::lock_guard<std::mutex> lock(callback_mutex);
        assert(callback_inputs.size() == 2);
        assert(callback_inputs[0] == kVideoInput);
        assert(callback_inputs[1] == kAudioInput);
    }
    source->stop();
}

// HOLD_OWNER replacement memory is retained until the last downstream module
// releases the processor output.
void testReplacementOwnerLifetime()
{
    const auto source_channel = makeVideoChannel(kSourceVideoChannel);
    const auto output_channel = makeVideoChannel(kProcessedVideoChannel);
    auto source = std::make_shared<ModuleAppSource>(
        std::vector<MediaChannelInfo>{source_channel});
    auto lifetime = std::make_shared<LifetimeObserver>();

    auto processor = std::make_shared<ModuleAppProcessor>(
        std::vector<MediaChannelRequirement>{rawVideoInput()},
        std::vector<MediaChannelInfo>{output_channel},
        [lifetime](const MediaBufferContext&) {
            auto owner = std::make_shared<OwnedBytes>(lifetime, 16, 0x91);
            AppFrame output;
            output.channel_id = kProcessedVideoChannel;
            output.data = owner->bytes.data();
            output.size = owner->bytes.size();
            output.memory_mode = AppMemoryMode::HOLD_OWNER;
            output.owner = owner;
            return AppProcessResult::replace(output);
        });
    auto sink = std::make_shared<BlockingSink>();

    assert(source->init() == 0);
    assert(processor->connectProducer(source) == 0);
    assert(processor->init() == 0);
    assert(sink->connectProducer(processor) == 0);
    source->start();

    std::vector<uint8_t> input_data(32, 0x90);
    uint64_t ticket = 0;
    assert(source->submit(copyInput(input_data, source_channel.id, 1), 1000,
                          &ticket)
           == 0);
    assert(sink->waitUntilEntered());
    {
        std::lock_guard<std::mutex> lock(lifetime->mutex);
        assert(!lifetime->destroyed);
    }
    sink->release();
    assert(lifetime->waitUntilDestroyed());
    waitSuccessfully(*source, ticket);
    source->stop();
}

void testErrorExceptionAndInitializationValidation()
{
    const auto source_channel = makeVideoChannel(kSourceVideoChannel);
    const auto output_channel = makeVideoChannel(kProcessedVideoChannel);

    ModuleAppProcessor no_callback(
        {rawVideoInput()}, {output_channel}, AppProcessCallback());
    assert(no_callback.init() == -EINVAL);

    AppProcessorOptions no_buffers;
    no_buffers.output_buffer_count = 0;
    ModuleAppProcessor invalid_buffers(
        {rawVideoInput()}, {output_channel},
        [](const MediaBufferContext&) { return AppProcessResult::drop(); },
        no_buffers);
    assert(invalid_buffers.init() == -EINVAL);

    ModuleAppProcessor no_connected_input(
        {rawVideoInput()}, {},
        [](const MediaBufferContext&) { return AppProcessResult::drop(); });
    assert(no_connected_input.init() == -ENODEV);

    assert(AppProcessResult::error(5).error_code == -5);
    assert(AppProcessResult::error(-6).error_code == -6);
    assert(AppProcessResult::error(0).error_code == -EIO);

    auto run_failure = [&](AppProcessCallback callback) {
        auto source = std::make_shared<ModuleAppSource>(
            std::vector<MediaChannelInfo>{source_channel});
        auto processor = std::make_shared<ModuleAppProcessor>(
            std::vector<MediaChannelRequirement>{rawVideoInput()},
            std::vector<MediaChannelInfo>{output_channel},
            std::move(callback));
        StatusObserver observer;
        processor->setMediaStatusChangeHooker(
            [&observer](const std::string&, MediaStatus status) {
                observer.onStatus(status);
            });
        assert(source->init() == 0);
        assert(processor->connectProducer(source) == 0);
        assert(processor->init() == 0);
        source->start();

        std::vector<uint8_t> input_data(32, 0xa1);
        uint64_t ticket = 0;
        assert(source->submit(
                   copyInput(input_data, source_channel.id, 1), 1000, &ticket)
               == 0);
        waitSuccessfully(*source, ticket);
        assert(observer.waitFor(MediaStatus::ABNORMAL));
        source->stop();
    };

    run_failure([](const MediaBufferContext&) {
        return AppProcessResult::error(-EPERM);
    });
    run_failure([](const MediaBufferContext&) -> AppProcessResult {
        throw std::runtime_error("application processing failed");
    });
}

}  // namespace

int main()
{
    testForwardReplaceDropAndExternalConsumer();
    testAutomaticOutputsAndMultiChannelRouting();
    testReplacementOwnerLifetime();
    testErrorExceptionAndInitializationValidation();
    return 0;
}
