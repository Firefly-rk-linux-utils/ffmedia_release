/*
 * @Author: Kaison Deng dkx@t-chip.com.cn
 * @Date: 2026-06-15 09:31:54
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-01 11:43:51
 * @Description:
 * Copyright (c) 2026-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>

#include "ff_media_consumer.hpp"

namespace FFMedia
{
class MediaProducer
{
public:
    MediaProducer();
    ~MediaProducer();

    void addConsumer(std::shared_ptr<MediaConsumer> consumer);
    void addConsumer(std::shared_ptr<MediaConsumer> consumer,
                     const std::vector<MediaChannelRoute>& routes);
    void removeConsumer(std::shared_ptr<MediaConsumer> consumer);

protected:
    virtual void pushMediaBuffer(const std::shared_ptr<MediaBuffer>& media_buffer,
                                 MediaChannelId producer_channel_id);

protected:
    std::vector<std::shared_ptr<MediaConsumer>> consumers;
    std::unordered_map<MediaConsumer*, std::vector<MediaChannelRoute>> consumer_routes;
    std::mutex productor_mutex;
};

}  // namespace FFMedia
