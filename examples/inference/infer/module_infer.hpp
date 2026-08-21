#pragma once

#include "module/module_media.hpp"
#include "buffer/infer_buffer.hpp"

namespace FFMedia
{

class ModuleInfer : public ModuleMedia
{
public:
    ModuleInfer(const std::string& name);
    virtual ~ModuleInfer();

    virtual int init() override;

protected:
    virtual int initBuffer() override;
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;

    virtual int initModel() = 0;
    virtual int inferCombinations(const std::shared_ptr<MediaBuffer>& input_buffer) = 0;

private:
    /* data */
};

}  // namespace FFMedia
