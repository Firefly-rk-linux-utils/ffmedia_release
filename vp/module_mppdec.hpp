#ifndef __MODULE__MPPDEC_HPP__
#define __MODULE__MPPDEC_HPP__

#include "module/module_media.hpp"
#include "base/ff_type.hpp"

class MppDecoder;

class ModuleMppDec : public ModuleMedia
{
private:
    MppDecoder* dec;
    DecodeType decode_type;

protected:
    void setAlign(DecodeType decode_type);
    virtual EnQueueResult doEnQueue(MediaBuffer* input_buffer, MediaBuffer* output_buffer) override;
    virtual ProcessResult doProcess(MediaBuffer* buffer) override;
    virtual int initBuffer() override;
    virtual void bufferReleaseCallBack(MediaBuffer* buffer) override;

public:
    ModuleMppDec(ImagePara& input_para);
    ModuleMppDec(ImagePara& input_para, DecodeType type);
    ~ModuleMppDec();
    int init();
};

#endif
