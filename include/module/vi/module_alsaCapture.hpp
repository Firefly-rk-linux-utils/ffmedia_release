#pragma once
#include "module/module_media.hpp"

class AlsaCapture;
class ModuleAlsaCapture : public ModuleMedia
{
public:
    ModuleAlsaCapture(const std::string& dev);
    ModuleAlsaCapture(const std::string& dev, const SampleInfo& sample_info, AI_LAYOUT_E layout = AI_LAYOUT_NORMAL);
    ~ModuleAlsaCapture();
    int changeSource(const std::string& dev, const SampleInfo& sample_info, AI_LAYOUT_E layout = AI_LAYOUT_NORMAL);
    int init() override;

protected:
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;

private:
    AlsaCapture* capture;
    std::string device;
    SampleInfo sampleInfo;
    AI_LAYOUT_E layout;
};