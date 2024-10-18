/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-18 13:51:24
 * @Description: 输入源组件, 支持MIPI CSI摄像头和USB摄像头。通过V4L2接口操作获取数据。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE_CAM_HPP__
#define __MODULE_CAM_HPP__

#include "module/module_media.hpp"
class v4l2Cam;

class ModuleCam : public ModuleMedia
{
private:
    string dev;
    shared_ptr<v4l2Cam> cam;

protected:
    virtual bool setup() override;
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> output_buffer) override;
    virtual void bufferReleaseCallBack(shared_ptr<MediaBuffer> buffer) override;

public:
    /**
     * @description: ModuleCam 的构造函数.
     * @param {string} vdev     视频设备路径.
     * @return {*}
     */
    ModuleCam(string vdev);
    ~ModuleCam();

    /**
     * @description: 改变视频采集设备。此调用应在对象停止时使用。
     * @param {string} vdev     视频设备路径。
     * @return {int}            成功返回 0，失败返回负数。
     */
    int changeSource(string vdev);

    /**
     * @description: 系统调用操纵视频设备的底层设备参数。此调用应在对象初始化后使用。
     * @param {unsigned long} cmd   设备相关请求命令。
     * @param {void*} arg           指向参数的无类型指针。
     * @return {int}                通常，如果成功，返回0。一些ioctl()请求使用返回值作为输出参数，并在成功时返回一个非负值。如果出现错误，则返回-1，并适当地设置errno。
     */
    int camIoctlOperation(unsigned long cmd, void* arg);

    /**
     * @description: 设置采集数据的超时时间。
     * @param {unsigned} sec    秒。
     * @param {unsigned} usec   微秒。
     * @return {*}
     */
    void setTimeOutSec(unsigned sec, unsigned usec);

    /**
     * @description: 初始化 ModuleCam。
     * @return {int}    成功返回 0，失败返回负数。
     */
    int init() override;
};
#endif
