#ifndef __MODULE_FILEREADER_HPP__
#define __MODULE_FILEREADER_HPP__

#include "module/module_media.hpp"
class generalFileRead;

class ModuleFileReader : public ModuleMedia
{
public:
private:
    string filepath;
    size_t fileSize;
    shared_ptr<generalFileRead> reader;
    bool first_audio_frame;
    bool loopMode;

protected:
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;

public:
    ModuleFileReader(string path, bool loop_play = false);
    ~ModuleFileReader();
    int changeSource(string path, bool loop_play = false);
    int init() override;
    const uint8_t* audioExtraData();
    unsigned audioExtraDataSize();
    const uint8_t* videoExtraData();
    unsigned videoExtraDataSize();
    int setFileReaderSeek(int64_t ms_time);
    int64_t getFileReaderMaxSeek();
};

#endif
