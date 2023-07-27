#ifndef __MODULE_MEMREADER_HPP__
#define __MODULE_MEMREADER_HPP__

#include "module/module_media.hpp"

class ModuleMemReader : public ModuleMedia
{
public:
    enum DATA_PROCESS_STATUS {
        PROCESS_STATUS_EXIT,
        PROCESS_STATUS_HANDLE,
        PROCESS_STATUS_PREPARE,
        PROCESS_STATUS_DONE
    };
public:
    ModuleMemReader(const ImagePara& para);
    ~ModuleMemReader();
    int init();
    int setInputBuffer(void *buf, size_t bytes);
    int waitProcess(int timeout_ms);
    void setProcessStatus(DATA_PROCESS_STATUS status);
    DATA_PROCESS_STATUS getProcessStatus();

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