/*
 * ModuleAppSource public API usage test.
 *
 * This file intentionally uses only FFMedia public headers and links only
 * libff_media. Each test function is also a small example that SDK users can
 * copy into an application which owns the media input data.
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
#include <vector>

using namespace FFMedia;

namespace
{

constexpr MediaChannelId kVideoChannel = 10;
constexpr MediaChannelId kAudioChannel = 20;

MediaChannelInfo makeVideoChannel(MediaChannelId id = kVideoChannel)
{
    MediaChannelInfo channel;
    channel.id = id;
    channel.name = "application-video";
    channel.media_type = BUFFER_TYPE_VIDEO;
    channel.codec = MEDIA_CODEC_VIDEO_RAW;
    channel.image_para = ImagePara(16, 8, 16, 8, V4L2_PIX_FMT_NV12);
    return channel;
}

MediaChannelInfo makeAudioChannel(MediaChannelId id = kAudioChannel)
{
    MediaChannelInfo channel;
    channel.id = id;
    channel.name = "application-audio";
    channel.media_type = BUFFER_TYPE_AUDIO;
    channel.codec = MEDIA_CODEC_AUDIO_PCM_S16;
    channel.sample_info.fmt = SAMPLE_FMT_S16;
    channel.sample_info.channels = 2;
    channel.sample_info.sample_rate = 48000;
    channel.sample_info.nb_samples = 16;
    return channel;
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
    bool eos = false;
    int dmabuf_fd = -1;
    size_t active_size = 0;
    std::vector<uint8_t> payload;
    std::shared_ptr<MediaBuffer> extra_data;
};

class CallbackCollector
{
public:
    void onBuffer(const std::string&, int,
                  const std::shared_ptr<MediaBuffer>& buffer)
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
        snapshot.eos = buffer->getEos();
        snapshot.active_size = buffer->getActiveSize();
        snapshot.extra_data = buffer->getExtraData();

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
        : ModuleMedia("application-sink")
    {
        MediaChannelRequirement input;
        input.input_id = 0;
        input.name = "raw-video";
        input.media_type = BUFFER_TYPE_VIDEO;
        input.codecs = {MEDIA_CODEC_VIDEO_RAW};
        setInputMediaChannelRequirements({input});
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
        assert(input.input_id == 0);
        assert(input.buffer->getMediaChannelId() == kVideoChannel);
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

AppFrame copyFrame(std::vector<uint8_t>& data, MediaChannelId channel_id,
                   int64_t pts_us)
{
    AppFrame frame;
    frame.channel_id = channel_id;
    frame.data = data.data();
    frame.size = data.size();
    frame.pts_us = pts_us;
    frame.dts_us = pts_us - 1;
    frame.flags = static_cast<int>(pts_us);
    frame.memory_mode = AppMemoryMode::COPY;
    return frame;
}

void waitSuccessfully(ModuleAppSource& source, uint64_t ticket)
{
    assert(ticket != 0);
    assert(source.wait(ticket, 2000) == 0);
}

// Quick-start example: one raw video channel and every supported input form.
void testQuickStartAndAllMemoryModes()
{
    auto video = makeVideoChannel();
    auto codec_config = std::make_shared<MediaBuffer>(2);
    static_cast<uint8_t*>(codec_config->getData())[0] = 0x12;
    static_cast<uint8_t*>(codec_config->getData())[1] = 0x10;
    codec_config->setActiveSize(2);
    video.extra_data = codec_config;

    AppSourceOptions options;
    options.queue_capacity = 4;
    options.queue_policy = AppQueuePolicy::BLOCK;

    auto source = std::make_shared<ModuleAppSource>(
        std::vector<MediaChannelInfo>{video}, options);
    assert(source->init() == 0);
    assert(source->getOutputMediaChannels().size() == 1);

    CallbackCollector collector;
    auto external_consumer = source->addExternalConsumer(
        "application-callback",
        [&collector](const std::string& name, int queue_size,
                     std::shared_ptr<MediaBuffer> buffer) {
            collector.onBuffer(name, queue_size, buffer);
        });
    assert(external_consumer);
    source->start();

    // COPY: submit() owns a copy immediately, so application storage can be
    // modified or freed as soon as submit() returns.
    std::vector<uint8_t> copied(16 * 8 * 3 / 2, 0x11);
    uint64_t copy_ticket = 0;
    assert(source->submit(copyFrame(copied, video.id, 100), 1000,
                          &copy_ticket)
           == 0);
    std::memset(copied.data(), 0xff, copied.size());
    waitSuccessfully(*source, copy_ticket);

    // HOLD_OWNER: FFMedia retains owner until every downstream consumer has
    // released the frame.
    auto owned = std::make_shared<std::vector<uint8_t>>(32, 0x22);
    std::weak_ptr<std::vector<uint8_t>> weak_owned = owned;
    AppFrame held;
    held.channel_id = video.id;
    held.data = owned->data();
    held.size = owned->size();
    held.pts_us = 200;
    held.memory_mode = AppMemoryMode::HOLD_OWNER;
    held.owner = owned;
    uint64_t held_ticket = 0;
    assert(source->submit(held, 1000, &held_ticket) == 0);
    held.owner.reset();
    owned.reset();
    assert(!weak_owned.expired());
    waitSuccessfully(*source, held_ticket);
    assert(weak_owned.expired());

    // BORROW: application keeps storage alive and waits for the ticket before
    // reusing it.
    std::vector<uint8_t> borrowed(24, 0x33);
    AppFrame borrowed_frame;
    borrowed_frame.channel_id = video.id;
    borrowed_frame.data = borrowed.data();
    borrowed_frame.size = borrowed.size();
    borrowed_frame.pts_us = 300;
    borrowed_frame.memory_mode = AppMemoryMode::BORROW;
    uint64_t borrowed_ticket = 0;
    assert(source->submit(borrowed_frame, 1000, &borrowed_ticket) == 0);
    waitSuccessfully(*source, borrowed_ticket);
    borrowed[0] = 0x44;

    // Existing MediaBuffer: useful when forwarding data produced by another
    // FFMedia component or by an application-owned buffer pool.
    auto application_buffer = std::make_shared<VideoBuffer>(
        VideoBuffer::MALLOC_BUFFER);
    application_buffer->setImagePara(video.image_para);
    application_buffer->allocBuffer(20);
    std::memset(application_buffer->getData(), 0x55, 20);
    application_buffer->setActiveSize(20);
    application_buffer->setPUstimestamp(400);
    uint64_t buffer_ticket = 0;
    assert(source->submit(application_buffer, video.id, 1000, &buffer_ticket)
           == 0);
    waitSuccessfully(*source, buffer_ticket);

    // DMA-BUF: no mapping is required by ModuleAppSource. The actual fd must
    // remain valid through owner/ticket lifetime in a real application.
    auto dmabuf_owner = std::make_shared<int>(1);
    AppFrame dmabuf;
    dmabuf.channel_id = video.id;
    dmabuf.dmabuf_fd = 123;
    dmabuf.size = 4096;
    dmabuf.pts_us = 500;
    dmabuf.memory_mode = AppMemoryMode::HOLD_OWNER;
    dmabuf.owner = dmabuf_owner;
    uint64_t dmabuf_ticket = 0;
    assert(source->submit(dmabuf, 1000, &dmabuf_ticket) == 0);
    waitSuccessfully(*source, dmabuf_ticket);

    uint64_t eos_ticket = 0;
    assert(source->sendEos(video.id, 1000, &eos_ticket) == 0);
    waitSuccessfully(*source, eos_ticket);
    assert(source->submit(copyFrame(copied, video.id, 600)) == -EPIPE);

    assert(collector.waitForCount(6));
    const auto frames = collector.frames();
    assert(frames.size() == 6);
    assert(frames[0].payload.front() == 0x11);
    assert(frames[0].pts_us == 100);
    assert(frames[0].dts_us == 99);
    assert(frames[0].flags == 100);
    assert(frames[0].image_para == video.image_para);
    assert(frames[0].extra_data == codec_config);
    assert(frames[1].payload.front() == 0x22);
    assert(frames[2].payload.front() == 0x33);
    assert(frames[3].payload.front() == 0x55);
    assert(frames[4].dmabuf_fd == 123);
    assert(frames[4].active_size == 4096);
    assert(frames[5].eos);
    source->stop();
}

// A source can publish video and audio simultaneously. channel_id selects the
// target output channel and the declared metadata is applied automatically.
void testMultipleMediaChannels()
{
    const auto video = makeVideoChannel();
    const auto audio = makeAudioChannel();
    auto source = std::make_shared<ModuleAppSource>(
        std::vector<MediaChannelInfo>{video, audio});
    assert(source->init() == 0);

    CallbackCollector collector;
    auto external_consumer = source->addExternalConsumer(
        "multi-channel-callback",
        [&collector](const std::string& name, int queue_size,
                     std::shared_ptr<MediaBuffer> buffer) {
            collector.onBuffer(name, queue_size, buffer);
        });
    assert(external_consumer);
    source->start();

    std::vector<uint8_t> video_data(32, 0x61);
    AppFrame ambiguous = copyFrame(video_data, MEDIA_CHANNEL_ID_ANY, 10);
    assert(source->submit(ambiguous) == -EINVAL);

    std::vector<uint8_t> audio_data(64, 0x72);
    uint64_t video_ticket = 0;
    uint64_t audio_ticket = 0;
    assert(source->submit(copyFrame(video_data, video.id, 1000), 1000,
                          &video_ticket)
           == 0);
    assert(source->submit(copyFrame(audio_data, audio.id, 2000), 1000,
                          &audio_ticket)
           == 0);
    waitSuccessfully(*source, video_ticket);
    waitSuccessfully(*source, audio_ticket);
    assert(collector.waitForCount(2));

    const auto frames = collector.frames();
    assert(frames[0].channel_id == video.id);
    assert(frames[0].media_type == BUFFER_TYPE_VIDEO);
    assert(frames[0].codec == MEDIA_CODEC_VIDEO_RAW);
    assert(frames[0].image_para == video.image_para);
    assert(frames[1].channel_id == audio.id);
    assert(frames[1].media_type == BUFFER_TYPE_AUDIO);
    assert(frames[1].codec == MEDIA_CODEC_AUDIO_PCM_S16);
    assert(frames[1].sample_info.fmt == SAMPLE_FMT_S16);
    assert(frames[1].sample_info.channels == 2);
    assert(frames[1].sample_info.sample_rate == 48000);
    source->stop();
}

// Queue policies are useful when the application produces data faster than
// downstream modules can consume it.
void testQueuePoliciesAndFlush()
{
    const auto video = makeVideoChannel();
    std::vector<uint8_t> data(32, 0x7a);
    AppFrame frame = copyFrame(data, video.id, 1);

    AppSourceOptions block_options;
    block_options.queue_capacity = 1;
    block_options.queue_policy = AppQueuePolicy::BLOCK;
    ModuleAppSource blocking({video}, block_options);
    assert(blocking.init() == 0);
    uint64_t block_ticket = 0;
    assert(blocking.submit(frame, 0, &block_ticket) == 0);
    assert(blocking.submit(frame, 0) == -EAGAIN);
    assert(blocking.submit(frame, 20) == -ETIMEDOUT);
    assert(blocking.flush(true, 0) == 0);
    assert(blocking.wait(block_ticket, 0) == -ECANCELED);

    AppSourceOptions newest_options = block_options;
    newest_options.queue_policy = AppQueuePolicy::DROP_NEWEST;
    ModuleAppSource drop_newest({video}, newest_options);
    assert(drop_newest.init() == 0);
    uint64_t newest_ticket = 0;
    assert(drop_newest.submit(frame, 0, &newest_ticket) == 0);
    assert(drop_newest.submit(frame, 0) == -EAGAIN);
    assert(drop_newest.flush(true, 0) == 0);
    assert(drop_newest.wait(newest_ticket, 0) == -ECANCELED);

    AppSourceOptions oldest_options = block_options;
    oldest_options.queue_policy = AppQueuePolicy::DROP_OLDEST;
    ModuleAppSource drop_oldest({video}, oldest_options);
    assert(drop_oldest.init() == 0);
    uint64_t old_ticket = 0;
    uint64_t replacement_ticket = 0;
    assert(drop_oldest.submit(frame, 0, &old_ticket) == 0);
    assert(drop_oldest.submit(frame, 0, &replacement_ticket) == 0);
    assert(drop_oldest.wait(old_ticket, 0) == -ECANCELED);
    assert(drop_oldest.flush(true, 0) == 0);
    assert(drop_oldest.wait(replacement_ticket, 0) == -ECANCELED);

    // Discarding pending EOS also reopens that channel for new submissions.
    uint64_t eos_ticket = 0;
    assert(drop_oldest.sendEos(video.id, 0, &eos_ticket) == 0);
    assert(drop_oldest.submit(frame) == -EPIPE);
    assert(drop_oldest.flush(true, 0) == 0);
    assert(drop_oldest.wait(eos_ticket, 0) == -ECANCELED);
    assert(drop_oldest.submit(frame) == 0);
    assert(drop_oldest.flush(true, 0) == 0);
}

// Existing FFMedia modules connect with connectProducer(). A ticket completes
// only after the slowest downstream consumer releases the frame.
void testPipelineConnectionAndInFlightFlush()
{
    const auto video = makeVideoChannel();
    auto source = std::make_shared<ModuleAppSource>(
        std::vector<MediaChannelInfo>{video});
    auto sink = std::make_shared<BlockingSink>();
    assert(source->init() == 0);
    assert(sink->connectProducer(source) == 0);
    source->start();

    std::vector<uint8_t> data(32, 0x35);
    uint64_t ticket = 0;
    assert(source->submit(copyFrame(data, video.id, 1), 1000, &ticket) == 0);
    assert(sink->waitUntilEntered());
    assert(source->flush(false, 0) == -EAGAIN);
    sink->release();
    waitSuccessfully(*source, ticket);
    assert(source->flush(false, 1000) == 0);
    source->stop();
}

void testInputValidation()
{
    const auto video = makeVideoChannel();

    ModuleAppSource empty(std::vector<MediaChannelInfo>{});
    assert(empty.init() == -EINVAL);

    AppSourceOptions invalid_options;
    invalid_options.queue_capacity = 0;
    ModuleAppSource invalid_capacity({video}, invalid_options);
    assert(invalid_capacity.init() == -EINVAL);

    ModuleAppSource source({video});
    assert(source.init() == 0);
    std::vector<uint8_t> data(16, 0x18);

    AppFrame borrowed;
    borrowed.data = data.data();
    borrowed.size = data.size();
    borrowed.memory_mode = AppMemoryMode::BORROW;
    assert(source.submit(borrowed) == -EINVAL);

    AppFrame owner_missing = borrowed;
    owner_missing.memory_mode = AppMemoryMode::HOLD_OWNER;
    assert(source.submit(owner_missing) == -EINVAL);

    AppFrame unknown = copyFrame(data, 999, 1);
    assert(source.submit(unknown) == -ENOENT);
}

}  // namespace

int main()
{
    testQuickStartAndAllMemoryModes();
    testMultipleMediaChannels();
    testQueuePoliciesAndFlush();
    testPipelineConnectionAndInFlightFlush();
    testInputValidation();
    return 0;
}
