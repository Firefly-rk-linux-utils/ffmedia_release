/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-11-01 09:07:55
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-01 11:40:52
 * @Description: 输入源组件, 支持文件、网络及UVC等流的读取。通过FFmpeg接口操作获取数据。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once
#include "module/module_media.hpp"
#if FFMPEG_SUPPORT

struct AVFormatContext;
struct AVInputFormat;
struct AVDictionary;
struct AVBSFContext;
struct AVCodecParameters;
namespace FFMedia
{
class ModuleFFmpegDemux : public ModuleMedia
{
public:
    /**
     * @description: ModuleFFmpegDemux 的构造函数。
     * @param {const string &} filename     文件、网络流及UVC设备路径。
     * @param {int} loop            循环读取次数。
     * @return {*}
     */
    ModuleFFmpegDemux(const std::string& filename, int loop = 1);
    ~ModuleFFmpegDemux();

    /**
     * @description: 改变读取对象。此调用应在对象停止时使用。
     * @param {const string &} filename     文件、网络流及UVC设备路径。
     * @param {int} loop            循环读取次数。
     * @return {int}            成功返回 0，失败返回负数。
     */
    int changeSource(const std::string& filename, int loop = 1);

    /**
     * @description: 根据format使用AVinputFormat。
     * @param {const string &} format   AVinputFormat对应的短名称，为空则使用默认的AVinputFormat。
     * @return {int}            成功返回0，失败返回负数错误代码。
     */
    int setInputFormat(const std::string& format);

    /**
     * @description: 设置参数选项的键值对。
     * @param {const string &} key      键, 为空时清空所有键值对。
     * @param {const string &} value    值。
     * @param {int} flags               标志位。
     * @return {int}                    >= o 为成功，< 0 为错误代码。
     */
    int setFormatOption(const std::string& key, const std::string& value, int flags);
    /**
     * @description: 通过键获取参数选项值。
     * @param {const string &} key  键。
     * @param {int} flags   标志位。
     * @return {string}     返回键对应的值。
     */
    std::string getFormatOption(const std::string& key, int flags);

    /**
     * @description: 初始化 ModuleFFmpegDemux。
     * @return {int}    成功返回 0，失败返回负数。
     */
    int init() override;

    /**
     * @description: 设置读取点。此调用应在对象初始化后使用。
     * @param {int64_t} ts      目标时间戳。
     * @param {int} flags       标志位。1：向后寻找；2：基于字节位置寻找；4：查找任何帧；8：基于帧数寻找。
     * @return {*}
     */
    int setFileSeek(int64_t ts, int flags);

    /**
     * @description: 获取音频格式。此调用应在对象初始化后使用。
     * @return {*}
     */
    media_codec_t getAudioCodec();

    /**
     * @description: 获取音频样品信息。此调用应在对象初始化后使用。
     * @return {*}
     */
    SampleInfo getAudioSampleInfo();

    /**
     * @description: 获取指定类型的媒体附加数据。此调用应在对象初始化之后调用。
     * @param {MEDIA_BUFFER_TYPE} meida_type    媒体类型。
     * @return {shared_ptr<MediaBuffer>}        成功返回含有附加数据及媒体参数的MediaBuffer，失败返回空指针。
     */
    std::shared_ptr<MediaBuffer> getExtraBuffer(MEDIA_BUFFER_TYPE media_type);

    /**
     * @description: 获取视频格式。此调用应在对象初始化后使用。
     * @return {*}
     */
    media_codec_t getVideoCodec();

    /**
     * @description: 设置读取数据包的超时时间。
     * @param {int64} usec   微秒。
     * @return {*}
     */
    void setTimeOut(int64_t usec);

protected:
    int initMediaInfo(const AVCodecParameters* par);
    void cleanup();
    virtual bool teardown() override;

    virtual ProduceResult doProduce(std::shared_ptr<MediaBuffer>& output_buffer) override;
    virtual void bufferReleaseCallBack(const std::shared_ptr<MediaBuffer>& buffer) override;

private:
    AVFormatContext* pFCtx;
    const AVInputFormat* pInFmt;
    AVDictionary* pFOpts;
    AVBSFContext* vBSFCtx;

    int videoStream;
    int audioStream;
    media_codec_t videoCodec;
    media_codec_t audioCodec;
    SampleInfo audioSampleInfo;

    int videoFirstBuffer;
    int audioFirstBuffer;

    int loop;
    int64_t timeOutUsec;

    std::string src;
};

}  // namespace FFMedia
#endif  // FFMPEG_SUPPORT