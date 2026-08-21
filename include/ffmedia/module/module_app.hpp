/*
 * Copyright (c) 2026-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "module/module_media.hpp"

namespace FFMedia
{

enum class AppQueuePolicy : uint8_t {
    BLOCK = 0,
    DROP_NEWEST,
    DROP_OLDEST,
};

enum class AppMemoryMode : uint8_t {
    BORROW = 0,
    HOLD_OWNER,
    COPY,
};

/**
 * A frame supplied by application code.
 *
 * Use either buffer, or data/size/dmabuf_fd. Channel media parameters are
 * taken from the ModuleAppSource/ModuleAppProcessor output channel.
 */
struct AppFrame {
    MediaChannelId channel_id = MEDIA_CHANNEL_ID_ANY;

    std::shared_ptr<MediaBuffer> buffer;

    void* data = nullptr;
    size_t size = 0;
    int dmabuf_fd = -1;

    int64_t pts_us = 0;
    int64_t dts_us = 0;
    int flags = 0;
    bool eos = false;

    AppMemoryMode memory_mode = AppMemoryMode::HOLD_OWNER;
    std::shared_ptr<void> owner;
    std::shared_ptr<MediaBuffer> extra_data;
};

struct AppSourceOptions {
    size_t queue_capacity = 4;
    AppQueuePolicy queue_policy = AppQueuePolicy::BLOCK;
};

/** Application-fed source for raw, encoded, audio, and auxiliary data. */
class FFMEDIA_API ModuleAppSource final : public ModuleMedia
{
public:
    explicit ModuleAppSource(
        const std::vector<MediaChannelInfo>& channels,
        const AppSourceOptions& options = AppSourceOptions());
    ~ModuleAppSource() override;

    int init() override;

    int submit(const AppFrame& frame, int timeout_ms = 0,
               uint64_t* ticket = nullptr);
    int submit(const std::shared_ptr<MediaBuffer>& buffer,
               MediaChannelId channel_id = MEDIA_CHANNEL_ID_ANY,
               int timeout_ms = 0, uint64_t* ticket = nullptr);

    /** Completed results are retained for the latest 1024 unwaited tickets. */
    int wait(uint64_t ticket, int timeout_ms);
    int sendEos(MediaChannelId channel_id, int timeout_ms = 0,
                uint64_t* ticket = nullptr);
    int flush(bool discard_pending, int timeout_ms = -1);

protected:
    ProduceResult doProduce(std::shared_ptr<MediaBuffer>& output_buffer) override;
    void bufferReleaseCallBack(
        const std::shared_ptr<MediaBuffer>& buffer) override;
    bool setup() override;
    bool teardown() override;

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

enum class AppProcessAction : uint8_t {
    FORWARD = 0,
    REPLACE,
    DROP,
    ERROR,
};

struct FFMEDIA_API AppProcessResult {
    AppProcessAction action = AppProcessAction::DROP;
    AppFrame output;
    int error_code = 0;

    static AppProcessResult forward(
        MediaChannelId output_channel_id = MEDIA_CHANNEL_ID_ANY);
    static AppProcessResult drop();
    static AppProcessResult replace(const AppFrame& frame);
    static AppProcessResult replace(
        const std::shared_ptr<MediaBuffer>& buffer,
        MediaChannelId output_channel_id = MEDIA_CHANNEL_ID_ANY,
        std::shared_ptr<void> owner = std::shared_ptr<void>());
    static AppProcessResult error(int error_code);
};

using AppProcessCallback = std::function<AppProcessResult(const MediaBufferContext& input)>;

struct AppProcessorOptions {
    uint16_t output_buffer_count = 4;
};

/** Callback-backed processing node for application-defined media handling. */
class FFMEDIA_API ModuleAppProcessor final : public ModuleMedia
{
public:
    ModuleAppProcessor(
        const std::vector<MediaChannelRequirement>& inputs,
        const std::vector<MediaChannelInfo>& outputs,
        AppProcessCallback callback,
        const AppProcessorOptions& options = AppProcessorOptions());
    ~ModuleAppProcessor() override;

    int init() override;

protected:
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;
    void bufferReleaseCallBack(
        const std::shared_ptr<MediaBuffer>& buffer) override;
    bool teardown() override;

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
};

}  // namespace FFMedia
