#ifndef __MODULE__MPPDEC_HPP__
#define __MODULE__MPPDEC_HPP__

#include "module/module_media.hpp"
#include "base/ff_type.hpp"

class MppDecoder;

class ModuleMppDec : public ModuleMedia
{
private:
    shared_ptr<MppDecoder> dec;
    DecodeType decode_type;

protected:
    void setAlign(DecodeType decode_type);
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> buffer) override;
    virtual int initBuffer() override;
    virtual void bufferReleaseCallBack(shared_ptr<MediaBuffer> buffer) override;
    void reset() override;

public:
    ModuleMppDec();
    ModuleMppDec(const ImagePara& input_para);
    ModuleMppDec(const ImagePara& input_para, DecodeType type);
    ~ModuleMppDec();
    void setNeedSplit(uint32_t need_split);
    int init() override;
};

#endif
