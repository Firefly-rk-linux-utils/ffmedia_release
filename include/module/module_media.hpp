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
    STATUS_CREATED = 0,
    STATUS_STARTED,
    STATUS_EOS,
    STATUS_STOPED,
};

class ModuleMedia : public std::enable_shared_from_this<ModuleMedia>
{
public:
    ModuleMedia(const char* name_ = NULL);
    virtual ~ModuleMedia();

    virtual int init() { return 0; };
    void start();
    void stop();

    void setProductor(shared_ptr<ModuleMedia> module);
    shared_ptr<ModuleMedia> getProductor();

    void addConsumer(shared_ptr<ModuleMedia> consumer);
    void removeConsumer(shared_ptr<ModuleMedia> consumer);

    shared_ptr<ModuleMedia>& getConsumer(uint16_t index);
    uint16_t getConsumersCount() const { return consumers_count; }

    void setBufferCount(uint16_t bufferCount) { buffer_count = bufferCount; }
    uint16_t getBufferCount() const { return buffer_count; }
    shared_ptr<MediaBuffer> getBufferFromIndex(uint16_t index);

    void setInputImagePara(const ImagePara& inputPara) { input_para = inputPara; }
    ImagePara getInputImagePara() const { return input_para; }

    void setOutputImagePara(const ImagePara& outputPara) { output_para = outputPara; }
    ImagePara getOutputImagePara() const { return output_para; }

    const char* getName() const { return name; }
    int getIndex() const { return index; }
    ModuleStatus getModuleStatus() const { return module_status; }
    MEDIA_BUFFER_TYPE getMediaType() const { return media_type; }

    void setSynchronize(shared_ptr<Synchronize> syn) { sync = syn; }

    void setOutputDataCallback(void_object_p ctx, callback_handler callback);
    shared_ptr<ModuleMedia> addExternalConsumer(const char* name,
                                                void_object_p external_consume_ctx,
                                                callback_handler external_consume);

    void setBufferSize(const size_t& bufferSize) { buffer_size = bufferSize; }
    size_t getBufferSize() const;

    void dumpPipe();
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
