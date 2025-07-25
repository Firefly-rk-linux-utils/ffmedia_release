/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2025-05-21 15:33:04
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2025-07-08 17:38:01
 * @Description: 输出组件，支持文件，网络等流输出。通过FFmpeg接口输出数据。
 * Copyright (c) 2025-present The ffmedia project authors, All Rights Reserved.
 */

#pragma once
#include "module/module_media.hpp"
#if FFMPEG_SUPPORT

struct AVFormatContext;
struct AVDictionary;
struct AVCodecParameters;
struct AVStream;
struct AVPacket;

class ModuleFFmpegMux : public ModuleMedia
{
public:
    ModuleFFmpegMux(const string& uri, const string& format);
    ~ModuleFFmpegMux();

    /**
     * @description: 更改输出源。此调用应在对象停止时使用。
     * @param {string&} uri     输出源。
     * @param {string&} format  输出源格式。
     * @return {int}            成功返回 0，失败返回负数。
     */
    int changeSource(const string& uri, const string& format);

    /**
     * @description: 设置参数选项的键值对。
     * @param {const string &} key      键, 为空时清空所有键值对。
     * @param {const string &} value    值。
     * @param {int} flags               标志位。
     * @return {int}                    >= o 为成功，< 0 为错误代码。
     */
    int setFormatOption(const string& key, const string& value, int flags);

    /**
     * @description: 通过键获取参数选项值。
     * @param {const string &} key  键。
     * @param {int} flags           标志位。
     * @return {string}             返回键对应的值。
     */
    string getFormatOption(const string& key, int flags);
    /**
     * @description: 设置视频参数，不设置则从生产者获取，应在对象初始化之前调用。
     * @param {int} width           图像宽度。
     * @param {int} height          图像高度。
     * @param {media_codec_t} type  图像格式类型。
     * @return {int}                >= o 为成功，< 0 为错误代码。
     */
    int setVideoParameter(int width, int height, media_codec_t type, uint32_t v4l2_fmt = 0);

    /**
     * @description: 设置媒体附件数据，应在对象初始化之前调用。
     * @param {MEDIA_BUFFER_TYPE} media_type            媒体流类型。
     * @param {shared_ptr<MediaBuffer>} extra_buffer    含有媒体附加数据的MediaBuffer。
     * @return {int}                                    >= o 为成功，< 0 为错误代码。
     */
    int setExtraBuffer(MEDIA_BUFFER_TYPE media_type, shared_ptr<MediaBuffer> extra_buffer);

    /**
     * @description: 设置音频参数，应在对象初始化之前调用。
     * @param {int} channel_count   音频通道数量。
     * @param {int} bit_per_sample  音频样品比特率。
     * @param {int} sample_rate     音频样品速率。
     * @param {media_codec_t} type  音频格式类型。
     * @return {int}                >= o 为成功，< 0 为错误代码。
     */
    int setAudioParameter(int channel_count, int bit_per_sample, int sample_rate, media_codec_t type, SampleFormat sample_fmt = SAMPLE_FMT_NONE);

    /**
     * @description: 初始化 ModuleFFmpegDemux。
     * @return {int}    成功返回 0，失败返回负数。
     */

    int init() override;

    /**
     * @description: 手动输入媒体数据, 此调用应在对象初始化之后使用。
     * @param {shared_ptr<MediaBuffer>} input_buffer    含有媒体数据的MediaBuffer。
     * @return {int}                                    >= o 为成功，< 0 为错误代码。
     */
    int setInputBuffer(shared_ptr<MediaBuffer> input_buffer);

protected:
    struct OutPutStream {
        AVStream* st;
        AVCodecParameters* param;
        AVPacket* tmp_pkt;
    };

    void cleanup();
    int addStream(OutPutStream* stream);

    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer>& input_buffer, shared_ptr<MediaBuffer>& output_buffer) override;
    virtual bool teardown() override;

private:
    string _uri;
    string _format;
    AVFormatContext* _ctx;
    AVDictionary* _opts;

    OutPutStream _video_stream;
    OutPutStream _audio_stream;
    bool _header_written;
};

#endif  // FFMPEG_SUPPORT