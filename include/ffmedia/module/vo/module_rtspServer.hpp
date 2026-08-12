/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-01 11:39:27
 * @Description: 输出组件。rtsp服务器，支持tcp和udp推流。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once


#include "module/module_media.hpp"

namespace FFMedia
{
class FFMEDIA_API ModuleRtspServer : public ModuleMedia
{
private:
    class Impl;
    std::unique_ptr<Impl> impl_;


protected:
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;
    virtual bool setup() override;

public:
    /**
     * @description: ModuleRtspServer 的构建函数。
     * @param {const std::string&} path  rtsp地址路径。
     * @param {int} port    rtsp端口号。
     * @return {*}
     */
    ModuleRtspServer(const std::string& path = "/live", int port = 554);
    ModuleRtspServer(const ImagePara& para, const std::string& path, int port);
    ~ModuleRtspServer();

    /**
     * @description: 设置媒体数据参数及附加数据。此调用会创建媒体轨道。
     * @param {shared_ptr<MediaBuffer>} extra_buffer    含有媒体数据类型和附加数据的MediaBuffer。
     * @return {int}                                    >= o 为成功，< 0 为错误代码。
     */
    int setExtraBuffer(const std::shared_ptr<MediaBuffer>& extra_buffer);

    /**
     * @description: 设置身份信息，用于身份验证。此调用应在对象初始化之前调用。
     * @param {string} &realm       领域
     * @param {string} &username    用户名称
     * @param {string} &password    用户密码
     * @return {*}
     */
    void setAuthInfo(const std::string& realm, const std::string& username, const std::string& password);

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    int init() override;
};

typedef ModuleRtspServer ModuleRtspServerVideoTrack;

}  // namespace FFMedia
