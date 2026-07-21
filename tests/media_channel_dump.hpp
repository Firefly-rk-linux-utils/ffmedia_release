#pragma once

#include "base/ff_log.h"
#include "module/module_media.hpp"

namespace FFMedia
{

inline void dumpMediaBufferBrief(const std::string& module_name,
                                 int queue_size,
                                 std::shared_ptr<MediaBuffer> buffer)
{
    if (!buffer)
        return;

    ff_info("%s: queue %d, channel %u, type %d, codec %d, "
            "bytes %zu, pts %ld, dts %ld\n",
            module_name.c_str(), queue_size, buffer->getMediaChannelId(),
            static_cast<int>(buffer->getMediaBufferType()),
            static_cast<int>(buffer->getMediaCodec()),
            buffer->getActiveSize(), buffer->getPUstimestamp(),
            buffer->getDUstimestamp());
}

inline void dumpOutputMediaChannels(const ModuleMedia& module)
{
    const auto channels = module.getOutputMediaChannels();
    ff_info("%s output channels:\n", module.getName().c_str());

    for (const auto& channel : channels) {
        const size_t extra_size = channel.extra_data
                                      ? channel.extra_data->getActiveSize()
                                      : 0;

        if (channel.media_type == BUFFER_TYPE_VIDEO) {
            const auto& image = channel.image_para;
            ff_info("  Channel[%u] %s: video, codec %d, format %s(0x%08x), "
                    "size %ux%u, stride %ux%u, extra %zu bytes\n",
                    channel.id, channel.name.c_str(),
                    static_cast<int>(channel.codec),
                    v4l2GetFmtName(image.v4l2Fmt), image.v4l2Fmt,
                    image.width, image.height, image.hstride, image.vstride,
                    extra_size);
        } else if (channel.media_type == BUFFER_TYPE_AUDIO) {
            const auto& sample = channel.sample_info;
            ff_info("  Channel[%u] %s: audio, codec %d, format %d, "
                    "channels %d, sample rate %d, samples %d, extra %zu bytes\n",
                    channel.id, channel.name.c_str(),
                    static_cast<int>(channel.codec),
                    static_cast<int>(sample.fmt), sample.channels,
                    sample.sample_rate, sample.nb_samples, extra_size);
        } else {
            ff_info("  Channel[%u] %s: type %d, codec %d, extra %zu bytes\n",
                    channel.id, channel.name.c_str(),
                    static_cast<int>(channel.media_type),
                    static_cast<int>(channel.codec), extra_size);
        }
    }
}

}  // namespace FFMedia
