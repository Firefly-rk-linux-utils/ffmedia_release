/*
 * @Author: Kaison Deng dkx@t-chip.com.cn
 * @Date: 2026-06-17 11:13:03
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-01 11:43:44
 * @Description:
 * Copyright (c) 2026-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "base/ff_type.hpp"
#include "base/media_buffer.hpp"

namespace FFMedia
{

using MediaBufferHooker = std::function<void(
    const std::string&, int, const std::shared_ptr<MediaBuffer>&)>;

enum class MediaStatus : int {
    CREATED = 0,  // 创建状态
    STARTED,      // 运行状态
    EOS,          // 流结束状态
    STOPPED,      // 停止状态
    ABNORMAL,     // 运行异常状态
};
using MediaStatusHooker = std::function<void(const std::string&, MediaStatus)>;

class FFMEDIA_API MediaHookable
{
protected:
    MediaHookable();
    ~MediaHookable();

    // Hook configuration is mutable before the owning module starts and is
    // read-only while it is running. This lets the dispatch path call the
    // configured std::function directly without locking or snapshot copies.
    void freezeHookConfiguration();

    void allowHookConfiguration();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

public:
    MediaHookable(const MediaHookable&) = delete;
    MediaHookable& operator=(const MediaHookable&) = delete;
    MediaHookable(MediaHookable&&) = delete;
    MediaHookable& operator=(MediaHookable&&) = delete;

    /** Configure before start(). Returns false while the module is running. */
    bool setMediaBufferConsumeHooker(MediaBufferHooker hooker);

    /** Configure before start(). Returns false while the module is running. */
    bool setMediaBufferProduceHooker(MediaBufferHooker hooker);

    /** Configure before start(). Returns false while the module is running. */
    bool setMediaStatusChangeHooker(MediaStatusHooker hooker);

    void invokeMediaBufferConsumeHooker(
        const std::string& name, int queue_size,
        const std::shared_ptr<MediaBuffer>& media_buffer);

    void invokeMediaBufferProduceHooker(
        const std::string& name, int queue_size,
        const std::shared_ptr<MediaBuffer>& media_buffer);

    void invokeMediaStatusChangeHooker(
        const std::string& name, MediaStatus media_status);
};

}  // namespace FFMedia
