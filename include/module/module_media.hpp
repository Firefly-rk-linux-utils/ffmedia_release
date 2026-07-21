/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:54
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-07-01 11:35:40
 * @Description: 所有组件均派生自ModuleMedia类，ModuleMedia的成员中包含一个消费者队列，记录该组件的所有消费者；
 *               一个MediaBuffer队列，记录该组件所分配的buffer. MediaBuffer队列中存储当前组件的输出数据，
 *               MediaBuffer队列同时也是该组件的所有消费者的输入。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once

#include <queue>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <functional>
#include <thread>

#include "ff_media_producer.hpp"
#include "ff_media_hookable.hpp"

#include "base/pixel_fmt.hpp"
#include "base/video_buffer.hpp"
#include "base/ff_synchronize.hpp"
#include "base/ff_type.hpp"
#include "base/ff_log.h"
#include "base_config.h"

namespace FFMedia
{

enum class ModuleType {
    SRC,  // 源组件
    PRC,  // 处理组件
};

class ModuleMedia : public MediaProducer,
                    public MediaConsumer,
                    public MediaHookable,
                    public std::enable_shared_from_this<ModuleMedia>
{
public:
    /**
     * @description: ModuleMedia的构造函数。
     * @param {char*} name_ 组件名称。
     * @return {*}
     */
    ModuleMedia(const std::string& name_ = "");
    virtual ~ModuleMedia();

    /**
     * @description: 初始化组件。
     * @return {int}    成功返回0，失败返回其他。
     */
    virtual int init() { return 0; };

    /**
     * @description: 启动组件工作线程。
     * @return {*}
     */
    void start();
    /**
     * @description: 停止组件工作线程。
     * @return {*}
     */
    void stop();

    /**
     * @description: 设置组件的生产者。
     * @param {shared_ptr<ModuleMedia>} module 生产者组件。
     * @return {*}
     */
    void setProductor(std::shared_ptr<ModuleMedia> module);

    /**
     * Connect a producer and select its output channels. Empty selection means
     * all channels. The connection is rejected when none of the selected
     * channels satisfies this module's input requirements, or when a matched
     * input with allow_multiple=false is already used by another producer.
     */
    int setProductor(std::shared_ptr<ModuleMedia> module,
                     const MediaChannelSelection& selection);
    int connectProducer(std::shared_ptr<ModuleMedia> module,
                        const MediaChannelSelection& selection = MediaChannelSelection());
    /**
     * @description: 移除组件的生产者。
     * @param {shared_ptr<ModuleMedia>} module 生产者组件。如果为空则移除所有生产者。
     * @return {*}
     */
    void removeProductor(std::shared_ptr<ModuleMedia> module);

public:
    /**
     * @description: 设置对象的缓冲区数量。
     * @param {uint16_t} bufferCount    缓冲区数量。
     * @return {*}
     */
    void setBufferCount(uint16_t bufferCount) { buffer_count = bufferCount; }

    /**
     * @description: 获取对象缓冲区数量。
     * @return {*}
     */
    uint16_t getBufferCount() const { return buffer_count; }

    /**
     * @description: 设置对象单个缓冲区的大小。从InputImagePara计算缓冲区大小不满足时，可通过该接口手动设置。此调用应在对象初始化之前设置。
     * @param {size_t&} bufferSize  缓冲区大小。
     * @return {*}
     */
    void setBufferSize(const size_t& bufferSize) { buffer_size = bufferSize; }
    /**
     * @description: 获取对象的单个缓冲区大小。
     * @return {size_t} 返回对象缓冲区大小。
     */
    size_t getBufferSize() const;

    /**
     * @description: 获取对象指定索引值的缓冲区。
     * @param {uint16_t} index              缓冲区索引值。
     * @return {shared_ptr<MediaBuffer>}    返回缓冲区。
     */
    std::shared_ptr<MediaBuffer> getBufferFromIndex(uint16_t index);

    /**
     * @description: 持有输出缓冲区，成功持有后，对象不会使用该缓冲区。
     * @param {shared_ptr<MediaBuffer>} &obuf   需要持有的输出缓冲区。
     * @return {int}                            成功返回0，失败返回负数。
     */
    static int holdOutputBuffer(const std::shared_ptr<MediaBuffer>& obuf);

    /**
     * @description: 释放输出缓冲区。
     * @param {shared_ptr<MediaBuffer>} &obuf   需要释放的输出缓冲区。
     * @return {int}                            成功返回0，失败返回负数。
     */
    static int releaseOutputBuffer(const std::shared_ptr<MediaBuffer>& obuf);

    /**
     * @description: 设置对象的输入数据的图像参数。
     * @param {ImagePara&} inputPara 图像参数。
     * @return {*}
     */
    void setInputImagePara(const ImagePara& inputPara) { input_para = inputPara; }
    /**
     * @description: 获取对象的输入数据的图像参数。
     * @return {ImagePara}  返回对象的输出数据的图像参数。
     */
    const ImagePara& getInputImagePara() const { return input_para; }

    /**
     * @description: 设置对象的输出数据的图像参数。
     * @param {ImagePara&} outputPara   图像参数。
     * @return {*}
     */
    void setOutputImagePara(const ImagePara& outputPara);
    /**
     * @description: 获取对象的输出数据的图像参数。
     * @return {ImagePara}  返回对象的输出数据的图像参数。
     */
    const ImagePara& getOutputImagePara() const { return output_para; }

    /** Replace or query the media channels published by this module. */
    void setOutputMediaChannels(const std::vector<MediaChannelInfo>& channels);
    void addOutputMediaChannel(const MediaChannelInfo& channel);
    void clearOutputMediaChannels();
    std::vector<MediaChannelInfo> getOutputMediaChannels() const;
    bool getOutputMediaChannel(MediaChannelId id, MediaChannelInfo& channel) const;

    /** Describe accepted input formats before connecting producers. */
    void setInputMediaChannelRequirements(
        const std::vector<MediaChannelRequirement>& requirements);
    void addInputMediaChannelRequirement(const MediaChannelRequirement& requirement);
    const std::vector<MediaChannelRequirement>& getInputMediaChannelRequirements() const
    {
        return input_channel_requirements;
    }

    /** Matched producer channels available to init(). */
    const std::vector<MediaInputChannel>& getInputMediaChannels() const
    {
        return input_channels;
    }
    bool getInputMediaChannel(MediaChannelId input_id, MediaInputChannel& channel) const;

    /**
     * @description: 获取对象名称。
     * @return {const std::string&}    返回对象名称字符串。
     */
    const std::string& getName() const { return name; }

    /**
     * @description: 获取对象当前的工作状态。
     * @return {MediaStatus} 返回对象状态。
     */
    MediaStatus getModuleStatus() const { return module_status; }
    /**
     * @description: 获取对象输入数据的媒体类型。
     * @return {MEDIA_BUFFER_TYPE}
     */
    MEDIA_BUFFER_TYPE getMediaType() const { return media_type; }

    /**
     * @description: 设置音视频同步组件。
     * @param {shared_ptr<Synchronize>} syn
     * @return {*}
     */
    void setSynchronize(std::shared_ptr<Synchronize> syn) { sync = syn; }

    /**
     * @description: 为组件添加一个外部的消费者。其功能与添加回调相似，两者区别在于组件可以添加多个外部消费者，但是只能添加一个回调函数。
     * @param {const std::string&} name     外部消费者名称
     * @param {MediaBufferHooker} callback   外部消费者数据处理函数。
     * @return {shared_ptr<ModuleMedia>}    返回外部组件。
     */
    std::shared_ptr<ModuleMedia> addExternalConsumer(const std::string& name,
                                                     MediaBufferHooker external_cb);

    /**
     * @description: 打印出以当前组件为输入源的整个Pipe结构
     * @return {*}
     */
    void dumpPipe();
    /**
     * @description: 打印Pipe结构，以及Pipe中各个节点的运行统计数据。
     * @return {*}
     */
    void dumpPipeSummary();

    /**
     * @description: 压入MediaBuffer到输入队列，并通知模块消费处理。
     * @param {shared_ptr<MediaBuffer>} buffer  接收的MediaBuffer。
     * @return {*}
     */
    void receiveMediaBuffer(const std::shared_ptr<MediaBuffer>& buffer);
    void receiveMediaBuffer(const MediaBufferContext& context) override;
    /**
     * @description: 清理模块没有消费的输入MediaBuffer。
     * @return {*}
     */
    void clearInputBufferQueue();
    /**
     * @description: 设置输入队列的大小,默认为1024，当队列满时，新来的MediaBuffer会被丢弃。
     * @param {size_t} size
     * @return {*}
     */
    void setInputBufferQueueSize(size_t size);
    size_t getInputBufferQueueSize() const;

protected:
    enum ConsumeResult {
        CONSUME_SUCCESS = 0,
        CONSUME_WAIT_FOR_CONSUMER,
        CONSUME_WAIT_FOR_PRODUCTOR,
        CONSUME_NEED_REPEAT,
        CONSUME_SKIP,
        CONSUME_BYPASS,
        CONSUME_EOS,
        CONSUME_FAILED,
    };

    enum ProduceResult {
        PRODUCE_SUCCESS = 0,
        PRODUCE_CONTINUE,
        PRODUCE_EMPTY,
        PRODUCE_BYPASS,
        PRODUCE_EOS,
        PRODUCE_FAILED,
    };

protected:
    virtual ConsumeResult doConsume(const std::shared_ptr<MediaBuffer>& input_buffer, std::shared_ptr<MediaBuffer>& output_buffer);
    virtual ConsumeResult doConsume(const MediaBufferContext& input,
                                    std::shared_ptr<MediaBuffer>& output_buffer);
    virtual ProduceResult doProduce(std::shared_ptr<MediaBuffer>& buffer);

    virtual int initBuffer();
    int initBuffer(VideoBuffer::BUFFER_TYPE buffer_type);
    void setupBufferQueueCallbacks();
    void resetBufferQueueCallbacks();

    std::shared_ptr<MediaBuffer>& outputBufferQueueHead();
    void setOutputBufferQueueHead(const std::shared_ptr<MediaBuffer>& buffer);
    void clearCacheBufferQueue();

    void bufferRefZeroCallBack(const std::shared_ptr<MediaBuffer>& buffer);
    virtual void bufferReleaseCallBack(const std::shared_ptr<MediaBuffer>& buffer);

    std::shared_ptr<MediaBuffer>& waitProduceBuffer();
    void notifyProduce();
    void notifyConsume();

    inline void setModuleStatus(const MediaStatus& status);

    void work();
    void _dumpPipe(int depth, std::function<void(ModuleMedia*, int)> func);
    static void printChannelPara(ModuleMedia* module, int depth);
    static void printSummary(ModuleMedia* module, int depth);
    virtual bool setup()
    {
        return true;
    }

    virtual bool teardown()
    {
        return true;
    }

    virtual void reset();


private:
    void resetModule();

    void produceOneBuffer(const std::shared_ptr<MediaBuffer>& buffer);
    void consumeOneBuffer(const std::shared_ptr<MediaBuffer>& buffer);

private:
    bool work_flag;
    std::thread* work_thread;

    // to be a producer
    // record the head in ring queue buffer_ptr_queue
    uint16_t output_buffer_queue_head;

    std::vector<std::weak_ptr<ModuleMedia>> producers;

    MediaStatus module_status;
    MediaBufferHooker external_cb;

    uint64_t blocked_as_consumer;
    uint64_t blocked_as_porductor;

protected:
    ModuleType module_type;
    std::string name;
    uint16_t buffer_count;
    size_t buffer_size;
    std::vector<std::shared_ptr<MediaBuffer>> buffer_pool;

    // ring queue, point to buffer_pool
    std::vector<std::shared_ptr<MediaBuffer>> buffer_ptr_queue;

    std::queue<MediaBufferContext> input_buffer_queue;
    size_t input_buffer_queue_size;
    std::mutex in_queue_mtx;

    ImagePara input_para = {0, 0, 0, 0, 0};
    ImagePara output_para = {0, 0, 0, 0, 0};

    std::vector<MediaChannelInfo> output_channels;
    std::vector<MediaChannelRequirement> input_channel_requirements;
    std::vector<MediaInputChannel> input_channels;

    std::mutex mtx;
    std::condition_variable produce, consume;

    MEDIA_BUFFER_TYPE media_type;
    std::shared_ptr<Synchronize> sync;
    bool initialize;
    const uint32_t produce_timeout = 5000;
    const uint32_t consume_timeout = 5000;

    bool is_clear_cache;
};

}  // namespace FFMedia
