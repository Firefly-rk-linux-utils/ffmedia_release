#ifndef __MODULE_MPPENC_HPP__
#define __MODULE_MPPENC_HPP__

#include "module/module_media.hpp"
#include "base/ff_type.hpp"

class MppEncoder;
class VideoBuffer;

class ModuleMppEnc : public ModuleMedia
{
private:
    EncodeType encode_type;
    MppEncoder* enc;
    int fps;
    int gop;
    int bps;
    EncodeRcMode mode;
    EncodeQuality quality;
    EncodeProfile profile;
    VideoBuffer* encoderExtraData(VideoBuffer* buffer);

protected:
    virtual EnQueueResult doEnQueue(MediaBuffer* input_buffer, MediaBuffer* output_buffer) override;
    virtual ProcessResult doProcess(MediaBuffer* buffer) override;
    virtual int initBuffer() override;
    virtual void bufferReleaseCallBack(MediaBuffer* buffer) override;

public:
    ModuleMppEnc(EncodeType type, ImagePara& input_para);
    ModuleMppEnc(EncodeType type, ImagePara& input_para, int fps, int gop, int bps, EncodeRcMode mode, EncodeQuality quality,
                 EncodeProfile profile);
    ~ModuleMppEnc();
    int init();
};
#endif
