#ifndef __MODULE_FILEWRITER_HPP__
#define __MODULE_FILEWRITER_HPP__

#include "module/module_media.hpp"
class generalFileWrite;

class ModuleFileWriter : public ModuleMedia
{
private:
    string filepath;
    uint32_t file_number;
    uint32_t max_frame_count;
    uint32_t cur_frame_count;
    bool video_extra_flag;
    generalFileWrite* writer;

public:
    ModuleFileWriter(ImagePara& para, string path);
    ~ModuleFileWriter();
    int init();
    int restart(string file_name);
    void setMaxFrameCount(uint32_t frame_count);
    void setVideoParameter(int width, int height, media_codec_t type);
    void setVideoExtraData(const uint8_t* extra_data, unsigned extra_size);
    void setAudioParameter(int channel_count, int bit_per_sample, int sample_rate, media_codec_t type);
    void setAudioExtraData(const uint8_t* extra_data, unsigned extra_size);

protected:
    virtual EnQueueResult doEnQueue(MediaBuffer* input_buffer, MediaBuffer* output_buffer) override;
};

#endif
