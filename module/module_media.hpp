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
#include <condition_variable>
#include <functional>
#include <thread>

#include "base/pixel_fmt.hpp"
#include "base/video_buffer.hpp"
#include "base/ff_type.hpp"

class Synchronize;

using namespace std;
#ifdef PYBIND11_MODULE
#include <pybind11/pybind11.h>
using callback_handler = std::function<void(pybind11::object, MediaBuffer*)>;
#else
using callback_handler = std::function<void(void*, MediaBuffer*)>;
#endif

enum ModuleStatus {
    STATUS_CREATED = 0,
    STATUS_STARTED,
    STATUS_EOS,
    STATUS_STOPED,
};

class ModuleMedia
{

public:
    enum EnQueueResult {
        ENQUEUE_SUCCESS = 0,
        ENQUEUE_WAIT_FOR_CONSUMER,
        ENQUEUE_WAIT_FOR_PRODUCTOR,
        ENQUEUE_NEED_REPEAT,
        ENQUEUE_SKIP,
        ENQUEUE_BYPASS,
        ENQUEUE_EOS,
        ENQUEUE_FAILED,
    };

    enum ProcessResult {
        PROCESS_SUCCESS = 0,
        PROCESS_CONTINUE,
        PROCESS_EMPTY,
        PROCESS_BYPASS,
        PROCESS_EOS,
        PROCESS_FAILED,
    };

protected:
    const char* name;
    bool work_flag;
    thread* work_thread;

    uint16_t buffer_count;
    size_t buffer_size;
    vector<MediaBuffer*> buffer_pool;

    //ring queue, point to buffer_pool
    vector<MediaBuffer*> buffer_ptr_queue;

    //to be a consumer
    //each consumer has a buffer queue tail
    //point to the productor's buffer_ptr_queue
    //record the position of the buffer the current consumer consume
    uint16_t input_buffer_queue_tail;

    //to be a producer
    //record the head in ring queue buffer_ptr_queue
    uint16_t output_buffer_queue_head;

    bool input_buffer_queue_empty;
    bool input_buffer_queue_full;

    ModuleMedia* productor;
    vector<ModuleMedia*> consumers;
    uint16_t consumers_count;

    ImagePara input_para = {0, 0, 0, 0, 0};
    ImagePara output_para = {0, 0, 0, 0, 0};

    /*Don't need to do anything for enqueue buffer after Setup, Like a black hole*/
    bool HoleModule;
    ModuleStatus module_status;

#ifdef PYBIND11_MODULE
    pybind11::object callback_ctx;
#else
    void* callback_ctx;
#endif
    callback_handler output_data_callback;

    bool mppModule = false;
    mutex mtx;
    condition_variable produce, consume;

    ModuleMedia* external_consumer;
    int media_type;
    int index;
    uint64_t blocked_as_consumer;
    uint64_t blocked_as_porductor;

    Synchronize* sync;

private:
    void resetModule();
    int nextBufferPos(uint16_t pos);

    void produceOneBuffer(MediaBuffer* buffer);
    void consumeOneBuffer();

    MediaBuffer* inputBufferQueueTail();
    bool inputBufferQueueIsFull();
    bool inputBufferQueueIsEmpty();

protected:
    virtual EnQueueResult doEnQueue(MediaBuffer* input_buffer, MediaBuffer* output_buffer);
    virtual ProcessResult doProcess(MediaBuffer* buffer);

    virtual int initBuffer();
    int initBuffer(VideoBuffer::BUFFER_TYPE buffer_type);

    MediaBuffer* outputBufferQueueHead();
    void setOutputBufferQueueHead(MediaBuffer* buffer);
    void fillAllOutputBufferQueue();

    virtual void bufferReleaseCallBack(MediaBuffer* buffer);
    std::cv_status waitForProduce();
    std::cv_status waitForConsume();

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

public:
    ModuleMedia();
    ModuleMedia(const char* name_);
    virtual ~ModuleMedia();
    bool isHoleModule();

    int initBufferFromExternalBuffer(MediaBuffer** buffers, uint16_t count);
#ifdef PYBIND11_MODULE
    void setOutputDataCallback(pybind11::object& ctx, callback_handler callback);
#else
    void setOutputDataCallback(void* ctx, callback_handler callback);
#endif
    void start();
    void stop();

    void setProductor(ModuleMedia* module);
    ModuleMedia* getProductor();
    void addConsumer(ModuleMedia* consumer);
    void removeConsumer(ModuleMedia* consumer);

    const char* getName() const { return name; }

    uint16_t getBufferCount() const { return buffer_count; }
    void setBufferCount(uint16_t bufferCount) { buffer_count = bufferCount; }
    MediaBuffer* getBufferFromIndex(uint16_t index);

    ImagePara getInputImagePara() const { return input_para; }
    void setInputImagePara(const ImagePara& inputPara) { input_para = inputPara; }

    ImagePara getOutputImagePara() const { return output_para; }
    void setOutputImagePara(const ImagePara& outputPara) { output_para = outputPara; }

    ModuleStatus getModuleStatus() const { return module_status; }

    void addExternalConsumer(const char* name);
    MediaBuffer* externalGetoutputBuffer();
    void externalConsumeBuffer();
    bool readyForExternalConsumer();

    size_t getBufferSize() const { return buffer_size; }
    void setBufferSize(const size_t& bufferSize) { buffer_size = bufferSize; }

    uint16_t getConsumersCount() const { return consumers_count; }
    ModuleMedia* getConsumer(uint16_t index) const { return consumers[index]; }
    int getMediaType() const { return media_type; }

    void dumpPipe();
    void dumpPipeSummary();

    void setSynchronize(Synchronize* syn) { sync = syn; }
};

#endif
