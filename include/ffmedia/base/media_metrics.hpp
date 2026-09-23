/*
 * Runtime media pipeline metrics.
 *
 * This is an additive snapshot type. It is intentionally independent from
 * MediaBuffer and ModuleMedia storage so collecting metrics does not change
 * the existing public object layouts or virtual tables.
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once

#include <cstdint>

namespace FFMedia
{

struct FFMEDIA_API MediaMetrics {
    static constexpr uint32_t VERSION = 1;

    // Callers may use these fields for size/version negotiation when the
    // snapshot crosses a shared-library or language boundary.
    uint32_t size;
    uint32_t version;
    bool enabled;

    // Frame flow counters. Input frames are non-empty buffers taken by the
    // module worker; queue-full rejects are counted only as input drops.
    uint64_t input_frames;
    uint64_t output_frames;

    // Current and high-water input queue depth.
    uint64_t queue_depth;
    uint64_t queue_peak_depth;

    // Combined doConsume() and doProduce() callback processing latency.
    uint64_t processing_samples;
    uint64_t processing_time_new_usec;
    double processing_time_avg_usec;
    uint64_t processing_time_max_usec;

    uint64_t buffer_pool_exhausted;

    // Drop counters partition dropped frames by the point of failure.
    uint64_t input_dropped_frames;
    uint64_t processing_dropped_frames;
    uint64_t output_dropped_frames;
    uint64_t error_count;

    MediaMetrics() noexcept
        : size(sizeof(MediaMetrics)), version(VERSION), enabled(true),
          input_frames(0), output_frames(0), queue_depth(0),
          queue_peak_depth(0), processing_samples(0),
          processing_time_new_usec(0), processing_time_avg_usec(0.0),
          processing_time_max_usec(0), buffer_pool_exhausted(0),
          input_dropped_frames(0), processing_dropped_frames(0),
          output_dropped_frames(0), error_count(0)
    {
    }
};

}  // namespace FFMedia
