#ifndef __MODULE_FILEREADER_HPP__
#define __MODULE_FILEREADER_HPP__

#include "module/module_media.hpp"
class generalFileRead;

class ModuleFileReader : public ModuleMedia
{
public:
private:
    char* filepath;
    size_t fileSize;
    generalFileRead* reader;

    bool loopMode;

protected:
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> output_buffer) override;

public:
    ModuleFileReader(const char* path, bool loop_play);
    ~ModuleFileReader();
    int init();
    const uint8_t* audioExtraData();
    unsigned audioExtraDataSize();
    const uint8_t* videoExtraData();
    unsigned videoExtraDataSize();
    int setFileReaderSeek(int64_t ms_time);
    int64_t getFileReaderMaxSeek();
};

#endif
