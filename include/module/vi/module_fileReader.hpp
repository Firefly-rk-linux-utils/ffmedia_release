/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-04-25 12:52:36
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2025-05-29 16:20:10
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
    int videoFirstBuffer;
    int audioFirstBuffer;

    bool loopMode;

    media_codec_t videoCodec;
    media_codec_t audioCodec;
    SampleInfo audioSampleInfo;

protected:
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer>& output_buffer) override;
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
     * @description: 获取指定类型的媒体附加数据。此调用应在对象初始化之后调用。
     * @param {MEDIA_BUFFER_TYPE} meida_type    媒体类型。
     * @return {shared_ptr<MediaBuffer>}        成功返回含有附加数据及媒体参数的MediaBuffer，失败返回空指针。
     */
    shared_ptr<MediaBuffer> getExtraBuffer(MEDIA_BUFFER_TYPE media_type);

    /**
     * @description: 获取视频格式。此调用应在对象初始化后使用。
     * @return {*}
     */
    media_codec_t getVideoCodec();

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
