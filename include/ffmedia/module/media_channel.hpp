/*
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/media_buffer.hpp"

namespace FFMedia
{

class ModuleMedia;

using MediaChannelId = uint32_t;

constexpr MediaChannelId MEDIA_CHANNEL_ID_ANY = std::numeric_limits<MediaChannelId>::max();
constexpr MediaChannelId MEDIA_CHANNEL_ID_DEFAULT = 0;

/** A producer's media output channel and its initialization parameters. */
struct MediaChannelInfo {
    MediaChannelId id = MEDIA_CHANNEL_ID_DEFAULT;
    std::string name;
    MEDIA_BUFFER_TYPE media_type = BUFFER_TYPE_ETC;
    media_codec_t codec = MEDIA_CODEC_UNKNOWN;
    ImagePara image_para;
    SampleInfo sample_info;
    std::shared_ptr<MediaBuffer> extra_data;
};

/** Formats accepted by one logical input of a consumer module. */
struct MediaChannelRequirement {
    MediaChannelId input_id = MEDIA_CHANNEL_ID_DEFAULT;
    std::string name;
    MEDIA_BUFFER_TYPE media_type = BUFFER_TYPE_ETC;
    std::vector<media_codec_t> codecs;
    std::vector<uint32_t> pixel_formats;
    std::vector<SampleFormat> sample_formats;
    bool allow_multiple = false;

    bool matches(const MediaChannelInfo& channel) const
    {
        if (media_type != BUFFER_TYPE_ETC && media_type != channel.media_type)
            return false;

        if (!codecs.empty()
            && std::find(codecs.begin(), codecs.end(), channel.codec) == codecs.end())
            return false;

        if (!pixel_formats.empty()
            && std::find(pixel_formats.begin(), pixel_formats.end(),
                         channel.image_para.v4l2Fmt)
                   == pixel_formats.end())
            return false;

        if (!sample_formats.empty()
            && std::find(sample_formats.begin(), sample_formats.end(),
                         channel.sample_info.fmt)
                   == sample_formats.end())
            return false;

        return true;
    }
};

/** Producer output channels selected for one connection. Empty means all. */
struct MediaChannelSelection {
    std::vector<MediaChannelId> output_ids;

    MediaChannelSelection() = default;
    MediaChannelSelection(std::initializer_list<MediaChannelId> ids)
        : output_ids(ids)
    {
    }

    bool accepts(MediaChannelId id) const
    {
        return output_ids.empty()
               || std::find(output_ids.begin(), output_ids.end(), id) != output_ids.end();
    }
};

/** A channel selected and configured on a consumer by connectProducer(). */
struct MediaInputChannel {
    MediaChannelId input_id = MEDIA_CHANNEL_ID_DEFAULT;
    MediaChannelId producer_channel_id = MEDIA_CHANNEL_ID_DEFAULT;
    std::weak_ptr<ModuleMedia> producer;
    std::string producer_name;
    MediaChannelInfo media;
};

/** Per-connection routing entry used while distributing a buffer. */
struct MediaChannelRoute {
    MediaChannelId producer_channel_id = MEDIA_CHANNEL_ID_ANY;
    MediaChannelId consumer_input_id = MEDIA_CHANNEL_ID_ANY;

    MediaChannelRoute() = default;
    MediaChannelRoute(MediaChannelId producer_id, MediaChannelId input_id)
        : producer_channel_id(producer_id), consumer_input_id(input_id)
    {
    }
};

/**
 * Consumer-side envelope. Unlike MediaBuffer metadata, this information is
 * connection-specific and is therefore safe when one buffer is fanned out.
 */
struct MediaBufferContext {
    std::shared_ptr<MediaBuffer> buffer;
    MediaChannelId input_id = MEDIA_CHANNEL_ID_DEFAULT;
};

}  // namespace FFMedia
