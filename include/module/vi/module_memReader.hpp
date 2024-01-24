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
    int changeInputPara(const ImagePara& para);
    int init() override;
    int setInputBuffer(void* buf, size_t bytes, int buf_fd = -1);
    int waitProcess(int timeout_ms);
    void setProcessStatus(DATA_PROCESS_STATUS status);
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