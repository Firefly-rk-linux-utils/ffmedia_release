#include "moudule/vi/module_demo_vi.hpp"


ModuleDemoVi::ModuleDemoVi(string _str)
    : str(_str)
{
    name = __func__;
    buffer_count = 10;  // buffer pool count
}

ModuleDemoVi::~ModuleDemoVi()
{
}

int ModuleDemoVi::init()
{
    ff_info_m("%s\n", str.c_str());
    output_para.width = 1920;
    output_para.height = 1080;
    output_para.hstride = 1920;
    output_para.vstride = 1080;
    output_para.v4l2Fmt = V4L2_PIX_FMT_H264;
    return initBuffer();  // init buffer poll
}

//
bool ModuleDemoVi::setup()
{
    ff_info_m("%s\n", str.c_str());
    // traverse the buffer pool
    for (auto b : buffer_pool)
        ff_info_m("buffer %p size %ld\n", b->getData(), b->getSize());
    return true;
}

bool ModuleDemoVi::teardown()
{
    ff_info_m("%s\n", str.c_str());
    return true;
}

ModuleMedia::ProduceResult ModuleDemoVi::doProduce(shared_ptr<MediaBuffer> output_buffer)
{
    void* buf = output_buffer->getData();
    size_t size = output_buffer->getSize();
    ff_info_m("%s\n", str.c_str());
    if (size > str.length()) {
        memcpy(buf, str.c_str(), str.length());
        output_buffer->setActiveData(buf);
        output_buffer->setActiveSize(str.length());
    } else {
        output_buffer->setActiveSize(0);
        return PRODUCE_EMPTY;
    }

    usleep(5 * 100 * 1000);
    // setOutputBufferQueueHead(buffer); //Manually set the buffer to the consumer queue
    return PRODUCE_SUCCESS;
}
