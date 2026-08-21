#pragma once

#include "module/module_media.hpp"
#include "buffer/infer_buffer.hpp"

namespace FFMedia
{

enum class ModuleTrackFor {
    NORMAL = 1,
    FACE = 2,
    POSE = 3
};

class ModuleTrack : public ModuleMedia
{
public:
    ModuleTrack(const std::string& module_name, ModuleTrackFor track_for = ModuleTrackFor::NORMAL);
    ~ModuleTrack();

    virtual int init() override;

protected:
    virtual int initBuffer() override;
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;
    virtual void track(std::shared_ptr<InferBuffer>& output_buffer) = 0;

protected:
    ModuleTrackFor track_for;
};


}  // namespace FFMedia
