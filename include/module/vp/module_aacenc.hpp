#ifndef __MODULE__AACENC_HPP__
#define __MODULE__AACENC_HPP__

#include "module/module_media.hpp"
#include "base/ff_type.hpp"

struct AACENCODER;

class ModuleAacEnc : public ModuleMedia
{
    AACENCODER* enc;
    SampleInfo sample_info;
    int aot;
    int bit_rate;
    int afterburner;
    int eld_sbr;
    int vbr;

public:
    /*
     * SampleFormat:
     *	SAMPLE_FMT_S16, SAMPLE_FMT_NONE
     * _sample_rate:
     *	96000, 88200, 64000, 48000, 44100, 32000,
     *	24000, 22050, 16000, 12000, 11025, 8000, 0
     * _nb_channels:
     *	1 ~ 8
     */
    ModuleAacEnc(const SampleInfo& sample_info);
    ~ModuleAacEnc();
    int init() override;

    // aot == 2;  "LC"
    // aot == 5;  "HE-AAC"
    // aot == 29; "HE-AACv2"
    // aot == 23; "LD"
    // aot == 39; "ELD"
    void setAot(int _aot) { aot = _aot; }
    int getAot() { return aot; }
    void setBitrate(int bitrate) { bit_rate = bitrate; }
    int getBitrate() { return bit_rate; }
    void setAfterburner(int _afterburner) { afterburner = _afterburner; }
    int getAfterburner() { return afterburner; }
    void setEldSbr(int _eld_sbr) { eld_sbr = _eld_sbr; }
    int getEldSbr() { return eld_sbr; }
    void setVbr(int _vbr) { vbr = _vbr; }
    int gerVbr() { return vbr; }

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;

private:
    void close();
};

#endif
