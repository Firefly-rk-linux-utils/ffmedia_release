# ModuleMedia 派生模块开发指南

`ModuleMedia` 是 FFMedia 的公共基类，负责管线连接、工作线程、输入队列和输出缓冲区。
模块开发者只需要配置媒体信息，并实现初始化、处理和资源启停接口。

本文只介绍开发 `ModuleMedia` 派生类需要了解的基本流程和 public/protected 扩展接口。

## 1. 基本数据流

FFMedia 管线通常由三类节点组成：

```text
源模块（SRC）          处理模块（PRC）          输出模块（PRC）
Camera/File/RTSP  ->  Decoder/RGA/Encoder  ->  Display/Mux/Server
     doProduce()          doConsume()              doConsume()
```

- 源模块没有上游，通过 `doProduce()` 产生 `MediaBuffer`。
- 处理模块通过 `doConsume()` 接收输入，可同时产生新的输出。
- 输出模块消费输入，通常不再连接下游。
- 模块之间使用 `connectProducer()` 连接，媒体数据由 `std::shared_ptr<MediaBuffer>` 传递。

一条管线的基本生命周期为：

```text
创建并配置模块
      |
初始化生产者，发布输出通道
      |
连接下游模块
      |
初始化下游模块
      |
从源模块 start()
      |
工作线程执行 setup() -> doProduce()/doConsume()
      |
从源模块 stop()
      |
工作线程执行 teardown()
```

管线应保持为无环结构。对源模块调用 `start()` 和 `stop()` 时，框架会递归启停下游模块。

## 2. 派生类需要实现什么

常用扩展接口如下：

| 接口 | 用途 |
| --- | --- |
| 构造函数 | 设置模块类型、媒体类型、缓冲区和通道要求。 |
| `init()` | 校验配置、初始化长期资源、发布通道并创建输出缓冲池。 |
| `setup()` | 每次工作线程启动前开启设备、流或会话。 |
| `doProduce()` | 源模块产生一份输出数据。 |
| `doConsume()` | 处理或输出模块消费一份输入数据。 |
| `teardown()` | 工作线程退出时停止流并释放本轮运行资源。 |
| 析构函数 | 在释放派生类资源前调用 `stop()`。 |

源模块在构造函数中设置：

```cpp
setModuleType(ModuleType::SRC);
```

处理模块默认是 `ModuleType::PRC`，通常不需要再次设置。

模块应由 `std::shared_ptr` 管理，推荐使用：

```cpp
auto module = makeMediaModule<MyModule>(constructor_args...);
```

## 3. 最小源模块示例

下面示例从自定义数据源读取数据，并发布到通道 `0`：

```cpp
#include <ffmedia/ffmedia.hpp>

#include <cerrno>

using namespace FFMedia;

class MySource final : public ModuleMedia
{
public:
    MySource()
        : ModuleMedia("MySource")
    {
        setModuleType(ModuleType::SRC);
        setMediaType(BUFFER_TYPE_ETC);
        setBufferCount(4);
        setBufferSize(4096);
    }

    ~MySource() override
    {
        stop();
    }

    int init() override
    {
        if (isInitialized())
            return 0;

        MediaChannelInfo output;
        output.id = 0;
        output.name = "data";
        output.media_type = BUFFER_TYPE_ETC;
        setOutputMediaChannels({output});

        const int ret = initBuffer();
        if (ret < 0)
            return ret;

        setInitialized();
        return 0;
    }

protected:
    bool setup() override
    {
        return openSource() == 0;
    }

    ProduceResult doProduce(
        std::shared_ptr<MediaBuffer>& output) override
    {
        size_t bytes = 0;
        const int ret = readSource(
            output->getData(), output->getSize(), bytes);
        if (ret == -EAGAIN)
            return PRODUCE_EMPTY;
        if (ret < 0)
            return PRODUCE_FAILED;
        if (bytes == 0)
            return PRODUCE_EOS;

        output->setActiveSize(bytes);
        output->setMediaChannelId(0);
        return PRODUCE_SUCCESS;
    }

    bool teardown() override
    {
        closeSource();
        return true;
    }

private:
    int openSource();
    int readSource(void* data, size_t capacity, size_t& bytes);
    void closeSource();
};
```

需要从设备查询格式时，可在 `init()` 中打开设备并发布实际的输出通道；`setup()` 和
`teardown()` 只负责每轮运行所需的开始与停止操作。

## 4. 最小处理模块示例

处理模块先声明可接受的输入类型，再在 `doConsume()` 中处理输入并填写输出 Buffer：

```cpp
#include <ffmedia/ffmedia.hpp>

#include <cerrno>
#include <cstring>

using namespace FFMedia;

class CopyProcessor final : public ModuleMedia
{
public:
    CopyProcessor()
        : ModuleMedia("CopyProcessor")
    {
        setMediaType(BUFFER_TYPE_ETC);
        setBufferCount(4);
        setBufferSize(4096);

        MediaChannelRequirement input;
        input.input_id = 0;
        input.name = "input";
        input.media_type = BUFFER_TYPE_ETC;
        setInputMediaChannelRequirements({input});
    }

    ~CopyProcessor() override
    {
        stop();
    }

    int init() override
    {
        if (isInitialized())
            return 0;
        if (getInputMediaChannels().empty())
            return -ENOTCONN;

        MediaChannelInfo output;
        output.id = 0;
        output.name = "output";
        output.media_type = BUFFER_TYPE_ETC;
        setOutputMediaChannels({output});

        const int ret = initBuffer();
        if (ret < 0)
            return ret;

        setInitialized();
        return 0;
    }

protected:
    ConsumeResult doConsume(
        const MediaBufferContext& input,
        std::shared_ptr<MediaBuffer>& output) override
    {
        if (!input.buffer)
            return CONSUME_FAILED;
        if (input.buffer->getEos())
            return CONSUME_EOS;

        const size_t bytes = input.buffer->getActiveSize();
        if (bytes > output->getSize())
            return CONSUME_FAILED;

        std::memcpy(output->getData(),
                    input.buffer->getActiveData(), bytes);
        output->setActiveSize(bytes);
        output->setPUstimestamp(input.buffer->getPUstimestamp());
        output->setMediaChannelId(0);
        return CONSUME_SUCCESS;
    }
};
```

`MediaBufferContext::input_id` 是当前输入对应的逻辑通道 ID。模块有多个输入时，应使用它区分
视频、音频或不同生产者，不要通过修改共享 Buffer 来表达路由信息。

## 5. Buffer 生命周期

`ModuleMedia` 使用固定数量的输出 Buffer 循环工作。一次正常的 Buffer 流转过程如下：

```text
initBuffer() / commitOutputBufferPool()
              |
        输出 Buffer 进入空闲池
              |
        工作线程预留一个 Buffer
              |
      doProduce() / doConsume() 填写数据
              |
         返回 SUCCESS，发布到下游
              |
    一个或多个消费者持有同一份 Buffer
              |
  最后一份下游 shared_ptr 引用释放
              |
       执行可选 recycle_handler
              |
         Buffer 返回空闲池复用
```

输出 Buffer 通常不会每帧重新申请和销毁，而是在池中重复使用。派生模块只负责填写有效数据、
时间戳、格式和通道 ID，不需要手动把普通 Buffer 放回池中。

下游收到的 Buffer 有效期与该帧 `std::shared_ptr<MediaBuffer>` 的持有期一致。只要下游仍保留
任意一份该共享指针或它的副本，框架就不会复用对应 Buffer；最后一份下游共享指针释放后，
Buffer 可以立即返回池中，原有数据和元信息随后可能被下一帧覆盖。需要在回调或
`doConsume()` 返回后继续使用时，应复制并保存收到的共享指针。

需要注意：

- `PRODUCE_SUCCESS` 或 `CONSUME_SUCCESS` 表示当前输出可以发布。
- `PRODUCE_EMPTY`、`CONSUME_SKIP` 或失败结果不会发布当前输出。
- 下游可以共享同一份数据，复制共享指针即可延长本帧 Buffer 的有效期。
- 回调或消费者需要异步使用数据时，必须保留收到的 `std::shared_ptr<MediaBuffer>`。
- 只保存 `getData()`、裸指针或 DMA-BUF fd，不能延长本帧 Buffer 的使用时间。
- 摄像头、编码器等设备 Buffer 可使用 `recycle_handler` 在回收时重新入队或归还设备。

## 6. 私有 Buffer 初始化示例

普通内存、视频或音频模块优先调用基类 `initBuffer()`。派生模块需要创建自定义类型、DRM、
DMA 或设备私有 Buffer 时，可覆盖 `initBuffer()` 并提交自己的输出池：

```cpp
#include <ffmedia/ffmedia.hpp>

#include <cerrno>
#include <new>
#include <utility>

using namespace FFMedia;

class PrivateBufferModule : public ModuleMedia
{
protected:
    int initBuffer() override
    {
        const uint16_t count = getBufferCount();
        const ImagePara para = getOutputImagePara();
        if (count == 0 || para.width == 0 || para.height == 0)
            return -EINVAL;

        OutputBufferPool pool;
        try {
            pool.buffers.reserve(count);
            pool.rotation_buffers.reserve(count);

            for (uint16_t i = 0; i < count; ++i) {
                auto buffer = std::make_shared<VideoBuffer>(
                    VideoBuffer::DRM_BUFFER_NONCACHEABLE);
                buffer->allocBuffer(para);
                if (buffer->getSize() == 0)
                    return -ENOMEM;

                buffer->setIndex(i);
                buffer->setMediaBufferType(BUFFER_TYPE_VIDEO);
                buffer->setImagePara(para);

                pool.buffers.push_back(buffer);
                pool.rotation_buffers.push_back(std::move(buffer));
            }
        } catch (const std::bad_alloc&) {
            return -ENOMEM;
        }

        return commitOutputBufferPool(std::move(pool));
    }
};
```

派生类在 `init()` 中设置好 `ImagePara`、Buffer 数量等参数后，像普通模块一样调用
`initBuffer()`，此时会进入上面的覆盖实现。

- `pool.buffers` 保存模块实际拥有的私有 Buffer。
- `pool.rotation_buffers` 是工作线程循环取得的 Buffer；大多数模块与 `buffers` 使用同一批对象。
- 两个列表必须数量一致、元素非空，同一列表中不能重复提交同一对象。
- 应先在局部完成全部申请和设备导入，确认成功后再调用 `commitOutputBufferPool()`。
- 提交失败时直接返回错误，不要把未完成初始化的 Buffer 发布给工作线程。

如果 Buffer 释放后必须归还设备，可在提交前设置回收函数：

```cpp
const auto device = device_;
pool.recycle_handler =
    [device](const std::shared_ptr<MediaBuffer>& buffer) {
        if (device && buffer)
            device->queueBuffer(buffer->getIndex());
    };
```

回收函数应捕获能够独立保持生命周期的设备对象，不要依赖可能已经析构的派生类成员。
设备重新入队、解除映射或归还硬件句柄的具体操作由派生模块自行实现。

## 7. 连接和运行模块

生产者应先初始化并发布输出通道，下游连接成功后再初始化：

```cpp
auto source = makeMediaModule<MySource>();
auto processor = makeMediaModule<CopyProcessor>();

int ret = source->init();
if (ret < 0)
    return ret;

ret = processor->connectProducer(source);
if (ret < 0)
    return ret;

ret = processor->init();
if (ret < 0)
    return ret;

source->start();

// 等待业务结束或退出信号。

source->stop();
```

继续添加下游节点时，重复“连接当前生产者，再初始化消费者”的过程即可。

## 8. 返回值的基本含义

派生模块通常只需要使用以下结果：

| 返回值 | 含义 |
| --- | --- |
| `PRODUCE_SUCCESS` | 已生成有效输出。 |
| `PRODUCE_EMPTY` | 本次暂时没有数据，稍后继续。 |
| `PRODUCE_EOS` | 输入源结束。 |
| `PRODUCE_FAILED` | 生产失败。 |
| `CONSUME_SUCCESS` | 输入处理成功；存在输出 Buffer 时发布输出。 |
| `CONSUME_SKIP` | 输入已处理，但本次不产生输出。 |
| `CONSUME_EOS` | 输入到达结束。 |
| `CONSUME_FAILED` | 消费或处理失败。 |

## 9. 视频和音频模块

视频或音频模块仍遵循相同流程，只需补充对应媒体参数：

- 视频模块设置 `ImagePara`、像素格式、编码类型和 `BUFFER_TYPE_VIDEO`。
- 音频模块设置 `SampleInfo`、编码类型和 `BUFFER_TYPE_AUDIO`。
- 生产者通过 `setOutputMediaChannels()` 发布实际输出。
- 消费者通过 `setInputMediaChannelRequirements()` 声明可接受的类型和格式。
- 消费者可在 `init()` 中通过 `getInputMediaChannels()` 取得连接后的实际输入参数。

简单的单路视频模块也可使用 `setInputImagePara()` 和 `setOutputImagePara()`。

## 10. 开发注意事项

- 在 `start()` 前完成参数配置、通道连接和 `init()`。
- 不要覆盖 `start()`、`stop()` 或 `receiveMediaBuffer()`；处理逻辑放在受保护的虚函数中。
- `setup()` 可能被多次调用，`teardown()` 必须与其资源操作对应，以支持重复启停。
- `doProduce()` 和 `doConsume()` 运行在模块工作线程中，不要无限阻塞。
- 输出数据必须设置有效长度；多通道模块还应设置正确的输出通道 ID。
- 异步保存 Buffer 时应保留收到的 `std::shared_ptr<MediaBuffer>`，不要只保存裸指针或 fd。
- 正常退出时从源节点显式调用 `stop()`，并确保派生资源在工作线程停止后再释放。
- 普通模块优先使用 `initBuffer()` 创建输出池；只有硬件导入等特殊场景才需要自定义缓冲池。

完整参数接口、状态查询和其他公共 API 参见 [FFMedia API](ffmedia_api.md)。
