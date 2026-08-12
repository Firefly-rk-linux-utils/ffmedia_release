#include <cassert>
#include <cstdint>
#include <memory>

#include <ffmedia/module/module_app.hpp>
#include <ffmedia/module/module_media.hpp>

class InstalledSource final : public FFMedia::ModuleMedia
{
public:
    InstalledSource()
        : ModuleMedia("InstalledSource")
    {
        setModuleType(FFMedia::ModuleType::SRC);
        setMediaType(FFMedia::BUFFER_TYPE_ETC);
        setBufferCount(1);
        setBufferSize(8);
    }

    ~InstalledSource() override
    {
        assert(!isWorkerActive());
    }

    int init() override
    {
        return initBuffer();
    }

protected:
    ProduceResult doProduce(
        std::shared_ptr<FFMedia::MediaBuffer>& buffer) override
    {
        if (produced_)
            return PRODUCE_EOS;
        produced_ = true;
        buffer->setActiveSize(1);
        return PRODUCE_SUCCESS;
    }

private:
    bool produced_ = false;
};

class InstalledProcessor final : public FFMedia::ModuleMedia
{
public:
    InstalledProcessor()
        : ModuleMedia("InstalledProcessor")
    {
        setModuleType(FFMedia::ModuleType::PRC);
        setMediaType(FFMedia::BUFFER_TYPE_ETC);
    }

    int init() override
    {
        setInitialized();
        return 0;
    }

protected:
    ConsumeResult doConsume(
        const FFMedia::MediaBufferContext& input,
        std::shared_ptr<FFMedia::MediaBuffer>&) override
    {
        return input.buffer ? CONSUME_SKIP : CONSUME_FAILED;
    }
};

int main()
{
    static_assert(FFMEDIA_MODULE_ABI_VERSION == 1,
                  "Unexpected FFMedia module ABI");
#if defined(__aarch64__)
    static_assert(sizeof(FFMedia::ModuleMedia) == 72,
                  "Installed ModuleMedia layout mismatch");
#endif
    assert(ffmedia_module_abi_version() == FFMEDIA_MODULE_ABI_VERSION);
    assert(ffmedia_glibcxx_use_cxx11_abi() == FFMEDIA_GLIBCXX_USE_CXX11_ABI);

    const auto dropped = FFMedia::AppProcessResult::drop();
    assert(dropped.action == FFMedia::AppProcessAction::DROP);

    auto source = FFMedia::makeMediaModule<InstalledSource>();
    assert(source->init() == 0);
    source->start();
    source.reset();

    auto processor = FFMedia::makeMediaModule<InstalledProcessor>();
    assert(processor->init() == 0);
    processor.reset();
    return 0;
}
