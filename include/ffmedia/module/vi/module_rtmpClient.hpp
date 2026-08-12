/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-04-25 12:52:36
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-01 11:40:25
 * @Description:兼容输入和输出组件。Rtmp客户端，支持推流到Rtmp服务器和从Rtmp服务器拉流。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once


#include "module/module_media.hpp"
namespace FFMedia
{
class FFMEDIA_API ModuleRtmpClient : public ModuleMedia
{
public:
    /**
     * @description: ModuleRtmpClient 构建函数.
     * @param {string} rtmp_url     Rtmp服务器地址。
     * @param {ImagePara} para      输入图像参数，。
     * @param {int} _publish        推流和拉流标志；1为拉流，0为推流。
     * @return {*}
     */
    ModuleRtmpClient(std::string rtmp_url = "", ImagePara para = ImagePara(), int _publish = 1);
    ~ModuleRtmpClient();

    /**
     * @description: 改变Rtmp服务器地址，此调用应在对象停止时使用。
     * @param {string} rtmp_url     新的rtmp服务器地址。
     * @param {int} _publish        推流和拉流标志。
     * @return {int}                成功返回0，失败返回负数。
     */
    int changeSource(std::string rtmp_url, int _publish = 1);

    /**
     * @description: 初始化 ModuleRtmpClient 对象。
     * @return {int} 成功返回0，失败返回负数。
     */
    int init() override;

    /**
     * @description: 获取指定类型的媒体附加数据。此调用应在对象初始化之后调用。
     * @param {MEDIA_BUFFER_TYPE} meida_type    媒体类型。
     * @return {shared_ptr<MediaBuffer>}        成功返回含有附加数据及媒体参数的MediaBuffer，失败返回空指针。
     */
    std::shared_ptr<MediaBuffer> getExtraBuffer(MEDIA_BUFFER_TYPE media_type);
    /**
     * @description: 设置获取网络数据的超时时间。
     * @param {unsigned} sec    秒。
     * @param {unsigned} usec   微秒。
     * @return {*}
     */
    void setTimeOutSec(int sec, int usec);

protected:
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;
    virtual ProduceResult doProduce(std::shared_ptr<MediaBuffer>& output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;
    virtual void bufferReleaseCallBack(const std::shared_ptr<MediaBuffer>& buffer) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};


}  // namespace FFMedia
