#ifndef __MODULE_FILEREADER_HPP__
#define __MODULE_FILEREADER_HPP__

#include "module/module_media.hpp"
class MP4DVideo;
class MKVDVideo;

enum FILE_TYPE {
    FILE_TYPE_MP4,
    FILE_TYPE_MKV,
    FILE_TYPE_H264,
    FILE_TYPE_HEVC,
    FILE_TYPE_JPG,
    FILE_TYPE_UNKNOWN
};

class ModuleFileReader : public ModuleMedia
{
private:
    FILE* fp;
    char* filepath;
    FILE_TYPE filetype;
    size_t fileSize;
    size_t seek;
    size_t sizePerRead;
    bool eof;
    MP4DVideo* mp4video;
    MKVDVideo* mkvvideo;

    char* mp4CacheBuffer;
    bool loopMode;

    int H2645ReadFrame(char* in_buf, int in_buf_size);
    int hevc_probe(const char* buf, int buf_size);

protected:
    virtual ProcessResult doProcess(MediaBuffer* output_buffer) override;

public:
    ModuleFileReader(const char* path, bool loop_play);
    ~ModuleFileReader();
    int init();
    const uint8_t* audioExtraData();
    unsigned audioExtraDataSize();
};

#endif
