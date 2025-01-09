/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-11-01 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-12-31 10:35:38
 * @Description: 输入源组件, 支持文件、网络及UVC等流的读取。通过FFmpeg接口操作获取数据。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once

#include "module/module_media.hpp"

struct AVFormatContext;
struct AVInputFormat;
struct AVDictionary;
struct AVBSFContext;
struct AVCodecParameters;
class ModuleFFmpegDemux : public ModuleMedia
{
public:
    /**
     * @description: ModuleFFmpegDemux 的构造函数。
     * @param {const string &} filename     文件、网络流及UVC设备路径。
     * @param {int} loop            循环读取次数。
     * @return {*}
     */
    ModuleFFmpegDemux(const string& filename, int loop = 1);
    ~ModuleFFmpegDemux();

    /**
     * @description: 改变读取对象。此调用应在对象停止时使用。
     * @param {const string &} filename     文件、网络流及UVC设备路径。
     * @param {int} loop            循环读取次数。
     * @return {int}            成功返回 0，失败返回负数。
     */
    int changeSource(const string& filename, int loop = 1);

    /**
     * @description: 根据format使用AVinputFormat。
     * @param {const string &} format   AVinputFormat对应的短名称，为空则使用默认的AVinputFormat。
     * @return {int}            成功返回0，失败返回负数错误代码。
     */
    int setInputFormat(const string& format);

    /**
     * @description: 设置参数选项的键值对。
     * @param {string} key      键。
     * @param {const string &} value    值。
     * @param {int} flags       标志位。
     * @return {int}            >= o 为成功，< 0 为错误代码。
     */
    int setFormatOption(const string& key, const string& value, int flags);
    /**
     * @description: 通过键获取参数选项值。
     * @param {const string &} key  键。
     * @param {int} flags   标志位。
     * @return {string}     返回键对应的值。
     */
    string getFormatOption(const string& key, int flags);

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
     * @description: 获取音频附加数据。此调用应在对象初始化后使用。
     * @return {const uint8_t*}
     */
    const uint8_t* audioExtraData();
    /**
     * @description: 获取音频附加数据大小。此调用应在对象初始化后使用。
     * @return {*}
     */
    unsigned audioExtraDataSize();

    /**
     * @description: 获取视频格式。此调用应在对象初始化后使用。
     * @return {*}
     */
    media_codec_t getVideoCodec();
    /**
     * @description: 获取视频附加数据。此调用应在对象初始化后使用。
     * @return {*}
     */
    const uint8_t* videoExtraData();
    /**
     * @description: 获取视频附加数据大小。 此调用应在对象初始化后使用。
     * @return {*}
     */
    unsigned videoExtraDataSize();

    /**
     * @description: 设置读取数据包的超时时间。
     * @param {int64} usec   微秒。
     * @return {*}
     */
    void setTimeOut(int64_t usec);

protected:
    int initMediaInfo(const AVCodecParameters* par);
    void cleanup();

    virtual ProduceResult doProduce(shared_ptr<MediaBuffer>& output_buffer) override;
    virtual void bufferReleaseCallBack(shared_ptr<MediaBuffer>& buffer) override;

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

    string src;
};
