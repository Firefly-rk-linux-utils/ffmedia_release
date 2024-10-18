/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-19 16:55:19
 * @Description: 输入源组件，支持从内存读取媒体数据。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE_MEMREADER_HPP__
#define __MODULE_MEMREADER_HPP__

#include "module/module_media.hpp"

class ModuleMemReader : public ModuleMedia
{
public:
    enum DATA_PROCESS_STATUS {
        PROCESS_STATUS_EXIT,     // 退出状态，对象不再阻塞处理输入数据。
        PROCESS_STATUS_HANDLE,   // 处理状态，对象正在处理输入数据。
        PROCESS_STATUS_PREPARE,  // 准备状态，输入数据已经准备完成，等待对象处理。
        PROCESS_STATUS_DONE      // 完成状态，输入数据已经被处理完成，等待新的数据输入。
    };

public:
    /**
     * @description: ModuleMemReader 的构造函数。
     * @param {ImagePara&} para     描述输入数据的图像参数。
     * @return {*}
     */
    ModuleMemReader(const ImagePara& para);
    ~ModuleMemReader();

    /**
     * @description: 改变输入数据的图像参数。 此调用应在对象停止时使用。
     * @param {ImagePara&} para     描述输入数据的图像参数。
     * @return {int}                成功返回0，失败返回负数。
     */
    int changeInputPara(const ImagePara& para);

    /**
     * @description: 初始化 ModuleMemReader 对象.
     * @return {int}  成功返回0。
     */
    int init() override;

    /**
     * @description: 设置输入媒体数据.
     * @param {void*} buf       输入数据的内存地址。
     * @param {size_t} bytes    输入数据的大小（字节）。
     * @param {int} buf_fd      输入内存地址的描述符，供零拷贝使用。
     * @param {int64_t} pts     时间戳（微秒）。
     * @return {int}            成功返回0，失败返回-1。
     */
    int setInputBuffer(void* buf, size_t bytes, int buf_fd = -1, int64_t pts = 0);

    /**
     * @description: 等待对象处理输入数据。
     * @param {int} timeout_ms  等待超时时间（毫秒）.
     * @return {int}            等待成功返回0，超时返回-100。
     */
    int waitProcess(int timeout_ms);

    /**
     * @description: 设置对象处理数据状态。
     * @param {DATA_PROCESS_STATUS} status 数据处理状态。
     * @return {*}
     */
    void setProcessStatus(DATA_PROCESS_STATUS status);

    /**
     * @description: 获取对象处理数据状态。
     * @return {DATA_PROCESS_STATUS} 返回对象处理状态。
     */
    DATA_PROCESS_STATUS getProcessStatus();

    void setBufferCount(uint16_t buffer_count) { (void)buffer_count; }

protected:
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> output_buffer) override;
    virtual void bufferReleaseCallBack(shared_ptr<MediaBuffer> buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;

private:
    shared_ptr<VideoBuffer> buffer;
    DATA_PROCESS_STATUS op_status;
    std::mutex tMutex;
    std::condition_variable tConVar;
};

#endif /* module_memReader_hpp */