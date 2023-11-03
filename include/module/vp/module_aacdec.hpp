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
    int nb_channels;
    int sample_rate;
    int samples;
    SampleFormat fmt;

    bool first_frame;
    shared_ptr<AlsaPlayBack> aplay;
    string a_dev;
    bool initialize;

public:
    ModuleAacDec();
    ModuleAacDec(const uint8_t* _extradata, unsigned _extradata_size,
                 int _sample_rate, int _nb_channels = -1);
    ~ModuleAacDec();
    void setAlsaDevice(string dev) { a_dev = dev; }

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;

private:
    int open();
    void close();
};

#endif
