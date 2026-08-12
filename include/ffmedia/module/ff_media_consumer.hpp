/*
 * @Author: Kaison Deng dkx@t-chip.com.cn
 * @Date: 2026-06-15 09:31:54
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-01 11:43:57
 * @Description:
 * Copyright (c) 2026-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once
#include <memory>
#include "base/ff_type.hpp"
#include "media_channel.hpp"

namespace FFMedia
{

class FFMEDIA_API MediaConsumer
{
public:
    MediaConsumer();
    virtual ~MediaConsumer();

    MediaConsumer(const MediaConsumer&) = delete;
    MediaConsumer& operator=(const MediaConsumer&) = delete;

    virtual void receiveMediaBuffer(MediaBufferContext&& context) = 0;
};

}  // namespace FFMedia
