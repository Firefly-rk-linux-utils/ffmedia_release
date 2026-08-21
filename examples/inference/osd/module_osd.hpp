#pragma once

#include "module/module_media.hpp"
#include "module/vp/module_rga.hpp"
#include "buffer/infer_buffer.hpp"

namespace FFMedia
{

class ModuleOsd : public ModuleMedia
{
public:
    ModuleOsd(const std::string& name);
    ~ModuleOsd();

    virtual int init();

protected:
    virtual int initBuffer() override;
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;
    virtual void osd(std::shared_ptr<InferBuffer>& output_buffer);

private:
    std::shared_ptr<ModuleRga> _converter;
};

}  // namespace FFMedia
