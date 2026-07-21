#include <cassert>
#include <cerrno>
#include <memory>
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
        module_type = ModuleType::SRC;
    }

    void emit(MediaChannelId channel_id)
    {
        auto buffer = std::make_shared<MediaBuffer>();
        buffer->setMediaChannelId(channel_id);
        pushMediaBuffer(buffer, channel_id);
    }
};

class TestConsumer : public ModuleMedia
{
public:
    explicit TestConsumer(const std::string& name)
        : ModuleMedia(name)
    {
    }

    void receiveMediaBuffer(const MediaBufferContext& context) override
    {
        contexts.push_back(context);
    }

    std::vector<MediaBufferContext> contexts;
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

    producer->emit(4);
    producer->emit(8);
    second_producer->emit(6);
    assert(audio_consumer->contexts.size() == 1);
    const auto& context = audio_consumer->contexts.front();
    assert(context.input_id == 8);

    assert(decoder->contexts.size() == 1);
    assert(decoder->contexts.front().input_id == 0);

    assert(multi_decoder->contexts.size() == 2);
    assert(multi_decoder->contexts[0].input_id == 0);
    assert(multi_decoder->contexts[1].input_id == 0);

    return 0;
}
