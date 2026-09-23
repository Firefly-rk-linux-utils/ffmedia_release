/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2026-09-10 20:40:00
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-09-10 20:40:00
 * @Description: Recording component. Segmenting, retention, event capture and
 *               crash recovery are built on the FFmpeg muxer.
 * Copyright (c) 2026-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "module/base_config.h"
#include "module/module_media.hpp"

#if FFMPEG_SUPPORT

namespace FFMedia
{

class FFMEDIA_API ModuleRecorder : public ModuleMedia
{
public:
    enum class RecorderMode : int32_t {
        /** Record continuously. triggerEvent() marks the current segment. */
        CONTINUOUS = 0,
        /** Cache the pre-event window and write it only after triggerEvent().
         */
        EVENT = 1,
    };

    enum class RecorderStatus : int32_t {
        IDLE = 0,
        ARMED = 1,
        RECORDING = 2,
        FAULTED = 3,
    };

    struct RecorderSegment {
        uint64_t sequence = 0;
        std::string path;
        int64_t start_us = -1;
        int64_t end_us = -1;
        uint64_t size_bytes = 0;
        std::string event_id;
        bool event = false;
        bool complete = false;
    };

    static constexpr int SEGMENT_DURATION_MS = 1 << 0;
    static constexpr int SEGMENT_SIZE_BYTES = 1 << 1;

    /**
     * @description: ModuleRecorder 的构建函数。
     * @param {string} path_pattern    输出文件模式，可用一个 printf 整数占位符。
     * @param {string} format          FFmpeg 输出格式，为空时根据扩展名推断。
     */
    ModuleRecorder(const std::string& path_pattern = "",
                   const std::string& format = "");
    ~ModuleRecorder();

    /**
     * @description: 更改输出文件模式。此调用应在对象停止且未录像时使用。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int changePathPattern(const std::string& path_pattern);

    /**
     * @description: 更改 FFmpeg 输出格式。此调用应在对象停止且未录像时使用。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int changeFormat(const std::string& format);

    /**
     * @description: 设置分片限制，flags 指定 value 表示时长还是大小。
     *              flags 一次只能指定一个目标；0 表示禁止该限制。
     * @param {uint64_t} value    时长（毫秒）或大小（字节），0 表示禁止限制。
     * @param {int} flags         只能为 SEGMENT_DURATION_MS 或 SEGMENT_SIZE_BYTES。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int setSegmentLimit(uint64_t value, int flags);

    /**
     * @description: 设置录像模式。EVENT 模式需要配合事件窗口使用；
     *              应在 start() 前设置，运行中不可修改。
     */
    int setRecorderMode(RecorderMode mode);
    RecorderMode recorderMode() const;

    /**
     * @description: 设置事件前缓冲和事件后录制时长。启动时均为 0 表示普通录像。
     * 录像期间修改会影响后续事件；已触发事件使用触发时捕获的窗口，且不会切换当前模式。
     */
    int setEventWindow(uint64_t pre_duration_ms, uint64_t post_duration_ms);

    /**
     * @description: 设置保留的分片数量，0 表示不自动删除旧分片。
     */
    int setRetainedSegments(uint32_t segment_count);

    /**
     * @description: 设置已关闭分片的存储配额，0 表示不限制。超过配额时优先删除旧分片。
     */
    int setStorageQuota(uint64_t size_bytes);

    /**
     * @description: 设置录像索引文件。默认为 `<path_pattern>.ffrec`。
     */
    int setIndexFile(const std::string& path);

    /**
     * @description: 设置传递给每个分片的 FFmpeg 选项。
     * @param {string} key 键，为空时清空所有选项。
     */
    int setFormatOption(const std::string& key, const std::string& value,
                        int flags);
    std::string getFormatOption(const std::string& key, int flags) const;

    /**
     * @description: 设置视频参数，不设置则从生产者获取。应在录像前调用。
     */
    int setVideoParameter(int width, int height, media_codec_t type,
                          uint32_t v4l2_fmt = 0);

    /**
     * @description: 设置媒体附件数据，应在录像前调用。
     */
    int setExtraBuffer(MEDIA_BUFFER_TYPE media_type,
                       const std::shared_ptr<MediaBuffer>& extra_buffer);

    /**
     * @description: 设置音频参数，不设置则从生产者获取。应在录像前调用。
     */
    int setAudioParameter(int channel_count, int bit_per_sample,
                          int sample_rate, media_codec_t type,
                          SampleFormat sample_fmt = SAMPLE_FMT_NONE);

    int init() override;

    /**
     * @description: 获取最近一次 setup/doConsume/teardown 的负数错误码。
     */
    int lastError() const;

    /**
     * @description: 在下一个视频关键帧处创建新分片；无视频流时立即分片。
     */
    int splitRecording();

    /**
     * @description: 触发事件并关联事件标识。CONTINUOUS 模式标记当前分片，EVENT 模式开始写事件分片。
     */
    int triggerEvent(const std::string& event_id = "");

    /**
     * @description: 获取当前录像状态。
     */
    RecorderStatus getRecorderStatus() const;

    /**
     * @description: 获取索引中的分片记录。
     */
    std::vector<RecorderSegment> getSegments() const;

    /**
     * @description: 恢复崩溃遗留的分片索引。只能在未录像时调用。
     */
    int recover();

protected:
    bool setup() override;
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;
    bool teardown() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace FFMedia
#endif  // FFMPEG_SUPPORT
