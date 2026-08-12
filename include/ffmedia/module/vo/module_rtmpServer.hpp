/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-04-25 12:52:36
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-01 11:39:39
 * @Description: 输出组件。rtmp服务器。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once


#include "module/module_media.hpp"


namespace FFMedia
{
class FFMEDIA_API ModuleRtmpServer : public ModuleMedia
{
private:
    class Impl;
    std::unique_ptr<Impl> impl_;

protected:
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;

public:
    /**
     * @description: ModuleRtmpServer 的构建函数。
     * @param {char*} path  推流路径。
     * @param {int} port    推流端口。
     * @return {*}
     */
    ModuleRtmpServer(const char* path = "/live", int port = 1935);
    ModuleRtmpServer(const ImagePara& para, const char* path, int port);
    ~ModuleRtmpServer();

    /**
     * @description: 初始化对象。
     * @return {int} 成功返回 0，失败返回负数。
     */
    virtual int init() override;

    /**
     * @description: 设置最多rtmp客户端连接数量。
     * @param {int} count   客户端数量。
     * @return {*}
     */
    void setMaxClientCount(int count);

    /**
     * @description: 获取最多rtmp客户端连接数量。
     * @return {int} 返回最大客户端数量。
     */
    int getMaxClientCount();

    /**
     * @description: 获取当前连接的rtmp客户端数量。
     * @return {int} 返回当前连接的客户端数量。
     */
    int getCurClientCount();

    /**
     * @description: 设置最大超时次数。
     * @param {int} count
     * @return {*}
     */
    void setMaxTimeOutCount(int count);
    int getMaxTimeOutCount();

    /**
     * @description: 设置超时时间。
     * @param {int} sec
     * @param {int} usec
     * @return {*}
     */
    void setTimeOutSec(int sec, int usec);
};

}  // namespace FFMedia
