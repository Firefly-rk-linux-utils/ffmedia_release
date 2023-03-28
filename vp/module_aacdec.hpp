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
    AlsaPlayBack* aplay;
    string a_dev;

public:
    ModuleAacDec(const uint8_t* _extradata, unsigned _extradata_size,
                 int _sample_rate, int _nb_channels = -1);
    ~ModuleAacDec();
    int init();

    void setAlsaDevice(string dev) { a_dev = dev; }

protected:
    virtual EnQueueResult doEnQueue(MediaBuffer* input_buffer, MediaBuffer* output_buffer) override;
    virtual ProcessResult doProcess(MediaBuffer* buffer) override;

private:
    void close();
};

#endif
