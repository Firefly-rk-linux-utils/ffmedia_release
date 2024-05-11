#ifndef __MODULE_FILEWRITER_HPP__
#define __MODULE_FILEWRITER_HPP__

#include "module/module_media.hpp"
class generalFileWrite;

class ModuleFileWriter : public ModuleMedia
{
    friend class ModuleFileWriterExtend;

private:
    string filepath;
    shared_ptr<generalFileWrite> writer;
    mutex extend_mtx;

public:
    ModuleFileWriter(string path);
    ModuleFileWriter(const ImagePara& para, string path);
    ~ModuleFileWriter();
    int changeFileName(string file_name);
    int init() override;
    void setVideoParameter(int width, int height, media_codec_t type);
    void setVideoExtraData(const uint8_t* extra_data, unsigned extra_size);
    void setAudioParameter(int channel_count, int bit_per_sample, int sample_rate, media_codec_t type);
    void setAudioExtraData(const uint8_t* extra_data, unsigned extra_size);

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    int restart(string file_name);
    void makeWriter();
};

class ModuleFileWriterExtend : public ModuleMedia
{
    shared_ptr<ModuleFileWriter> writer;

public:
    ModuleFileWriterExtend(shared_ptr<ModuleFileWriter> module, string path);
    ~ModuleFileWriterExtend();
    int changeFileName(string file_name);
    int init() override;
    void setVideoParameter(int width, int height, media_codec_t type);
    void setVideoExtraData(const uint8_t* extra_data, unsigned extra_size);
    void setAudioParameter(int channel_count, int bit_per_sample, int sample_rate, media_codec_t type);
    void setAudioExtraData(const uint8_t* extra_data, unsigned extra_size);

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
};

#endif
