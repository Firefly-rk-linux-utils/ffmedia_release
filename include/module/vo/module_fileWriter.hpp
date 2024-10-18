/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-04-25 12:52:36
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-19 16:47:43
 * @Description: 输出组件。文件写入，支持裸流写入及mp4、mkv、flv、ts及ps封装格式写入。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE_FILEWRITER_HPP__
#define __MODULE_FILEWRITER_HPP__

#include "module/module_media.hpp"
class generalFileWrite;

class ModuleFileWriter : public ModuleMedia
{
    friend class ModuleFileWriterExtend;

private:
    string filepath;
    shared_ptr<generalFileWrite> writer;
    mutex extend_mtx;

public:
    /**
     * @description: ModuleFileWriter 的构建函数。
     * @param {string} path 媒体文件路径。
     * @return {*}
     */
    ModuleFileWriter(string path);
    ModuleFileWriter(const ImagePara& para, string path);
    ~ModuleFileWriter();
    /**
     * @description: 改变对象写入媒体文件名称。此调用应在对象停止时使用。
     * @param {string} file_name    媒体文件名称.
     * @return {int}                成功返回 0，失败返回负数。
     */
    int changeFileName(string file_name);

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;

    /**
     * @description: 设置视频参数，提前创建视频封装器，不设置则从流中实时创建封装器。如果多流混合封装则全部都要提前创建封装器。
     * @param {int} width           图像宽度。
     * @param {int} height          图像高度。
     * @param {media_codec_t} type  图像格式类型。
     * @return {*}
     */
    void setVideoParameter(int width, int height, media_codec_t type);

    /**
     * @description: 设置视频额外数据。
     * @param {uint8_t*} extra_data     额外数据内存地址。
     * @param {unsigned} extra_size     额外数据大小。
     * @return {*}
     */
    void setVideoExtraData(const uint8_t* extra_data, unsigned extra_size);

    /**
     * @description: 设置音频参数，提前创建音频封装器，不设置则从流中实时创建封装器。如果多流混合封装则全部都要提前创建封装器。
     * @param {int} channel_count   音频通道数量。
     * @param {int} bit_per_sample  音频样品比特率。
     * @param {int} sample_rate     音频样品速率。
     * @param {media_codec_t} type  音频格式类型。
     * @return {*}
     */
    void setAudioParameter(int channel_count, int bit_per_sample, int sample_rate, media_codec_t type);

    /**
     * @description: 设置音频额外数据。
     * @param {uint8_t*} extra_data 额外数据内存地址。
     * @param {unsigned} extra_size 额外数据大小。
     * @return {*}
     */
    void setAudioExtraData(const uint8_t* extra_data, unsigned extra_size);

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    int restart(string file_name);
    void makeWriter();
};

class ModuleFileWriterExtend : public ModuleMedia
{
    shared_ptr<ModuleFileWriter> writer;

public:
    /**
     * @description: ModuleFileWriter 的扩展模块，可使用ModuleFileWriter来构建它，以实现多流混合封装。
     * @param {shared_ptr<ModuleFileWriter>} module     ModuleFileWriter对象。
     * @param {string} path                             媒体文件路径。
     * @return {*}
     */
    ModuleFileWriterExtend(shared_ptr<ModuleFileWriter> module, string path);
    ~ModuleFileWriterExtend();
    int changeFileName(string file_name);
    int init() override;
    void setVideoParameter(int width, int height, media_codec_t type);
    void setVideoExtraData(const uint8_t* extra_data, unsigned extra_size);
    void setAudioParameter(int channel_count, int bit_per_sample, int sample_rate, media_codec_t type);
    void setAudioExtraData(const uint8_t* extra_data, unsigned extra_size);

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
};

#endif
