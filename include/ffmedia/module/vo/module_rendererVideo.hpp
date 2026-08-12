/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-05-22 17:34:39
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-01 11:39:45
 * @Description: 输出组件。视频渲染器，使用可扩展显示后端和 OpenGL ES 渲染视频。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once

#include <memory>

#include "module/module_media.hpp"

namespace FFMedia
{
class FFMEDIA_API ModuleRendererVideo : public ModuleMedia
{
public:
    /**
     * @description: ModuleRendererVideo 的构建函数。
     * @param {string} title   创建的显示窗口标题名称。
     * @return {*}
     */
    ModuleRendererVideo(const ImagePara para = ImagePara(), const std::string& title = "");
    ~ModuleRendererVideo();

    /**
     * @description: 设置显示窗口位置及大小。
     * @param {uint32_t} x  显示窗口起点x坐标。
     * @param {uint32_t} y  显示窗口起点y坐标。
     * @param {uint32_t} w  显示窗口宽度。
     * @param {uint32_t} h  显示窗口高度。
     * @return {*}
     */
    int setWindowRect(int x, int y, uint32_t w, uint32_t h);

    /**
     * @description: 设置显示图像位置及大小。
     * @param {int} x       显示图像在窗口的x坐标。
     * @param {int} y       显示图像在窗口的y坐标。
     * @param {uint32_t} w  显示图像宽度。
     * @param {uint32_t} h  显示图像高度。
     * @return {*}
     */
    int setImageRect(int x, int y, uint32_t w, uint32_t h);
    /**
     * @description: 设置窗口可见性。
     * @param {bool} isVisible
     * @return {*}
     */
    int setWindowVisibility(bool isVisible);

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;

    /**
     * @description: 改变对象的输出图像分辨率和清除窗口。此调用应在对象停止时使用。
     * @param {int} width   图像宽度。
     * @param {int} height  图像高度。
     * @return {*}
     */
    int changeOutputResolution(int width, int height);

protected:
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;
    void resetRendererConfig();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace FFMedia
