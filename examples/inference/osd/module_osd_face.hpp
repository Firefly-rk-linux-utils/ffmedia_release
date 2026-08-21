#pragma once

#include "module_osd.hpp"

namespace FFMedia
{

class ModuleOsdFace : public ModuleOsd
{
public:
    ModuleOsdFace(const std::string& name);
    ~ModuleOsdFace();

protected:
    virtual void osd(std::shared_ptr<InferBuffer>& output_buffer) override;
};

}  // namespace FFMedia