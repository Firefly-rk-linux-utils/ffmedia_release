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
#include <vector>

#include "base/ff_type.hpp"
#include "ff_media_consumer.hpp"

namespace FFMedia
{
class FFMEDIA_API MediaProducer
{
public:
    MediaProducer();
    virtual ~MediaProducer();

    MediaProducer(const MediaProducer&) = delete;
    MediaProducer& operator=(const MediaProducer&) = delete;
    MediaProducer(MediaProducer&&) = delete;
    MediaProducer& operator=(MediaProducer&&) = delete;

    void addConsumer(std::shared_ptr<MediaConsumer> consumer);
    void addConsumer(std::shared_ptr<MediaConsumer> consumer,
                     const std::vector<MediaChannelRoute>& routes);
    void removeConsumer(std::shared_ptr<MediaConsumer> consumer);

protected:
    virtual void pushMediaBuffer(const std::shared_ptr<MediaBuffer>& media_buffer,
                                 MediaChannelId producer_channel_id) final;
    void pushMediaBuffer(std::shared_ptr<MediaBuffer>&& media_buffer,
                         MediaChannelId producer_channel_id);

    /**
     * Release dispatch snapshots retired by addConsumer()/removeConsumer().
     *
     * The caller must guarantee that dispatch is quiescent, normally after
     * the producer's worker thread has stopped.  Keeping this operation
     * explicit lets the frame hot path use a raw atomic snapshot pointer
     * without reader-side reference-count operations.
     */
    void reclaimRetiredDispatchSnapshots();
    std::vector<std::shared_ptr<MediaConsumer>> consumerSnapshot();

private:
    void rebuildDispatchSnapshotLocked();
    void dispatchMediaBuffer(MediaBufferContext&& context,
                             MediaChannelId producer_channel_id);

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace FFMedia
