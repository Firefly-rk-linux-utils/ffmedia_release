#ifndef __MODULE_MP4ENM_HPP__
#define __MODULE_MP4ENM_HPP__

#include "module/module_media.hpp"

struct MP4E_mux_tag;
struct mp4_h26x_writer_tag;

class ModuleMp4Enm : public ModuleMedia
{
private:
    bool sequential_mode;
    bool fragmentation_mode;
    int fps;
    string filename;
    FILE* fout;
    struct MP4E_mux_tag* mux;
    struct mp4_h26x_writer_tag* mp4wr;
    int64_t previousFrameTimestamp;
    uint64_t file_maxsize;
    uint32_t file_number;

    void Mp4EnmClose();
    int Mp4EnmInit();

protected:
    virtual EnQueueResult doEnQueue(MediaBuffer* input_buffer, MediaBuffer* output_buffer) override;

public:
    ModuleMp4Enm(ImagePara& para, bool sequential_mode, bool fragmentation_mode, int fps,
                 const char* _filename);
    ~ModuleMp4Enm();
    void setFileMaxSize(uint64_t size);
    int init();
};

#endif
