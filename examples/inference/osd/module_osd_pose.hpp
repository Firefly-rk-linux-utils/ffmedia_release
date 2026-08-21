#pragma once

#include "module_osd.hpp"

namespace FFMedia
{

class ModuleOsdPose : public ModuleOsd
{
public:
    ModuleOsdPose(const std::string& name);
    ~ModuleOsdPose();

protected:
    virtual void osd(std::shared_ptr<InferBuffer>& output_buffer) override;
};

}  // namespace FFMedia