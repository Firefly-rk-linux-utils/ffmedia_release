/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-04-25 12:52:36
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-26 15:28:25
 * @Description: 输入源组件，支持裸流读取和mp4、mkv、flv、ts及ps等媒体文件解封装读取。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE_FILEREADER_HPP__
#define __MODULE_FILEREADER_HPP__

#include "module/module_media.hpp"
class generalFileRead;

class ModuleFileReader : public ModuleMedia
{
public:
private:
    string filepath;
    size_t fileSize;
    shared_ptr<generalFileRead> reader;
    bool first_audio_frame;
    bool loopMode;

    media_codec_t videoCodec;
    media_codec_t audioCodec;
    SampleInfo audioSampleInfo;

protected:
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
    void initMediaInfo();

public:
    /**
     * @description: ModuleFileReader 的构造函数。
     * @param {string} path     媒体文件路径。
     * @param {bool} loop_play  循环读取标志。
     * @return {*}
     */
    ModuleFileReader(string path, bool loop_play = false);
    ~ModuleFileReader();

    /**
     * @description: 改变读取的媒体文件。此调用应在对象停止时使用。
     * @param {string} path     媒体文件路径
     * @param {bool} loop_play  循环读取标志.
     * @return {int}            成功返回0，失败返回负数。
     */
    int changeSource(string path, bool loop_play = false);

    /**
     * @description: 初始化 ModuleFileReader 对象。
     * @return {int} 成功返回0， 失败返回负数。
     */
    int init() override;

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
     * @description: 如果是媒体文件，设置从指定时间点获取视频数据；裸流则是设置从指定的文件偏移量读取视频数据。此调用应在对象初始化后使用。
     * @param {int64_t} ms_time     毫秒时间点或文件偏移量。
     * @return {int}                成功返回0。
     */
    int setFileReaderSeek(int64_t ms_time);
    /**
     * @description: 获取媒体文件最大时长或获取裸流文件最大偏移量。 此调用应在对象初始化后使用。
     * @return {*}
     */
    int64_t getFileReaderMaxSeek();
};

#endif
