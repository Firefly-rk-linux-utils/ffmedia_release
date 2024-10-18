/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:54
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-19 16:53:26
 * @Description: 所有组件均派生自ModuleMedia类，ModuleMedia的成员中包含一个消费者队列，记录该组件的所有消费者；
 *               一个MediaBuffer队列，记录该组件所分配的buffer. MediaBuffer队列中存储当前组件的输出数据，
 *               MediaBuffer队列同时也是该组件的所有消费者的输入。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#ifndef __MODULE_MEDIA_HPP__
#define __MODULE_MEDIA_HPP__

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

#include <queue>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <functional>
#include <thread>

#include "base/pixel_fmt.hpp"
#include "base/video_buffer.hpp"
#include "base/ff_synchronize.hpp"
#include "base/ff_type.hpp"
#include "base/ff_log.h"
#include "base_config.h"

using namespace std;
#ifdef PYBIND11_MODULE
#include <pybind11/pybind11.h>
using void_object = pybind11::object;
using void_object_p = pybind11::object&;
#else
using void_object = void*;
using void_object_p = void*;
#endif

using callback_handler = std::function<void(void_object, shared_ptr<MediaBuffer>)>;

enum ModuleStatus {
    STATUS_CREATED = 0,  // 创建状态
    STATUS_STARTED,      // 运行状态
    STATUS_EOS,          // 异常状态
    STATUS_STOPED,       // 停止状态
};

class ModuleMedia : public std::enable_shared_from_this<ModuleMedia>
{
public:
    /**
     * @description: ModuleMedia的构造函数。
     * @param {char*} name_ 组件名称。
     * @return {*}
     */
    ModuleMedia(const char* name_ = NULL);
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
    void setProductor(shared_ptr<ModuleMedia> module);
    /**
     * @description: 获取组件的生产者。
     * @return {shared_ptr<ModuleMedia>} 返回组件的生产者。
     */
    shared_ptr<ModuleMedia> getProductor();

    /**
     * @description: 添加消费者组件到对象的消费队列中。被添加的组件应处于停止状态。
     * @param {shared_ptr<ModuleMedia>} consumer 消费者组件。
     * @return {*}
     */
    void addConsumer(shared_ptr<ModuleMedia> consumer);

    /**
     * @description: 从对象的消费队列中移除该消费者组件。被移除的组件应处于停止状态。
     * @param {shared_ptr<ModuleMedia>} consumer 消费者组件。
     * @return {*}
     */
    void removeConsumer(shared_ptr<ModuleMedia> consumer);

    /**
     * @description: 从对象的消费队列中获取索引值对应消费者组件。
     * @param {uint16_t} index              索引值。
     * @return {shared_ptr<ModuleMedia>&}   返回消费者组件。
     */
    shared_ptr<ModuleMedia>& getConsumer(uint16_t index);

    /**
     * @description: 获取对象的消费队列长度。
     * @return {uint16_t}   返回消费队列长度。
     */
    uint16_t getConsumersCount() const { return consumers_count; }

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
     * @description: 获取对象指定索引值的缓冲区。
     * @param {uint16_t} index              缓冲区索引值。
     * @return {shared_ptr<MediaBuffer>}    返回缓冲区。
     */
    shared_ptr<MediaBuffer> getBufferFromIndex(uint16_t index);


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
    ImagePara getInputImagePara() const { return input_para; }


    /**
     * @description: 设置对象的输出数据的图像参数。
     * @param {ImagePara&} outputPara   图像参数。
     * @return {*}
     */
    void setOutputImagePara(const ImagePara& outputPara) { output_para = outputPara; }
    /**
     * @description: 获取对象的输出数据的图像参数。
     * @return {ImagePara}  返回对象的输出数据的图像参数。
     */
    ImagePara getOutputImagePara() const { return output_para; }

    /**
     * @description: 获取对象名称。
     * @return {const char*}    返回对象名称字符串。
     */
    const char* getName() const { return name; }

    /**
     * @description: 获取对象在其生产者的消费者队列的索引值。
     * @return {int} 返回索引值。没有生产者则小于0。
     */
    int getIndex() const { return index; }

    /**
     * @description: 获取对象当前的工作状态。
     * @return {ModuleStatus} 返回对象状态。
     */
    ModuleStatus getModuleStatus() const { return module_status; }
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
    void setSynchronize(shared_ptr<Synchronize> syn) { sync = syn; }

    /**
     * @description: 为对象添加回调函数，处理对象的输出数据，每次生产数据后，均会被调用。
     * @param {void_object_p} ctx           回调函数上下文。
     * @param {callback_handler} callback   回调函数。
     * @return {*}
     */
    void setOutputDataCallback(void_object_p ctx, callback_handler callback);
    /**
     * @description: 为组件添加一个外部的消费者。其功能与添加回调相似，两者区别在于组件可以添加多个外部消费者，但是只能添加一个回调函数。
     * @param {const char*} name            外部消费者名称
     * @param {void_object_p} ctx           外部消费者上下文。
     * @param {callback_handler} callback   外部消费者数据处理函数。
     * @return {shared_ptr<ModuleMedia>}    返回外部组件。
     */
    shared_ptr<ModuleMedia> addExternalConsumer(const char* name,
                                                void_object_p external_consume_ctx,
                                                callback_handler external_consume);

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
     * @description: 打印出以当前组件为输入源的整个Pipe结构。
     * @return {*}
     */
    void dumpPipe();
    /**
     * @description: 打印Pipe结构，以及Pipe中各个节点的运行统计数据。
     * @return {*}
     */
    void dumpPipeSummary();

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
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer);
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> buffer);

    virtual int initBuffer();
    int initBuffer(VideoBuffer::BUFFER_TYPE buffer_type);

    shared_ptr<MediaBuffer> outputBufferQueueHead();
    void setOutputBufferQueueHead(shared_ptr<MediaBuffer> buffer);
    void fillAllOutputBufferQueue();
    void cleanInputBufferQueue();

    virtual void bufferReleaseCallBack(shared_ptr<MediaBuffer> buffer);
    std::cv_status waitForProduce(std::unique_lock<std::mutex>& lk);
    void waitAllForConsume();
    std::cv_status waitForConsume(std::unique_lock<std::mutex>& lk);

    void notifyProduce();
    void notifyConsume();

    void setModuleStatus(const ModuleStatus& moduleStatus) { module_status = moduleStatus; }

    void work();
    void _dumpPipe(int depth, std::function<void(ModuleMedia*)> func);
    static void printOutputPara(ModuleMedia* module);
    static void printSummary(ModuleMedia* module);
    virtual bool setup()
    {
        return true;
    }

    virtual bool teardown()
    {
        return true;
    }

    int checkInputPara();
    virtual void reset();


private:
    void resetModule();
    int nextBufferPos(uint16_t pos);

    void produceOneBuffer(shared_ptr<MediaBuffer> buffer);
    void consumeOneBuffer();
    void consumeOneBufferNoLock();

    shared_ptr<MediaBuffer> inputBufferQueueTail();
    bool inputBufferQueueIsFull();
    bool inputBufferQueueIsEmpty();

private:
    bool work_flag;
    thread* work_thread;

    // to be a consumer
    // each consumer has a buffer queue tail
    // point to the productor's buffer_ptr_queue
    // record the position of the buffer the current consumer consume
    uint16_t input_buffer_queue_tail;

    // to be a producer
    // record the head in ring queue buffer_ptr_queue
    uint16_t output_buffer_queue_head;

    bool input_buffer_queue_empty;
    bool input_buffer_queue_full;

    weak_ptr<ModuleMedia> productor;
    vector<shared_ptr<ModuleMedia>> consumers;
    uint16_t consumers_count;

    ModuleStatus module_status;

    void_object external_consume_ctx;
    callback_handler external_consume;

    // as a consumer, sequence number in productor's consumer queue
    int index;

    uint64_t blocked_as_consumer;
    uint64_t blocked_as_porductor;

protected:
    const char* name;
    uint16_t buffer_count;
    size_t buffer_size;
    vector<shared_ptr<MediaBuffer>> buffer_pool;

    // ring queue, point to buffer_pool
    vector<shared_ptr<MediaBuffer>> buffer_ptr_queue;

    ImagePara input_para = {0, 0, 0, 0, 0};
    ImagePara output_para = {0, 0, 0, 0, 0};

    void_object callback_ctx;
    callback_handler output_data_callback;

    bool mppModule = false;

    mutex mtx;
    shared_timed_mutex productor_mtx;
    condition_variable produce, consume;

    MEDIA_BUFFER_TYPE media_type;
    shared_ptr<Synchronize> sync;
    bool initialize;
    const uint32_t produce_timeout = 5000;
    const uint32_t consume_timeout = 5000;
};

#endif
