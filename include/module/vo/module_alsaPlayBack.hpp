#pragma once
#include "module/module_media.hpp"

class AlsaPlayBack;
class ModuleAlsaPlayBack : public ModuleMedia
{
public:
    ModuleAlsaPlayBack(const std::string& dev);
    ModuleAlsaPlayBack(const std::string& dev, const SampleInfo& sample_info, AI_LAYOUT_E layout = AI_LAYOUT_NORMAL);
    ~ModuleAlsaPlayBack();
    int changeDevice(const std::string& dev, const SampleInfo& sample_info, AI_LAYOUT_E layout = AI_LAYOUT_NORMAL);
    int init() override;

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;

private:
    AlsaPlayBack* playBack;
    std::string device;
    SampleInfo sampleInfo;
    AI_LAYOUT_E layout;
};