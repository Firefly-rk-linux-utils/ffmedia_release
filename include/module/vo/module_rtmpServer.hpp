/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-04-25 12:52:36
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-09 17:20:09
 * @Description: 输出组件。rtmp服务器。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE_RTMPSERVER_HPP__
#define __MODULE_RTMPSERVER_HPP__

#include <string>
#include "module/module_media.hpp"


class rtmpServer;
class ModuleRtmpServer : public ModuleMedia
{
private:
    shared_ptr<rtmpServer> rtmp_server;
    int push_port;
    string push_path;

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;

public:
    /**
     * @description: ModuleRtmpServer 的构建函数。
     * @param {char*} path  推流路径。
     * @param {int} port    推流端口。
     * @return {*}
     */
    ModuleRtmpServer(const char* path, int port);
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

#endif
