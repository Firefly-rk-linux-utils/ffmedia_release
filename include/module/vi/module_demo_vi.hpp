#ifndef __MODULE_DEMO_VI_HPP__
#define __MODULE_DEMO_VI_HPP__

#include "module/module_media.hpp"

class ModuleDemoVi : public ModuleMedia
{
public:
    ModuleDemoVi(string _str);
    ~ModuleDemoVi();
    int init();

protected:
    virtual ProduceResult doProduce(shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;

private:
    string str;
};

#endif /* module_demo_vi_hpp */