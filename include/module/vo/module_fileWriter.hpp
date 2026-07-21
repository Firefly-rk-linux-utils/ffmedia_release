/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-04-25 12:52:36
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-01 11:39:58
 * @Description: 输出组件。文件写入，支持裸流写入及mp4、mkv、flv、ts及ps封装格式写入。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once


#include "module/module_media.hpp"
namespace FFMedia
{
class generalFileWrite;

class ModuleFileWriter : public ModuleMedia
{
    friend class ModuleFileWriterExtend;

private:
    std::string filepath;
    std::shared_ptr<generalFileWrite> writer;
    std::mutex extend_mtx;
    bool video_parameter_set = false;
    bool audio_parameter_set = false;

public:
    /**
     * @description: ModuleFileWriter 的构建函数。
     * @param {string} path 媒体文件路径。
     * @return {*}
     */
    ModuleFileWriter(const std::string& path);
    ModuleFileWriter(const ImagePara& para, const std::string& path);
    ~ModuleFileWriter();
    /**
     * @description: 改变对象写入媒体文件名称。此调用应在对象停止时使用。
     * @param {string} file_name    媒体文件名称.
     * @return {int}                成功返回 0，失败返回负数。
     */
    int changeFileName(const std::string& file_name);

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
     * @description: 设置媒体附件数据。
     * @param {MEDIA_BUFFER_TYPE} media_type            媒体流类型。
     * @param {shared_ptr<MediaBuffer>} extra_buffer    含有媒体附加数据的MediaBuffer。
     * @return {int}                                    >= o 为成功，< 0 为错误代码。
     */
    int setExtraBuffer(MEDIA_BUFFER_TYPE media_type, const std::shared_ptr<MediaBuffer>& extra_buffer);

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
     * @description: 手动输入媒体数据。
     * @param {shared_ptr<MediaBuffer>} input_buffer
     * @return {int}    成功返回0，失败返回负数。
     */
    int setInputBuffer(const std::shared_ptr<MediaBuffer>& input_buffer);

protected:
    virtual ConsumeResult doConsume(const std::shared_ptr<MediaBuffer>& input_buffer, std::shared_ptr<MediaBuffer>& output_buffer) override;
    int restart(const std::string& file_name);
    void makeWriter();
};

}  // namespace FFMedia
