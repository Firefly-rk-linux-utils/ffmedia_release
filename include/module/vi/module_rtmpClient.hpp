/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-04-25 12:52:36
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-09 15:36:10
 * @Description:兼容输入和输出组件。Rtmp客户端，支持推流到Rtmp服务器和从Rtmp服务器拉流。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE_RTMPCLIENT_HPP__
#define __MODULE_RTMPCLIENT_HPP__

#include "module/module_media.hpp"
class rtmpClient;

class ModuleRtmpClient : public ModuleMedia
{
public:
    /**
     * @description: ModuleRtmpClient 构建函数.
     * @param {string} rtmp_url     Rtmp服务器地址。
     * @param {ImagePara} para      输入图像参数，。
     * @param {int} _publish        推流和拉流标志；1为拉流，0为推流。
     * @return {*}
     */
    ModuleRtmpClient(string rtmp_url, ImagePara para = ImagePara(), int _publish = 1);
    ~ModuleRtmpClient();

    /**
     * @description: 改变Rtmp服务器地址，此调用应在对象停止时使用。
     * @param {string} rtmp_url     新的rtmp服务器地址。
     * @param {int} _publish        推流和拉流标志。
     * @return {int}                成功返回0，失败返回负数。
     */
    int changeSource(string rtmp_url, int _publish = 1);

    /**
     * @description: 初始化 ModuleRtmpClient 对象。
     * @return {int} 成功返回0，失败返回负数。
     */
    int init() override;

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
     * @description: 设置获取网络数据的超时时间。
     * @param {unsigned} sec    秒。
     * @param {unsigned} usec   微秒。
     * @return {*}
     */
    void setTimeOutSec(int sec, int usec);

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;
    virtual void bufferReleaseCallBack(shared_ptr<MediaBuffer> buffer) override;

private:
    shared_ptr<rtmpClient> rtmp_client;
    string url;
    int publish;
    shared_ptr<MediaBuffer> probe_buffer;
    bool first_audio_frame;
};


#endif