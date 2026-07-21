/*
 * @Author: Kaison Deng dkx@t-chip.com.cn
 * @Date: 2026-07-02 09:04:17
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-02 19:40:33
 * @Description: 视频拼接模块， 将多个输入模块的视频帧进行拼接，输出一个视频流
 * Copyright (c) 2026-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once

#include <unordered_map>

#include "module/module_media.hpp"

namespace FFMedia
{

class ModuleRga;

class ModuleVideoStack : public ModuleMedia
{
public:
    /**
     * @description: 构造函数
     * @param {string&} module_name     模块名称
     * @param {int} width               视频输出宽度
     * @param {int} height              视频输出高度
     * @param {float} fps               视频输出帧率
     * @return {*}
     */
    ModuleVideoStack(const std::string& module_name, int width, int height, float fps);
    ~ModuleVideoStack() override;

    /**
     * @description: 为模块添加视频输入模块
     * @param {string&} id                               视频输入模块ID
     * @param {std::shared_ptr<ModuleMedia>} module      视频输入模块
     * @param {ImageCrop&} stack_params                  模块拼接参数
     * @return {*}
     */
    void addInputModule(const std::string& id, std::shared_ptr<ModuleMedia> module,
                        const ImageCrop& stack_params);
    /**
     * @description: 移除模块的视频输入模块
     * @param {string&} id                               视频输入模块ID
     * @return {*}
     */
    void removeInputModule(const std::string& id);
    /**
     * @description: 设置视频输入模块的拼接参数
     * @param {string} &id                               视频输入模块ID
     * @param {ImageCrop&} stack_params                  模块拼接参数
     * @return {*}
     */
    void setModuleStackParams(const std::string& id, const ImageCrop& stack_params);

    int init() override;

protected:
    virtual ProduceResult doProduce(std::shared_ptr<MediaBuffer>& output_buffer) override;

private:
    std::mutex _maps_mtx;
    std::unordered_map<std::string, std::shared_ptr<ModuleRga>> _stack_maps;
    std::unordered_map<std::string, std::shared_ptr<ModuleMedia>> _input_maps;

    int _frame_interval;
    uint64_t _pts;
    std::chrono::time_point<std::chrono::steady_clock> _last_tp;

    std::mutex _buf_mtx;
    std::shared_ptr<VideoBuffer> _cache_buffer;
};

}  // namespace FFMedia