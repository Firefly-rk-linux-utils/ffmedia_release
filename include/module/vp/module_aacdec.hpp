#ifndef __MODULE__AACDEC_HPP__
#define __MODULE__AACDEC_HPP__

#include "module/module_media.hpp"
#include "base/ff_type.hpp"

class AlsaPlayBack;
struct AAC_DECODER_INSTANCE;

class ModuleAacDec : public ModuleMedia
{
    AAC_DECODER_INSTANCE* dec;
    uint8_t* extradata;
    unsigned extradata_size;
    SampleInfo sampleInfo;

public:
    ModuleAacDec();
    ModuleAacDec(const uint8_t* _extradata, unsigned _extradata_size,
                 int _sample_rate, int _nb_channels = -1);
    ~ModuleAacDec();

    int changeSampleInfo(const SampleInfo& sample_info);
    int init() override;

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool teardown() override;

private:
    int open();
    void close();
};

#endif
