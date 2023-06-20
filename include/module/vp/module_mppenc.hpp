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
    int64_t cur_pts;
    int64_t duration;
    shared_ptr<VideoBuffer> encoderExtraData(shared_ptr<VideoBuffer> buffer);

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> buffer) override;
    virtual int initBuffer() override;
    virtual void bufferReleaseCallBack(shared_ptr<MediaBuffer> buffer) override;

public:
    ModuleMppEnc(EncodeType type, const ImagePara& input_para);
    ModuleMppEnc(EncodeType type, const ImagePara& input_para, int fps, int gop, int bps, EncodeRcMode mode, EncodeQuality quality,
                 EncodeProfile profile);
    ~ModuleMppEnc();
    int init();
};
#endif
