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
    shared_ptr<MppEncoder> enc;
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
    void chooseOutputParaFmt();
    void reset() override;

public:
    ModuleMppEnc(EncodeType type, int fps = 30, int gop = 60, int bps = 2048,
                 EncodeRcMode mode = ENCODE_RC_MODE_CBR, EncodeQuality quality = ENCODE_QUALITY_BEST,
                 EncodeProfile profile = ENCODE_PROFILE_HIGH);
    ModuleMppEnc(EncodeType type, const ImagePara& input_para, int fps = 30, int gop = 60, int bps = 2048,
                 EncodeRcMode mode = ENCODE_RC_MODE_CBR, EncodeQuality quality = ENCODE_QUALITY_BEST,
                 EncodeProfile profile = ENCODE_PROFILE_HIGH);
    ~ModuleMppEnc();
    void setDuration(int64_t _duration);
    int changeEncodeParameter(EncodeType type, int fps = 30, int gop = 60, int bps = 2048,
                              EncodeRcMode mode = ENCODE_RC_MODE_CBR, EncodeQuality quality = ENCODE_QUALITY_BEST,
                              EncodeProfile profile = ENCODE_PROFILE_HIGH);
    int init() override;
};
#endif
