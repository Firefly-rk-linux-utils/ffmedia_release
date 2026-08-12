/*
 * GPU image processing based on EGL/OpenGL ES.
 *
 * DMA-BUF input and output stay zero-copy. AFBC compression/decompression is
 * selected through ImagePara::compression without an external processing SDK.
 */
#pragma once

#include <memory>

#include "module/module_media.hpp"

namespace FFMedia
{

class FFMEDIA_API ModuleImageProcessor : public ModuleMedia
{
public:
    ModuleImageProcessor();
    explicit ModuleImageProcessor(const ImagePara& output_para);
    ModuleImageProcessor(const ImagePara& input_para,
                         const ImagePara& output_para);
    ~ModuleImageProcessor() override;

    ModuleImageProcessor(const ModuleImageProcessor&) = delete;
    ModuleImageProcessor& operator=(const ModuleImageProcessor&) = delete;

    int init() override;

protected:
    ConsumeResult doConsume(const std::shared_ptr<MediaBuffer>& input_buffer,
                            std::shared_ptr<MediaBuffer>& output_buffer);
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;
    bool setup() override;
    bool teardown() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace FFMedia
