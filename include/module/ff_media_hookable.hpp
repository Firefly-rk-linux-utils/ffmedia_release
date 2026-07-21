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
#include <mutex>
#include "base/media_buffer.hpp"

namespace FFMedia
{

using MediaBufferHooker = std::function<void(const std::string&, int, std::shared_ptr<MediaBuffer>)>;

enum class MediaStatus : int {
    CREATED = 0,  // 创建状态
    STARTED,      // 运行状态
    EOS,          // 流结束状态
    STOPPED,      // 停止状态
    ABNORMAL,     // 运行异常状态
};
using MediaStatusHooker = std::function<void(const std::string&, MediaStatus)>;

class MediaHookable
{
protected:
    std::mutex media_buffer_hooker_mutex;
    // 媒体输入缓冲处理钩子函数，在处理媒体输入缓冲区前调用
    MediaBufferHooker media_buffer_consume_hooker;
    // 媒体输出缓冲处理完成钩子函数，在处理媒体输出缓冲区完成后调用
    MediaBufferHooker media_buffer_produce_hooker;

    std::mutex media_status_hooker_mutex;
    // 媒体状态变化钩子函数，在媒体状态改变时调用
    MediaStatusHooker media_status_change_hooker;

public:
    MediaHookable(){};
    ~MediaHookable(){};

    void setMediaBufferConsumeHooker(MediaBufferHooker hooker)
    {
        std::lock_guard<std::mutex> lock(media_buffer_hooker_mutex);
        media_buffer_consume_hooker = std::move(hooker);
    }

    void setMediaBufferProduceHooker(MediaBufferHooker hooker)
    {
        std::lock_guard<std::mutex> lock(media_buffer_hooker_mutex);
        media_buffer_produce_hooker = std::move(hooker);
    }

    void setMediaStatusChangeHooker(MediaStatusHooker hooker)
    {
        std::lock_guard<std::mutex> lock(media_status_hooker_mutex);
        media_status_change_hooker = std::move(hooker);
    }

    void invokeMediaBufferConsumeHooker(const std::string& name, int queue_size, const std::shared_ptr<MediaBuffer>& media_buffer)
    {
        std::lock_guard<std::mutex> lock(media_buffer_hooker_mutex);
        if (media_buffer_consume_hooker) {
            media_buffer_consume_hooker(name, queue_size, media_buffer);
        }
    }

    void invokeMediaBufferProduceHooker(const std::string& name, int queue_size, const std::shared_ptr<MediaBuffer>& media_buffer)
    {
        std::lock_guard<std::mutex> lock(media_buffer_hooker_mutex);
        if (media_buffer_produce_hooker) {
            media_buffer_produce_hooker(name, queue_size, media_buffer);
        }
    }

    void invokeMediaStatusChangeHooker(const std::string& name, MediaStatus media_status)
    {
        std::lock_guard<std::mutex> lock(media_status_hooker_mutex);
        if (media_status_change_hooker) {
            media_status_change_hooker(name, media_status);
        }
    }
};

}  // namespace FFMedia
