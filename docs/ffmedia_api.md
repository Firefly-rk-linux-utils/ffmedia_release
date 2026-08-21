# FFMedia API 介绍

本文档按已发布 SDK 中的模块职责整理公开 API。所有类均位于 `FFMedia` 命名空间，除特别
说明外，模块都继承自 `ModuleMedia`，通过 `MediaBuffer` 在管线中传递数据。稳定公开面以
SDK `include/ffmedia/` 实际安装的头文件为准；源码树中未安装的头、私有成员和动态库中偶然
可见但未声明的符号不属于兼容承诺。

## SDK 接入与兼容性

推荐通过 CMake imported target 使用 SDK：

```cmake
find_package(FFMedia REQUIRED CONFIG COMPONENTS core)
target_link_libraries(my_app PRIVATE FFMedia::FFMedia)
```

常用模块可统一包含：

```cpp
#include <ffmedia/ffmedia.hpp>
```

`FFMedia::ff_media` 以及 `<module/...>`、`<base/...>` include 形式作为兼容入口继续保留。

编译和部署时必须同时匹配 `FFMEDIA_MODULE_ABI_VERSION`、运行期
`ffmedia_module_abi_version()`、`libff_media.so.<abi>` SONAME，以及
`FFMEDIA_GLIBCXX_USE_CXX11_ABI`。不要混用不同 SDK 包中的头文件、CMake 元数据和动态库。

## 目录结构

- `include/ffmedia/module/`：模块公共基类、生产/消费接口、多媒体通道描述、状态和回调。
- `include/ffmedia/base/`：媒体基础数据结构和通用类型，例如 `MediaBuffer`、`VideoBuffer`、`ImagePara`、`SampleInfo`。
- `include/ffmedia/module/vi/`：输入源模块，负责采集、读取或拉取媒体数据。
- `include/ffmedia/module/vp/`：处理模块，负责编解码、图像处理、推理等。
- `include/ffmedia/module/vo/`：输出模块，负责播放、显示、封装写入或网络推流。

## 公共管线接口

### 多媒体通道模型

头文件：`module/media_channel.hpp`

多媒体通道接口用于描述一个模块同时输出的多路视频、音频或其他媒体数据，并在连接模块时完成通道选择、格式匹配和消费者输入参数配置。

#### MediaChannelInfo

生产者的一路输出通道描述：

| 字段 | 说明 |
| --- | --- |
| `id` | 生产者内部唯一的输出通道 ID；默认值为 `MEDIA_CHANNEL_ID_DEFAULT(0)`。 |
| `name` | 可读通道名称，例如 `video`、`audio`、`main-video`。 |
| `media_type` | 媒体类型：`BUFFER_TYPE_VIDEO`、`BUFFER_TYPE_AUDIO` 或 `BUFFER_TYPE_ETC`。 |
| `codec` | 编码格式，例如 H264、H265、VP8、VP9、AAC 或 RAW。 |
| `image_para` | 视频通道的图像尺寸、stride 和像素格式。 |
| `sample_info` | 音频通道的采样格式、通道数、采样率和单帧采样数。 |
| `extra_data` | SPS/PPS、AAC AudioSpecificConfig 等初始化附加数据。 |

#### MediaChannelRequirement

消费者一个逻辑输入允许的媒体格式：

| 字段 | 说明 |
| --- | --- |
| `input_id` | 消费者逻辑输入 ID。消费时通过 `MediaBufferContext::input_id` 获取。 |
| `name` | 可读输入名称。 |
| `media_type` | 要求的媒体类型；`BUFFER_TYPE_ETC` 表示不限制。 |
| `codecs` | 允许的编码格式列表；空列表表示不限制。 |
| `pixel_formats` | 允许的视频像素格式列表；空列表表示不限制。 |
| `sample_formats` | 允许的音频采样格式列表；空列表表示不限制。 |
| `allow_multiple` | 是否允许多个生产者输出通道匹配到该逻辑输入。 |

`matches(const MediaChannelInfo&)` 会依次检查媒体类型、编码格式、像素格式和采样格式。

#### 连接、匹配和输入配置

- `MediaChannelSelection::output_ids` 指定本次连接可选择的生产者输出通道；列表为空表示选择生产者全部输出通道。
- `connectProducer()` 会将选择结果与消费者的 `MediaChannelRequirement` 进行匹配。
- `allow_multiple=false` 的逻辑输入只能连接一个生产者；再次连接其他生产者时返回 `-EBUSY`。同一生产者可重新连接以更新路由。
- 消费者没有声明输入要求时，默认连接所选的全部生产者输出通道。
- 消费者声明了输入要求但没有任何通道匹配时返回 `-ENOTSUP`。
- 选择了生产者不存在的通道 ID 时返回 `-ENOENT`。
- 连接成功后生成 `MediaInputChannel`，其中记录消费者输入 ID、生产者输出通道 ID、生产者对象、生产者名称及完整媒体参数。
- 视频输入匹配成功且兼容字段 `input_para` 尚未完整配置时，会同步为首个匹配视频通道的 `ImagePara`；已手动设置的有效参数优先保留。

`MediaBufferContext` 包含本次消费的 `buffer` 和消费者逻辑 `input_id`。需要识别数据来源时，可使用 `input_id` 调用 `getInputMediaChannel()`，从返回的 `MediaInputChannel` 获取生产者和生产者输出通道 ID。

解码器输入要求示例：

```cpp
MediaChannelRequirement input;
input.input_id = 0;
input.name = "compressed-video";
input.media_type = BUFFER_TYPE_VIDEO;
input.codecs = {
    MEDIA_CODEC_VIDEO_VP8,
    MEDIA_CODEC_VIDEO_VP9,
    MEDIA_CODEC_VIDEO_H264,
    MEDIA_CODEC_VIDEO_H265,
};
setInputMediaChannelRequirements({input});
```

### MediaHookable

头文件：`module/ff_media_hookable.hpp`

提供媒体数据和状态变化钩子。

状态枚举 `MediaStatus`：

- `CREATED`：已创建。
- `STARTED`：运行中。
- `EOS`：流结束。
- `STOPPED`：已停止。
- `ABNORMAL`：异常。

| API | 说明 |
| --- | --- |
| `bool setMediaBufferConsumeHooker(MediaBufferHooker hooker)` | 在启动前设置输入缓冲消费前回调。 |
| `bool setMediaBufferProduceHooker(MediaBufferHooker hooker)` | 在启动前设置输出缓冲生产后回调。 |
| `bool setMediaStatusChangeHooker(MediaStatusHooker hooker)` | 在启动前设置状态变化回调。 |

`MediaBufferHooker` 形态为 `void(const std::string& name, int value, const std::shared_ptr<MediaBuffer>& buffer)`；
`MediaStatusHooker` 形态为 `void(const std::string& name, MediaStatus status)`。

第二个整数参数取决于回调位置：消费 Hook 和外部消费者回调传入当前输入队列长度；生产
Hook 传入当前输出 slot 索引。生产 Hook 调用时已经创建该帧 flight `shared_ptr`，随后才向
下游分发。Hook 在模块工作线程中同步执行，不适合执行长时间阻塞任务。

Hook 只能在模块 `start()` 前配置。设置成功返回 `true`；模块运行期间配置被冻结，设置接口
返回 `false` 并保留原 Hook。`stop()` 完整结束管线后会重新允许配置，可在下一次 `start()`
前替换或清除 Hook。运行时调用 Hook 不加锁，也不复制回调快照。

### ModuleMedia

头文件：`module/module_media.hpp`

模块统一基类。`ModuleMedia` 继承 `MediaParameter`，所有组件均可作为生产者或消费者接入管线。推荐流程为：初始化生产者并获得输出通道描述，构造消费者，调用 `connectProducer()` 完成通道匹配，再初始化消费者；整条管线配置完成后调用源模块 `start()`，结束时调用 `stop()`。

| API | 调用时机 | 说明 |
| --- | --- | --- |
| `init()` | 启动前 | 初始化模块资源，成功返回 `0`。 |
| `start()` / `stop()` | 初始化后 / 结束时 | 启停当前模块工作线程，并递归启停下游模块。 |
| `makeMediaModule<T>(args...)` | 构造时 | 推荐的外部派生模块工厂；删除对象前先调用 `stop()`，保证派生资源释放前工作线程已退出。 |
| `setProductor(std::shared_ptr<ModuleMedia> module)` | 初始化前 | 兼容接口，连接上游生产者的匹配通道；无法向调用方返回连接错误。新代码推荐使用 `connectProducer()`。 |
| `setProductor(std::shared_ptr<ModuleMedia> module, const MediaChannelSelection& selection)` | 初始化前 | 选择生产者输出通道并建立连接，返回连接结果。 |
| `connectProducer(std::shared_ptr<ModuleMedia> module, const MediaChannelSelection& selection = {})` | 初始化前 | 按消费者格式要求匹配并连接通道；空 selection 表示生产者全部输出通道。匹配到已被其他生产者占用的单路输入时返回 `-EBUSY`。 |
| `removeProductor(std::shared_ptr<ModuleMedia> module)` | 停止或重配置时 | 移除指定上游；传空指针表示移除所有上游。 |
| `setBufferCount(uint16_t count)` / `getBufferCount()` | 初始化前 | 设置/获取输出缓冲池数量。 |
| `setBufferSize(const size_t& size)` / `getBufferSize()` | 初始化前/任意查询 | 设置配置大小；查询已发布池的实际大小，池尚未发布或已失效时返回配置值。 |
| `getBufferFromIndex(uint16_t index)` | 初始化后 | 从原子池快照获取指定索引缓冲；并发重建不会使已返回的 `shared_ptr` 失效。 |
| `holdOutputBuffer(const std::shared_ptr<MediaBuffer>& obuf)` | 不建议使用 | Deprecated no-op，始终返回 `0`，不保存引用。 |
| `releaseOutputBuffer(const std::shared_ptr<MediaBuffer>& obuf)` | 不建议使用 | Deprecated no-op，始终返回 `0`，不释放引用。 |
| `setInputImagePara(const ImagePara& para)` / `getInputImagePara()` | 初始化前/查询 | 单视频输入兼容接口。连接成功后首个匹配视频通道会自动更新输入图像参数。 |
| `setOutputImagePara(const ImagePara& para)` / `getOutputImagePara()` | 初始化前/查询 | 单视频输出兼容接口。设置时会同步发布 ID 为 `0` 的视频输出通道。 |
| `setOutputMediaChannels(const std::vector<MediaChannelInfo>& channels)` | 生产者初始化期间 | 替换生产者发布的全部输出通道。 |
| `addOutputMediaChannel(const MediaChannelInfo& channel)` | 生产者初始化期间 | 新增输出通道；相同 ID 已存在时更新其描述。 |
| `clearOutputMediaChannels()` | 切换媒体源或重置时 | 清空输出通道描述。 |
| `getOutputMediaChannels()` | 生产者初始化后 | 获取全部输出通道；未显式发布时返回由旧图像接口生成的兼容通道。 |
| `getOutputMediaChannel(MediaChannelId id, MediaChannelInfo& channel)` | 生产者初始化后 | 按生产者输出通道 ID 查询描述，找到返回 `true`。 |
| `setInputMediaChannelRequirements(const std::vector<MediaChannelRequirement>& requirements)` | 建立连接前 | 替换消费者的全部输入格式要求。 |
| `addInputMediaChannelRequirement(const MediaChannelRequirement& requirement)` | 建立连接前 | 新增输入要求；相同逻辑输入 ID 已存在时更新。 |
| `getInputMediaChannelRequirements()` | 查询 | 获取消费者声明的输入格式要求。 |
| `getInputMediaChannels()` | 连接成功后 | 获取全部已匹配并配置的消费者输入通道。 |
| `getInputMediaChannel(MediaChannelId input_id, MediaInputChannel& channel)` | 连接成功后/消费时 | 按消费者逻辑输入 ID 查询来源生产者、生产者输出通道及媒体参数。 |
| `getName()` | 任意 | 获取模块名。 |
| `getModuleStatus()` | 任意 | 获取模块运行状态。 |
| `getMediaType()` | 初始化后 | 获取模块的兼容媒体类型字段。 |
| `setSynchronize(std::shared_ptr<Synchronize> syn)` | 任意 | 设置音视频同步对象。 |
| `addExternalConsumer(const std::string& name, MediaBufferHooker cb)` | 任意 | 添加外部消费者回调，返回一个外部消费模块。 |
| `dumpPipe()` / `dumpPipeSummary()` | 调试 | `dumpPipe()` 打印管线结构及各节点的输入通道、输入要求和输出通道完整参数；`dumpPipeSummary()` 打印运行统计信息。 |
| `receiveMediaBuffer(const std::shared_ptr<MediaBuffer>& buffer)` | 内部/手动输入 | 向模块输入队列压入缓冲。 |
| `receiveMediaBuffer(MediaBufferContext&& context)` | 内部/手动输入 | `final` 接收入口；向模块输入队列 move 一个带逻辑输入 ID 的上下文。派生类覆盖 `doConsume()`，不得覆盖此入口。 |
| `clearInputBufferQueue()` | 停止或重置时 | 清理未消费输入缓冲。 |
| `setInputBufferQueueSize(size_t size)` / `getInputBufferQueueSize()` | 任意 | 设置/获取输入队列容量，默认 `1024`，满队列时新缓冲会被丢弃。 |

#### 派生类开发与输出池提交

`ModuleMedia` 的输出池、轮转队列、初始化标志、模块类型、同步器和配置锁均为私有状态。
派生类不得直接依赖这些成员的名称、布局或容器实现，应只通过 public/protected 接口读写。
常用替代接口如下：

| 需求 | 派生类接口 |
| --- | --- |
| 设置/查询模块角色 | `setModuleType()` / `getModuleType()` |
| 设置兼容媒体类型 | `setMediaType()`；查询使用 public `getMediaType()` |
| 查询/切换模块状态 | public `getModuleStatus()` / protected `setModuleStatus()` |
| 查询配置的 Buffer 大小 | `getConfiguredBufferSize()`；`getBufferSize()` 查询当前池实际大小 |
| 查询/更新初始化状态 | `isInitialized()` / `setInitialized()` |
| 使用同步器 | `synchronizer()` |
| 设置回收前清 Cache 策略 | `setClearCachePolicy()` |
| 串行化初始化和重配置 | `lockConfiguration()` |
| 只读检查当前输出池 | `outputBufferPoolSize()` / `outputBufferAt()` |
| 发布或清空输出池 | `commitOutputBufferPool()` / `clearOutputBufferPool()` |

`lockConfiguration()` 用于低频的初始化、停止态重配置和资源切换，不应放入逐帧
`doConsume()` / `doProduce()` 热路径。修改配置或替换输出池时，派生类应在同一个配置锁
临界区内完成底层资源准备和池提交；成功提交后再调用 `setInitialized()`。

普通内存池可直接调用 `ModuleMedia::initBuffer()` 或
`ModuleMedia::initBuffer(buffer_type)`。需要自定义 Buffer、DMA/MPP 导入或外部资源的
派生类，应先在局部 `OutputBufferPool` 中完成全部分配、导入和校验，最后一次性发布：

```cpp
int ModuleExample::initBuffer()
{
    auto generation = createDeviceGeneration();
    if (!generation)
        return -ENOMEM;

    OutputBufferPool next;
    next.buffers.reserve(getBufferCount());
    next.rotation_buffers.reserve(getBufferCount());

    for (uint16_t i = 0; i < getBufferCount(); ++i) {
        auto buffer = createAndImportBuffer(generation, i);
        if (!buffer)
            return -ENOMEM;

        next.buffers.push_back(buffer);
        next.rotation_buffers.push_back(std::move(buffer));
    }

    next.recycle_handler =
        [generation](const std::shared_ptr<MediaBuffer>& buffer) {
            generation->release(buffer);
        };
    return commitOutputBufferPool(std::move(next));
}
```

`buffers` 是池所有者和索引查询视图，`rotation_buffers` 是轮转槽位视图。后者为空时
`commitOutputBufferPool()` 会从 `buffers` 补齐；对自定义池同时预留并填充两个 vector，
可避免提交阶段额外复制一遍 `shared_ptr`。epoch 会同时持有两个视图，因此 MPP 等模块使用
独立轮转 wrapper 时，底层 owner Buffer 也会存活到该代最后一个 flight 释放。提交会拒绝
空指针和超过 16 位槽位上限的池；
校验或内部分配失败时返回负 errno，当前活动池保持不变。

每次成功提交都会创建新的输出池 epoch。`recycle_handler` 保存在对应 epoch 中，旧池仍在
flight 的 Buffer 会继续调用旧 handler，最后一个 flight 释放后旧 epoch 才退出。因此处理器
应捕获该次建池所使用的 `shared_ptr` 资源代次，不应捕获 `this` 后再访问可能已被重配置替换的
当前设备、encoder、decoder、client 或 camera。

`recycle_handler` 由 ModuleMedia owner 线程在位图 drain 阶段调用，不在最后一个消费者线程
中执行；handler 应只处理对应资源代次的轻量回收，不应反向触发模块重配置。

优先在局部池的 `recycle_handler` 中完成代次绑定。只有复用基类 `initBuffer()` 建池、无法在
提交参数中直接设置 handler 的模块，才需要紧接着调用
`bindOutputBufferRecycleHandler()`；该接口要求池已经存在且尚无已预留或在途 Buffer，否则
分别返回 `-ENOENT` 或 `-EBUSY`。`bufferReleaseCallBack()` 只是在 epoch 未设置 handler 时使用
的兼容回退，不适合释放可能跨重配置代次的资源。停止或受控重配时可调用
`clearOutputBufferPool()` 发布空池；不要直接修改池容器，也不要依赖 `MediaBuffer` 的自定义
引用计数、状态位或引用归零回调完成轮转。

### MediaParameter

头文件：\`module/ff_media_parameter.hpp\`

\`MediaParameter\` 参考 GStreamer property、GstChildProxy 和 FFmpeg AVOption，使用一棵强类型参数树统一描述普通参数、结构化配置和原子事务：

- \`ParameterSchema\` 是模块类型共享的不可变参数树；实例只保存 binding、当前值和 revision。
- \`ParameterInfo\` 同时描述叶子参数和 \`OBJECT\` 节点，包含类型、说明、单位、默认值、范围、枚举、读写权限、可写状态及应用方式。
- 路径是唯一寻址方式，使用 \`/\` 分隔，例如 \`bitrate\`、\`sample/channels\`、\`crop/region/x\`。
- \`OBJECT\` 节点替代旧的 target/group 概念；\`atomic=true\` 表示该对象及任意后代的修改都必须合并后一次提交。
- \`ParameterValue\` 支持 \`BOOLEAN\`、\`INTEGER\`、\`DOUBLE\`、\`STRING\` 和递归 \`OBJECT\`。
- \`ParameterObject\` 既可表示完整对象，也可表示部分 patch；未出现的成员保持当前值。
- 注册和运行时都会校验名称、类型、范围、枚举、权限及状态，失败返回标准负 errno。
- \`parameterRevision()\` 是实例级单调递增版本，每次成功写入增加一次。
- binding getter/setter 在参数树锁之外执行，可以在 callback 内查询或读取其他参数；
  callback 内嵌套写入返回 \`-EDEADLK\`。标量写入会直接校验新值并调用 setter，
  不会为了设置参数而隐式调用 getter。同一实例已有 callback 执行时，其他线程的
  参数读写立即返回 \`-EAGAIN\`，调用方可稍后重试，元数据查询不受影响；getter
  循环依赖返回 \`-ELOOP\`。

\`ModuleMedia\` 默认注册以下通用参数：

| 路径 | 类型 | 默认值 | 范围 | 应用方式 | 说明 |
| --- | --- | --- | --- | --- | --- |
| \`buffer-count\` | INTEGER | \`0\` | \`0..65535\` | \`IMMEDIATE\` | 输出池配置数量；框架不施加模块级数量上限，调用方应在初始化或受控重配置前设置。 |
| \`buffer-size\` | INTEGER | \`0\` | \`0..INT64_MAX\` | \`IMMEDIATE\` | 单 Buffer 配置大小，单位字节；调用方负责与模块生命周期同步。 |
| \`input-queue-size\` | INTEGER | \`1024\` | \`1..INT64_MAX\` | \`IMMEDIATE\` | 输入队列最大缓冲数量；建议在启动前设置。C++ 直接接口允许传 \`0\`，表示丢弃全部新输入。 |

查询与设置统一使用路径：

\`\`\`cpp
std::shared_ptr<ModuleMedia> module = makeMediaModule<ModuleRga>();

for (const auto& info : module->queryParameters())
    std::cout << info.name << ": " << info.description << std::endl;

module->setParameter("buffer-count", 4);

uint16_t count = 0;
module->getParameter("buffer-count", count);

// 类似 AVOption 的字符串入口。
module->setParameterFromString("input-queue-size", "256");
\`\`\`

\`getParameter()\`、\`setParameter()\`、\`ParameterObject::getMember()\` 和数值型
\`setMember()\` 支持窄整数、无符号整数、浮点数和 enum。类型不匹配返回
\`-EINVAL\`，负数转无符号或数值越界返回 \`-ERANGE\`；转换失败时不会修改输出变量。

派生模块直接声明根节点，不再创建空名字 group。schema 与实例 binding 分离：

\`\`\`cpp
#include "module/ff_media_parameter_helpers.hpp"

class ModuleExample : public ModuleMedia
{
public:
    ModuleExample() : ModuleMedia("ModuleExample"), bitrate_(4000000)
    {
        static const auto schema = makeParameterSchema({
            integerParameter("bitrate", 4000000, 1, 100000000,
                             "Target bitrate", "bit/s"),
        });

        installParameterSchema(
            schema, {bindParameter("bitrate", bitrate_)});
    }

private:
    int64_t bitrate_;
};
\`\`\`

需要加锁、跨字段校验或重建底层资源时，使用 typed getter/setter。参数核心会先完成
\`ParameterValue\` 到 C++ 类型的转换和 schema 校验；setter 返回负 errno 即可拒绝提交：

\`\`\`cpp
bindParameter(
    "bitrate",
    [this]() { return bitrate_; },
    [this](int64_t value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_)
            return -EBUSY;
        bitrate_ = value;
        recreateEncoder();
        return 0;
    });
\`\`\`

\`ParameterInfo::writable_states\` 由参数核心检查，\`ParameterApplyMode\` 描述立即应用、
重配置、下次启动生效或仅构造期可写。涉及模块状态的 setter 仍应在模块锁内再次确认状态，
避免状态检查与提交之间的竞态。

#### 结构化与原子参数

对象通过 \`objectParameter()\` 直接加入参数树，成员可以继续递归包含对象：

\`\`\`cpp
ParameterInfo crop = objectParameter(
    "crop",
    {
        integerParameter("x", 0, 0, 8192),
        integerParameter("y", 0, 0, 8192),
        integerParameter("width", 1920, 1, 8192),
        integerParameter("height", 1080, 1, 8192),
        objectParameter(
            "region",
            {
                integerParameter("x", 0, 0, 8192),
                integerParameter("y", 0, 0, 8192),
            }),
    },
    "Visible image rectangle",
    PARAMETER_FLAG_READABLE | PARAMETER_FLAG_WRITABLE,
    true);  // atomic

static const auto schema = makeParameterSchema({crop});
\`\`\`

对象和任意后代都通过同一套路径 API 访问：

\`\`\`cpp
module->setParameter("crop/x", 120);
module->setParameterFromString("crop/y", "32");

int64_t x = 0;
module->getParameter("crop/x", x);

// 部分 patch：未列出的 width/height/region 保持当前值。
module->setParameter(
    "crop",
    ParameterObject({
        {"x", 100},
        {"region", {{"x", 20}}},
    }));
\`\`\`

\`queryParameters()\` 枚举根节点，\`queryParameters("crop")\` 枚举对象的直接子节点；
\`queryParameter("crop/region/x", info)\` 查询任意深度节点。查询返回沿父路径组合后的有效
metadata：读写权限和可写状态取交集，runtime/deprecated 标志向下继承，apply mode 取最严格值；
OBJECT 的 \`object_members\` 也会递归应用相同规则。OBJECT 默认值由各子节点默认值递归生成。
binding 注册同样按父链组合后的有效读写权限校验：有效只读节点只要求 getter，有效只写节点
只要求 setter，避免局部 flags 与运行时权限不一致。

原子对象使用 \`bindParameterObject()\` 绑定完整 getter/setter。参数核心先读取当前完整对象，
递归合并 patch，完成所有 schema 校验，再调用一次 setter。setter 可追加跨字段约束；
失败时对象不变。object getter 必须返回 schema 中的全部成员（包括嵌套成员），缺字段不会再用
默认值静默补齐。原子对象下禁止再绑定独立叶子 setter：

\`\`\`cpp
installParameterSchema(
    schema, {},
    {bindParameterObject(
        "crop",
        [this](ParameterObject& value) {
            value = ParameterObject({
                {"x", crop_.x},
                {"y", crop_.y},
                {"width", crop_.width},
                {"height", crop_.height},
            });
            return 0;
        },
        [this](const ParameterObject& value) {
            Crop next;
            if (value.getMember("x", next.x) < 0
                || value.getMember("y", next.y) < 0
                || value.getMember("width", next.width) < 0
                || value.getMember("height", next.height) < 0) {
                return -EINVAL;
            }
            if (next.x + next.width > input_width_)
                return -ERANGE;
            crop_ = next;
            return 0;
        })});
\`\`\`

未绑定的参数由参数系统保存；绑定参数以 getter/setter 为数据源。安装 schema 时会读取并校验
getter 当前值。callback 可重入查询或读取当前实例的其他参数，但不能嵌套写入；跨线程冲突
不会阻塞等待，调用方收到 `-EAGAIN` 后可重试。

非原子 OBJECT 的一次 patch 如果涉及两个及以上绑定 setter，参数核心会在调用任何 setter 前
返回 `-ENOTSUP`，避免前一成员已经生效、后一成员失败造成部分提交。需要跨多个 setter 的配置
应声明为 atomic OBJECT 并使用一个 object setter。纯内部存储 patch 或只涉及一个 setter 的
patch 仍可正常提交；非原子父对象中的子级 object binding 会作为一个完整提交动作执行。
整对象 patch 会递归检查实际出现的每个子项权限和可写状态，不能借父对象绕过只读或生命周期
门禁；读取整对象时若子树含不可读成员则返回 `-EACCES`，仍可单独读取其中允许访问的路径。

当前 `module/vi`、`module/vp`、`module/vo` 下的具体派生模块都会在析构函数入口调用
`stop()`，使工作线程在派生成员资源释放前退出。正常流程仍应显式从源节点调用 `stop()`；
自动停止用于异常返回或遗漏清理时兜底。自定义 `ModuleMedia` 派生类也应遵循相同规则，
因为 `ModuleMedia` 基类析构函数本身不会调用 `stop()`。

派生模块如果需要在消费处理中区分逻辑输入，应覆盖：

```cpp
ConsumeResult doConsume(const MediaBufferContext& input,
                        std::shared_ptr<MediaBuffer>& output_buffer) override
{
    MediaInputChannel channel;
    if (!getInputMediaChannel(input.input_id, channel))
        return CONSUME_SKIP;

    auto producer = channel.producer.lock();
    MediaChannelId producer_output_id = channel.producer_channel_id;
    return process(input.buffer, output_buffer, producer, producer_output_id);
}
```

派生消费者只覆盖该 `MediaBufferContext` 版本。`ModuleRga` 和
`ModuleImageProcessor` 保留的 `shared_ptr` 重载仅用于手动处理，不属于基类虚接口。

## 公共数据缓冲

### MediaBuffer

头文件：`base/media_buffer.hpp`

`MediaBuffer` 是 FFMedia 管线中的基础数据载体，保存媒体数据指针、有效数据区域、时间戳、媒体参数、附加数据和引用计数等信息。视频帧、音频帧、编码包、附加数据都可以通过该类型在模块间传递。

| API | 说明 |
| --- | --- |
| `MediaBuffer(size_t size = 0)` | 构造基础缓冲，可指定初始申请大小。 |
| `MediaBuffer(const MediaBuffer& other)` | 复制索引、时间戳、EOS、flags、媒体类型、图像/采样参数、codec 和通道 ID；不复制载荷、私有数据、附加数据、引用计数和回调。 |
| `allocBuffer(size_t size)` | 申请或重新申请内部数据缓冲。 |
| `fillWithBlack()` | 将缓冲填充为黑色或静默数据，具体行为由实现决定。 |
| `clone()` | 通过虚函数创建同动态类型对象；基础实现按 `active_size` 分配独立内存并复制 `active_data`。 |
| `STATUS_CLEAN` / `STATUS_DIRTY` | 缓冲状态常量，表示空闲/脏数据状态。 |

基础内存和有效数据区域：

| API | 说明 |
| --- | --- |
| `getIndex()` / `setIndex(uint16_t index)` | 获取/设置缓冲在池中的索引。 |
| `getData()` / `setData(void* data)` | 获取/设置底层数据地址。 |
| `getSize()` / `setSize(size_t size)` | 获取/设置底层数据容量。 |
| `getActiveData()` / `setActiveData(void* data)` | 获取/设置当前有效数据地址。 |
| `getActiveSize()` / `setActiveSize(size_t size)` | 获取/设置当前有效数据大小。 |

时间戳、标志和私有数据：

| API | 说明 |
| --- | --- |
| `getPUstimestamp()` / `setPUstimestamp(int64_t ts)` | 获取/设置 presentation timestamp，单位通常为微秒。 |
| `getDUstimestamp()` / `setDUstimestamp(int64_t ts)` | 获取/设置 decode timestamp，单位通常为微秒。 |
| `getEos()` / `setEos(bool eos)` | 获取/设置流结束标志。 |
| `getFlags()` / `setFlags(int flags)` | 获取/设置媒体标志位，例如关键帧等模块自定义标志。 |
| `getPrivateData()` / `setPrivateData(void* data)` | 获取/设置私有上下文指针。 |
| `getExtraData()` / `setExtraData(std::shared_ptr<MediaBuffer> extra)` | 获取/设置关联附加数据缓冲，例如 SPS/PPS、AAC config。 |

状态和引用计数：

| API | 说明 |
| --- | --- |
| `getStatus()` / `setStatus(bool status)` | Deprecated 兼容状态；`ModuleMedia` 轮转不使用。 |
| `increaseRefCount()` / `decreaseRefCount()` | Deprecated 兼容计数；`ModuleMedia` 轮转不使用。 |
| `getRefCount()` / `setRefCount(uint16_t count)` | Deprecated 兼容计数查询/设置。 |
| `setOnRefZeroCallback(void* owner, onRefZeroCB cb)` | Deprecated 兼容归零回调。 |
| `onRefZero(const std::shared_ptr<MediaBuffer>& buffer)` | Deprecated 手动兼容入口。 |

媒体类型和媒体参数：

| API | 说明 |
| --- | --- |
| `getMediaBufferType()` / `setMediaBufferType(MEDIA_BUFFER_TYPE type)` | 获取/设置缓冲媒体类型，如视频、音频或其他数据。 |
| `getImagePara()` / `setImagePara(const ImagePara& para)` | 获取/设置视频图像参数。 |
| `getSamplePara()` / `setSamplePara(const SampleInfo& para)` | 获取/设置音频采样参数。 |
| `getMediaCodec()` / `setMediaCodec(media_codec_t codec)` | 获取/设置媒体编码类型。 |
| `getMediaChannelId()` / `setMediaChannelId(uint32_t channel_id)` | 获取/设置该 Buffer 所属的生产者本地输出通道 ID，生产者分发时据此选择路由。 |

使用注意：

- `data/size` 表示缓冲总内存，`active_data/active_size` 表示当前有效载荷。
- 派生类应实现自己的拷贝构造函数并 override `clone()`，返回 `std::make_shared<Derived>(*this)`。
- 普通浅拷贝直接复制 `std::shared_ptr<MediaBuffer>`；拷贝构造只复制元数据，`clone()` 执行载荷深拷贝。
- `clone()` 不复制 `private_data`、`extra_data`、缓冲池引用计数或引用归零回调；调用方如需这些关联信息应显式重新设置。
- 流初始化附加数据通常也用 `MediaBuffer` 表示，并发布到 `MediaChannelInfo::extra_data`；
  `connectProducer()` 会将其复制到消费者的 `MediaInputChannel::media.extra_data`。
- `MediaBuffer::setExtraData()` 用于随具体数据包携带动态附加数据；模块级 `setExtraBuffer()`
  主要用于手动输入、未建立通道连接或覆盖自动配置的场景。
- 模块间零拷贝生命周期只由 flight `shared_ptr` 控制块决定；外部异步使用时直接保存回调收到的 `shared_ptr`。
- `ModuleMedia` 不再提供 `exportBufferFromBufferPool()` / `importBufferToBufferPool()`；需要独立载荷使用 `clone()`，需要短期零拷贝持有则保存 flight `shared_ptr`。

### VideoBuffer

头文件：`base/video_buffer.hpp`

`VideoBuffer` 继承自 `MediaBuffer`，面向视频帧和硬件缓冲扩展，支持 DRM buffer、MPP buffer、malloc buffer 和外部 buffer。MPP 编解码、RGA、DRM 显示等模块会使用该类型承载图像数据和零拷贝 fd。

缓冲类型 `VideoBuffer::BUFFER_TYPE`：

- `DRM_BUFFER_NONCACHEABLE`：无缓存 DRM 内存。
- `DRM_BUFFER_CACHEABLE`：缓存 DRM 内存。
- `MALLOC_BUFFER`：普通堆内存。
- `EXTERNAL_BUFFER`：外部传入内存。
- `DRM_BUFFER_NONCACHEABLE_DMA32`：DMA32 无缓存 DRM 内存。
- `DRM_BUFFER_CACHEABLE_DMA32`：DMA32 缓存 DRM 内存。

| API | 说明 |
| --- | --- |
| `VideoBuffer(BUFFER_TYPE type)` | 按指定类型构造视频缓冲。 |
| `VideoBuffer(const VideoBuffer& other)` | 只复制媒体元数据和缓冲类型，不复用源 DRM 或其他硬件后端句柄。 |
| `resetBuffer()` | 重置缓冲内部状态和资源引用。 |
| `allocBuffer(ImagePara para)` | 按图像参数重新申请视频缓冲并更新图像参数；保留时间戳、EOS、flags、通道、附加数据等现有帧元数据。 |
| `allocBuffer(size_t size)` | 按字节大小重新申请视频缓冲，并保留现有帧元数据。 |
| `clone()` | override 基类接口；有效图像参数存在时按 `ImagePara` 申请独立视频缓冲，否则按 `active_size` 申请，复制有效载荷并保留帧元数据和有效载荷大小。 |
| `fillWithBlack()` | 填充整帧为黑色。 |
| `fillWithBlack(uint32_t x, uint32_t y, uint32_t w, uint32_t h)` | 填充指定矩形区域为黑色。 |
| `initWithExternalBuffer(void* data, size_t size, int fd)` | 使用外部内存和 fd 初始化缓冲。 |

DRM 与 DMA-BUF 资源接口：

| API | 说明 |
| --- | --- |
| `getDrmBuf()` / `setDrmBuf(DrmBuffer* buffer)` | 获取/设置关联 DRM buffer。 |
| `getBufFd()` / `setBufFd(int fd)` | 获取/设置缓冲 fd，供零拷贝传递。 |
| `flushDrmBuf()` | 刷新 DRM 缓冲 cache，通常在 CPU 写入后给硬件读取前调用。 |
| `invalidateDrmBuf()` | 使 DRM 缓冲 cache 失效，通常在硬件写入后 CPU 读取前调用。 |

MPP buffer 的导入、引用和释放由 MPP 编解码模块在库内部管理，不属于公开
`VideoBuffer` API。外部模块只需提供有效的 DMA-BUF fd、容量和图像参数。

缓冲类型查询：

| API | 说明 |
| --- | --- |
| `getBufferType()` / `setBufferType(BUFFER_TYPE type)` | 获取/设置视频缓冲类型。 |

使用注意：

- 硬件处理链路中优先使用  fd 传递，减少拷贝。
- 缓存 DRM 内存在 CPU 与硬件共享时需要关注 `flushDrmBuf()` 和 `invalidateDrmBuf()`。
- `EXTERNAL_BUFFER` 适合封装由外部模块管理生命周期的内存；调用方需要确保外部内存在使用期间有效。

## vi 输入模块

### ModuleCam

头文件：`module/vi/module_cam.hpp`

V4L2 摄像头输入源，支持 MIPI CSI 摄像头和 USB 摄像头。

| API | 说明 |
| --- | --- |
| `ModuleCam(std::string vdev)` | 构造摄像头输入源，`vdev` 为视频设备路径，如 `/dev/video0`。 |
| `changeSource(std::string vdev)` | 停止状态下切换视频设备。调用后需重新初始化模块。 |
| `camIoctlOperation(unsigned long cmd, void* arg)` | 初始化后透传 V4L2 `ioctl` 操作。 |
| `setTimeOutSec(unsigned sec, unsigned usec)` | 设置采集超时时间。 |
| `init()` | 初始化采集设备和缓冲。 |

角色：源组件，输出视频帧。

### ModuleAlsaCapture

头文件：`module/vi/module_alsaCapture.hpp`

ALSA 音频采集源。

| API | 说明 |
| --- | --- |
| `ModuleAlsaCapture(const std::string& dev)` | 使用默认音频参数构造采集模块。 |
| `ModuleAlsaCapture(const std::string& dev, const SampleInfo& sample_info, AI_LAYOUT_E layout)` | 指定采样格式、采样率、通道数和通道布局。 |
| `changeSource(const std::string& dev, const SampleInfo& sample_info, AI_LAYOUT_E layout)` | 停止状态下切换采集设备或音频参数。调用后需重新初始化模块。 |
| `init()` | 初始化 ALSA 采集。 |

角色：源组件，输出音频 PCM 或布局转换后的音频数据。

### ModuleFFmpegDemux

头文件：`module/vi/module_ffmpegDemux.hpp`

基于 FFmpeg 的输入源，支持文件、网络流、UVC、设备 等输入，不支持mipi摄像头输入。

| API | 说明 |
| --- | --- |
| `ModuleFFmpegDemux(const std::string& filename, int loop = 1)` | 构造解封装输入源，`loop` 为循环读取次数。 |
| `changeSource(const std::string& filename, int loop = 1)` | 停止状态下切换输入源。调用后需重新初始化模块。 |
| `setInputFormat(const std::string& format)` | 指定 `AVInputFormat` 短名称，空字符串表示自动探测。 |
| `setFormatOption(const std::string& key, const std::string& value, int flags)` | 设置 FFmpeg format option；`key` 为空时清空全部选项。 |
| `getFormatOption(const std::string& key, int flags)` | 查询 format option。 |
| `setFileSeek(int64_t ts, int flags)` | 初始化后设置读取位置；运行期调用会与 `av_read_frame()` 串行，并刷新 bitstream filter 与首包状态。 |
| `getAudioCodec()` / `getVideoCodec()` | 初始化后获取音频/视频编码类型。 |
| `getAudioSampleInfo()` | 初始化后获取音频采样信息。 |
| `getExtraBuffer(MEDIA_BUFFER_TYPE media_type)` | 初始化后获取指定媒体类型的附加数据。 |
| `setTimeOut(int64_t usec)` | 设置读包超时，单位微秒。 |
| `init()` | 打开输入源并读取媒体参数。 |

角色：源组件，分别发布视频和音频输出通道，输出压缩音视频包或读取到的媒体数据。通道 ID 使用对应 FFmpeg stream index。

### ModuleFileReader

头文件：`module/vi/module_fileReader.hpp`

文件输入源，支持裸流读取及 mp4、mkv、flv、ts、ps 等媒体文件解封装读取。

| API | 说明 |
| --- | --- |
| `ModuleFileReader(std::string path, bool loop_play = false)` | 构造文件读取模块。 |
| `changeSource(std::string path, bool loop_play = false)` | 停止状态下切换文件。调用后需重新初始化模块。 |
| `getAudioCodec()` / `getVideoCodec()` | 初始化后获取音频/视频编码类型。 |
| `getAudioSampleInfo()` | 初始化后获取音频采样信息。 |
| `getExtraBuffer(MEDIA_BUFFER_TYPE media_type)` | 初始化后获取媒体附加数据。 |
| `setFileReaderSeek(int64_t ms_time)` | 初始化后设置读取点；媒体文件按毫秒，裸流按文件偏移量；运行期调用与当前 `decode()` 串行。 |
| `getFileReaderMaxSeek()` | 初始化后获取最大时长或最大偏移量；查询与读取/seek 使用同一锁。 |
| `init()` | 初始化文件读取器。 |

角色：源组件，输出文件中的音视频数据；视频通道 ID 为 `0`，音频通道 ID 为 `1`。

### ModuleMemReader

头文件：`module/vi/module_memReader.hpp`

内存输入源，用于将外部内存或 `MediaBuffer` 注入 FFMedia 管线。

处理状态 `DATA_PROCESS_STATUS`：

- `PROCESS_STATUS_EXIT`：退出，不再阻塞处理。
- `PROCESS_STATUS_HANDLE`：正在处理输入。
- `PROCESS_STATUS_PREPARE`：输入已准备，等待处理。
- `PROCESS_STATUS_DONE`：处理完成，等待新输入。

| API | 说明 |
| --- | --- |
| `ModuleMemReader(const ImagePara& para)` | 按输入图像参数构造内存源。 |
| `changeInputPara(const ImagePara& para)` | 停止状态下修改输入图像参数。调用后需重新初始化模块。 |
| `setInputBuffer(void* buf, size_t bytes, int buf_fd = -1, int64_t pts = 0)` | 设置外部内存输入；`buf_fd` 用于零拷贝，`pts` 单位微秒。 |
| `setInputBuffer(const std::shared_ptr<MediaBuffer>& buf)` | 直接设置 `MediaBuffer` 输入。 |
| `waitProcess(int timeout_ms)` | 等待当前输入处理完成，超时返回 `-100`。 |
| `setProcessStatus(DATA_PROCESS_STATUS status)` / `getProcessStatus()` | 设置/获取处理状态。 |
| `init()` | 初始化内存源。 |

注意：该类重载 `setBufferCount(uint16_t)` 为空实现，缓冲数量不由外部设置。

### ModuleAppSource / ModuleAppProcessor

头文件：`module/module_app.hpp`

`ModuleAppSource` 是面向应用的通用输入源。应用可以提交外部内存、DMA-BUF 或
`MediaBuffer`，并通过 `MediaChannelInfo` 声明视频、音频或其他输出通道。模块支持阻塞、
丢弃最新帧和丢弃最旧帧三种队列策略。

仅使用 FFMedia 头文件和 `libff_media.so` 的完整可运行示例见：

- `tests/test_module_app_source.cpp`：单/多媒体通道、三种内存生命周期、DMA-BUF、已有
  `MediaBuffer`、ticket/wait、EOS、flush、队列策略、普通模块连接和外部消费者；
- `tests/test_module_app_processor.cpp`：FORWARD、REPLACE、DROP、ERROR、异常处理、自动输出
  通道、多通道路由、替换内存生命周期和外部消费者。

`AppMemoryMode` 定义外部内存的生命周期：

- `BORROW`：调用方持有内存，必须获取 ticket 并等待 `wait()` 完成后才能释放或复用。
- `HOLD_OWNER`：模块持有 `AppFrame::owner`，直到所有下游释放该帧。
- `COPY`：提交时复制载荷，`submit()` 返回后调用方即可释放输入。

提交已有 `MediaBuffer` 时，`HeldFrame::frame.buffer` 会直接保存该 `shared_ptr`，直到该帧完成或被丢弃。

| API | 说明 |
| --- | --- |
| `ModuleAppSource(channels, options)` | 创建应用输入源并发布一个或多个媒体通道。 |
| `submit(frame, timeout_ms, ticket)` | 提交外部帧；成功接收返回 0。 |
| `submit(buffer, channel_id, timeout_ms, ticket)` | 提交已有 `MediaBuffer`。 |
| `wait(ticket, timeout_ms)` | 等待该帧被全部下游释放；保留最近 1024 个尚未 wait 的完成结果。 |
| `sendEos(channel_id, ...)` | 向指定通道发送 EOS，随后该通道不再接受普通帧。 |
| `flush(discard_pending, timeout_ms)` | 丢弃待发送帧或等待待发送及在途帧全部完成。 |

`ModuleAppProcessor` 是同步回调处理节点。回调在模块自己的工作线程中执行，可以返回：

- `FORWARD`：零拷贝转发当前 FFMedia Buffer；
- `REPLACE`：用应用提供的 `AppFrame` 或 `MediaBuffer` 替换输入；
- `DROP`：丢弃当前输入；
- `ERROR`：将模块切换为异常状态。

输入 Buffer 默认只读。由于输入可能同时发送给多个下游，应用不应在回调中原地修改共享
载荷。需要修改数据时应返回 `REPLACE`；外部替换载荷必须使用 `HOLD_OWNER` 或 `COPY`。

### ModuleRtmpClient

头文件：`module/vi/module_rtmpClient.hpp`

RTMP 客户端，兼具输入和输出能力，支持拉流和推流。

| API | 说明 |
| --- | --- |
| `ModuleRtmpClient(std::string rtmp_url, ImagePara para = ImagePara(), int publish = 1)` | 构造 RTMP 客户端；`publish` 注释定义为 `1` 拉流、`0` 推流。 |
| `changeSource(std::string rtmp_url, int publish = 1)` | 停止状态下切换 RTMP 地址和模式。调用后需重新初始化模块。 |
| `getExtraBuffer(MEDIA_BUFFER_TYPE media_type)` | 初始化后获取指定媒体类型附加数据。 |
| `setTimeOutSec(int sec, int usec)` | 设置网络数据超时时间。 |
| `init()` | 初始化 RTMP 客户端。 |

角色：拉流时为源组件并发布视频/音频通道；推流时为输出消费者。拉流视频通道 ID 为 `0`，音频通道 ID 为 `1`。

### ModuleRtspClient

头文件：`module/vi/module_rtspClient.hpp`

RTSP 客户端输入源，支持 UDP、TCP、多播。

会话状态 `SESSION_STATUS`：

- `SESSION_STATUS_CLOSED`
- `SESSION_STATUS_OPENED`
- `SESSION_STATUS_PLAYING`
- `SESSION_STATUS_PAUSE`

| API | 说明 |
| --- | --- |
| `ModuleRtspClient(std::string rtsp_url, RTSP_STREAM_TYPE stream_type = RTSP_STREAM_TYPE_UDP, bool enable_video = true, bool enable_audio = false)` | 构造 RTSP 输入源。 |
| `changeSource(std::string rtsp_url, RTSP_STREAM_TYPE stream_type = RTSP_STREAM_TYPE_UDP)` | 停止状态下切换地址或协议。调用后需重新初始化模块。 |
| `getVideoCodec()` / `getAudioCodec()` | 初始化后获取编码类型。 |
| `getExtraBuffer(MEDIA_BUFFER_TYPE media_type)` | 初始化后获取媒体附加数据。 |
| `getAudioSampleInfo()` | 初始化后获取音频采样信息。 |
| `audioChannel()` / `audioSampleRate()` | 获取音频通道数和采样率。 |
| `videoFPS()` | 获取视频帧率。 |
| `setTimeOutSec(unsigned sec, unsigned nsec)` | 设置网络超时，内部换算为毫秒；默认约 5 秒。 |
| `getSessionStatus()` | 获取 RTSP 会话状态。 |
| `init()` | 初始化 RTSP 客户端。 |

初始化成功后发布视频/音频输出通道；视频通道 ID 为 `0`，音频通道 ID 为 `1`。

### ModuleVideoStack

头文件：`module/vp/module_videoStack.hpp`

视频拼接处理模块，将多个输入通道的视频帧按指定区域合成为一路视频流。

| API | 说明 |
| --- | --- |
| `ModuleVideoStack(const std::string& module_name, int width, int height, float fps)` | 构造指定输出尺寸和帧率的视频拼接模块。 |
| `setModuleStackParams(MediaChannelId input_id, const ImageCrop& stack_params)` | 按输入通道 ID 设置拼接区域，并在内部创建或更新对应的 `MediaChannelRequirement`；宽或高为 `0` 时禁用该通道并将其上一次有效区域恢复为背景色。 |
| `init()` | 初始化拼接输出缓冲。 |

输入通道由 `setModuleStackParams()` 根据 `input_id` 自动创建或更新 requirement，使用标准 `connectProducer()` 和 `removeProductor()` 管理连接。角色：多输入处理组件，内部使用线程池和 RGA 更新拼接缓存；每个启用通道只保留一个在途处理任务，忙碌时跳过新帧。定时线程通过 Clock Buffer 传递单调时钟 PTS，`doConsume()` 收到时钟后直接生成并输出快照。`frame-rate` 使用原子间隔，可在模块运行期间修改，并从下一轮定时调度开始生效。`thread-count` 配置线程池最大并发线程数，默认值为 `2`，最小值为 `1`，可通过参数系统动态调整。

## vp 处理模块

### ModuleAacDec

头文件：`module/vp/module_aacdec.hpp`

AAC 音频解码模块。

| API | 说明 |
| --- | --- |
| `ModuleAacDec()` | 默认构造；连接生产者后从匹配 AAC 通道读取采样参数和附加数据。 |
| `ModuleAacDec(std::shared_ptr<MediaBuffer> extra_buffer)` | 使用附加数据构造。 |
| `ModuleAacDec(const uint8_t* extradata, unsigned extradata_size, int sample_rate, int nb_channels = -1)` | 显式传入 AAC 附加数据和采样信息。 |
| `changeSampleInfo(const SampleInfo& sample_info)` | 停止状态下修改输入采样信息。调用后需重新初始化模块。 |
| `init()` | 初始化 AAC 解码器。 |

角色：处理组件，输入 AAC，输出 PCM 音频。

输入要求：音频类型、`MEDIA_CODEC_AUDIO_AAC`。输出通道为 ID `0` 的 `MEDIA_CODEC_AUDIO_PCM_S16`。

### ModuleAacEnc

头文件：`module/vp/module_aacenc.hpp`

AAC 音频编码模块。

| API | 说明 |
| --- | --- |
| `ModuleAacEnc()` | 默认构造；连接生产者后从匹配 PCM 通道读取采样参数。 |
| `ModuleAacEnc(const SampleInfo& sample_info)` | 指定输入音频采样信息。 |
| `changeSampleInfo(const SampleInfo& sample_info)` | 停止状态下修改输入采样信息。调用后需重新初始化模块。 |
| `getExtraBuffer()` | 初始化后获取 AAC 附加数据。 |
| `setAot(int aot)` / `getAot()` | 设置/获取 AAC object type，例如 `2` LC、`5` HE-AAC、`29` HE-AACv2、`23` LD、`39` ELD。 |
| `setBitrate(int bitrate)` / `getBitrate()` | 设置/获取码率。 |
| `setAfterburner(int afterburner)` / `getAfterburner()` | 设置/获取 afterburner。 |
| `setEldSbr(int eld_sbr)` / `getEldSbr()` | 设置/获取 ELD SBR。 |
| `setVbr(int vbr)` / `gerVbr()` | 设置/获取 VBR 参数。 |
| `init()` | 初始化 AAC 编码器。 |

输入参数限制见头文件注释：`SampleFormat` 支持 `SAMPLE_FMT_S16`、`SAMPLE_FMT_NONE`；采样率支持 96000 到 8000 等常见值；通道数 `1~8`。

输入要求：PCM 音频，当前自动匹配要求采样格式为 `SAMPLE_FMT_S16`。输出通道为 ID `0` 的 AAC，并携带可用的 AAC 附加数据。

### ModuleMppDec

头文件：`module/vp/module_mppdec.hpp`

MPP 视频解码模块，支持 MPEG1、MPEG2、MPEG4、H264、H265、MJPEG、VP8、VP9。

| API | 说明 |
| --- | --- |
| `ModuleMppDec(const ImagePara& input_para = ImagePara())` | 根据输入图像参数构造；默认空参数时由匹配输入通道自动配置。 |
| `ModuleMppDec(const ImagePara& input_para, DecodeType type)` | 保留的兼容接口；输入 fourcc 无法派生类型时使用显式类型。 |
| `setNeedSplit(uint32_t split)` | 在 `CREATED` 或 `STOPPED` 状态设置内部分帧模式，`0` 关闭、`1` 开启，默认 `0`。 |
| `setFastMode(uint32_t fast)` | 在 `CREATED` 或 `STOPPED` 状态设置快速解析模式，`0` 关闭、`1` 开启，默认 `1`。 |
| `setDeinterlace(uint32_t deinterlace)` | 在 `CREATED` 或 `STOPPED` 状态设置去隔行，`0` 关闭、`1` 开启，默认 `1`。 |
| `setOutputTimeOut(int timeout_ms)` | 设置取帧超时时间，默认 `0`。 |
| `setBufferType(VideoBuffer::BUFFER_TYPE type)` | 在 `CREATED` 或 `STOPPED` 状态设置输出缓冲类型，默认 `DRM_BUFFER_NONCACHEABLE`。 |
| `init()` | 初始化 MPP 解码器。 |

角色：处理组件，输入压缩视频码流，输出解码后的图像帧。

输出 buffer 数量由调用方在 `init()` 前通过继承的 `setBufferCount()` 配置。框架不会根据
H.264/H.265 类型自动提升数量或设置 codec 专用下限；调用方应结合码流参考帧需求、下游
持有时长和 fanout 选择池大小。

参数 `decode-type` 是只读派生值：优先由 `input/format` 的 fourcc 决定，不再接受会在
`init()` 时被覆盖的独立参数写入。`output/format` 从构造完成起即为有效默认值
`V4L2_PIX_FMT_NV12`。普通解码配置的参数 getter、`init()` 与旧公开 setter 使用同一模块锁，
因此停止态重配置与并发参数查询不会读取到撕裂配置；运行期 timeout 仍使用原子快照。

输入要求会自动匹配 MPEG1、MPEG2、MPEG4、H264、H265、VP8、VP9、MJPEG 压缩视频通道，不会连接同一生产者的音频通道。输出通道为 ID `0` 的 RAW 视频。

### ModuleMppEnc

头文件：`module/vp/module_mppenc.hpp`

MPP 视频编码模块，支持 H264、H265、MJPEG。

| API | 说明 |
| --- | --- |
| `ModuleMppEnc(media_codec_t type, int fps = 30, ...)` / `ModuleMppEnc(EncodeType type, const ImagePara& input_para = ImagePara(), ...)` | 构造视频编码器；支持 H264、H265、MJPEG，推荐使用 `media_codec_t`，并保留 `EncodeType` 兼容接口。`media_codec_t` 接口的输入图像参数由匹配输入通道自动配置。 |
| `setDuration(int64_t duration)` | 设置输出时间戳间隔，单位微秒；小于等于 `0` 时使用输入时间戳。默认 30 fps 实例为 `33333` 微秒。 |
| `changeEncodeParameter(media_codec_t type, ...)` | 停止状态下修改编码类型、帧率、GOP、码率、码控模式、质量系数和 profile。调用后需重新初始化模块。 |
| `setIntraRefresh(bool intra_refresh, int refresh_mode, int refresh_num)` | 设置帧内刷新；`refresh_mode` 为 `0` 行刷新、`1` 列刷新。活动编码器存在时立即下发。 |
| `setOutputTimeOut(int timeout_ms)` | 设置取编码输出超时时间。 |
| `setInputCachePoolSize(int size)` | 设置输入缓存池大小，必须小于生产者输出缓冲区数量，默认 `1`。 |
| `getExtraBuffer()` | 初始化后获取编码附加数据。 |
| `init()` | 初始化 MPP 编码器。 |

角色：处理组件，输入原始图像帧，输出压缩视频码流。

参数路径 `intra-refresh`、`duration-usec`、`output-timeout-ms` 和
`input-cache-pool-size` 均为运行期参数；`intra-refresh` 在编码器已创建时立即下发，
否则会在后续 MPP 初始化时应用。

`duration-usec` 从 `0` 在运行期切换为正值时，下一帧会以 MPP 返回的当前输入 PTS 重基，
之后再按配置间隔递增，不会重新从 `0` 开始。编码配置 OBJECT 的 getter、`init()` 和
停止态旧公开重配置接口统一由模块锁保护；timestamp 重基只持有独立的短临界区。

输入要求为 RAW 视频。输出通道 ID 为 `0`，编码格式由 `media_codec_t`（或兼容接口的 `EncodeType`）决定，初始化后通道描述包含可用的 SPS/PPS 等附加数据。

### ModuleRga

头文件：`module/vp/module_rga.hpp`

RGA 图像处理模块，支持格式转换、缩放、裁剪、旋转、翻转、叠加等。

调度核心 `RGA_SCHEDULER_CORE`：

- `SCHEDULER_DEFAULT`
- `SCHEDULER_RGA3_CORE0`
- `SCHEDULER_RGA3_CORE1`
- `SCHEDULER_RGA2_CORE0`
- `SCHEDULER_RGA2_CORE1`
- `SCHEDULER_RGA3_DEFAULT`
- `SCHEDULER_RGA2_DEFAULT`

混合模式 `RGA_BLEND_MODE`：

- `BLEND_DISABLE`
- `BLEND_SRC`
- `BLEND_DST`
- `BLEND_SRC_OVER`
- `BLEND_DST_OVER`

| API | 说明 |
| --- | --- |
| `ModuleRga()` | 默认构造。 |
| `ModuleRga(const ImagePara& output_para, RgaRotate rotate)` | 指定输出参数和旋转模式。 |
| `ModuleRga(const ImagePara& input_para, const ImagePara& output_para, RgaRotate rotate)` | 指定输入/输出参数和旋转模式。 |
| `changeOutputPara(const ImagePara& para)` | 停止状态下修改输出图像参数。调用后需重新初始化模块。 |
| `setInputImageCrop(const ImageCrop& crop)` / `getInputImageCrop()` | 设置/获取输入图像处理区域。 |
| `setOutputImageCrop(const ImageCrop& crop)` / `getOutputImageCrop()` | 设置/获取输出图像写入区域。 |
| `setZoom(float zoom)` | 设置放大比例。 |
| `setZoomCenter(int x, int y)` | 设置放大中心点。 |
| `setBufferType(VideoBuffer::BUFFER_TYPE type)` | 设置输出缓冲类型。 |
| `setSrcPara(...)` / `setDstPara(...)` / `setPatPara(...)` | 设置输入、输出、叠加图像参数。 |
| `setSrcBuffer(void* buf)` / `setSrcBuffer(int fd)` | 设置输入图像虚拟地址或 fd。 |
| `setPatBuffer(void* buf, RGA_BLEND_MODE mode)` / `setPatBuffer(int fd, RGA_BLEND_MODE mode)` | 设置叠加图像地址或 fd 及混合模式。 |
| `setRotate(RgaRotate rotate)` | 设置旋转/翻转模式。 |
| `setRgaSchedulerCore(RGA_SCHEDULER_CORE core)` | 设置 RGA 调度核心。 |
| `dstFillColor(int color)` | 填充目标颜色。 |
| `alignStride(uint32_t fmt, uint32_t& wstride, uint32_t& hstride)` | 静态工具，按格式对齐 stride。 |
| `doConsume(const std::shared_ptr<MediaBuffer>& input_buffer, std::shared_ptr<MediaBuffer>& output_buffer)` | 可手动调用进行一次 RGA 处理；适合停止状态下手动处理。 |
| `init()` | 初始化 RGA 模块。 |

角色：处理组件，输入图像帧，输出处理后的图像帧。

自动连接时仅匹配 RAW 视频通道；`ModuleRga(output_para, rotate)` 可只指定输出参数，输入参数由连接结果配置。

### ModuleImageProcessor

头文件：`module/vp/module_imageProcessor.hpp`

图像处理模块，支持 RGB/YUV 格式转换、缩放、裁剪、旋转、镜像和 cover。

裁剪、旋转、镜像、翻转、Cover 和输出配置均通过 `MediaParameter` 路径访问，不新增
独立的 Cover C++ 接口或 Python 类型。

| 参数路径 | 类型 | 说明 |
| --- | --- | --- |
| `transform/crop/x`、`transform/crop/y` | INTEGER | 输入裁剪起点。 |
| `transform/crop/width`、`transform/crop/height` | INTEGER | 输入裁剪尺寸；宽高均为 `0` 表示完整输入。 |
| `transform/rotation` | INTEGER，单位 degree | 顺时针角度；当前支持 `0`、`90`、`180`、`270`，不是枚举值。 |
| `transform/mirror`、`transform/flip` | BOOLEAN | 水平镜像和垂直翻转。 |
| `cover/index` | INTEGER | Cover 槽位，范围 `0..15`。 |
| `cover/crop/x`、`cover/crop/y` | INTEGER | Cover 在输出图像中的起点。 |
| `cover/crop/width`、`cover/crop/height` | INTEGER | Cover 尺寸；宽高均为 `0` 表示关闭 Cover。 |
| `cover/color` | INTEGER | `0xAARRGGBB` 颜色，默认不透明黑色。 |
| `output/width`、`output/height` | INTEGER | 输出可见尺寸。 |
| `output/hstride`、`output/vstride` | INTEGER | 输出 stride；`0` 表示自动计算。 |
| `output/format` | INTEGER | V4L2 pixel format fourcc。 |
| `output/compression` | INTEGER enum | `linear` 或 `afbc-16x16`。 |
| `buffer-type` | INTEGER enum | `drm-noncached`、`drm-cached` 及对应 DMA32 类型。 |

`transform` 和 `cover` 是运行时可写原子对象；`output` 是停止状态下可写的重配置原子对象：

这里的停止状态明确指 `CREATED` 或 `stop()` 完成后的 `STOPPED`。`EOS`、`ABNORMAL`
状态下工作线程仍可能存在，必须先调用 `stop()` 再重配置输出或 Buffer 类型。

```cpp
processor->setParameter(
    "transform",
    ParameterObject({
        {"crop", {{"x", 100}, {"y", 50},
                  {"width", 1280}, {"height", 720}}},
        {"rotation", 90},
        {"mirror", true},
        {"flip", false},
    }));

processor->setParameter("buffer-type",
                        VideoBuffer::DRM_BUFFER_CACHEABLE);

processor->setParameter(
    "cover",
    ParameterObject({
        {"index", 0},
        {"crop", {
            {"x", 100},
            {"y", 80},
            {"width", 320},
            {"height", 180},
        }},
        {"color", static_cast<int64_t>(0x80ff0000U)},
    }));

processor->setParameter(
    "cover",
    ParameterObject({
        {"index", 1},
        {"crop", {
            {"x", 500},
            {"y", 200},
            {"width", 160},
            {"height", 90},
        }},
        {"color", static_cast<int64_t>(0xff00ff00U)},
    }));
```

最多可同时配置 16 个 Cover。`cover` getter 返回最近一次写入的槽位；切换 `index` 写入时
应提交包含 `index/crop/color` 的完整对象。Cover 在输出坐标系中绘制，可在模块运行期间更新。
将指定槽位的 `crop/width` 和 `crop/height` 同时设为 `0` 可关闭该 Cover；两者只有一个为 `0`
时返回 `-EINVAL`，矩形超出输出图像时返回 `-ERANGE`。

角色：处理组件，输入和输出均为 RAW 视频。

### ModuleInference

头文件：`module/vp/module_inference.hpp`

RKNN 模型推理模块，支持图像预处理和模型推理。

NPU 调度核心 `NPU_SCHEDULER_CORE`：

- `NPU_CORE_AUTO`
- `NPU_CORE_0`
- `NPU_CORE_1`
- `NPU_CORE_2`
- `NPU_CORE_0_1`
- `NPU_CORE_0_1_2`

| API | 说明 |
| --- | --- |
| `ModuleInference()` | 默认构造。 |
| `ModuleInference(const ImagePara& input_para)` | 指定输入图像参数。 |
| `setInferenceInterval(uint32_t frame_count)` | 设置推理帧间隔，默认 `0` 表示每帧推理。 |
| `setModelData(void* model, size_t model_size, NPU_SCHEDULER_CORE mask = NPU_CORE_AUTO)` | 初始化前或停止期间设置模型数据或模型路径；传路径时 `model_size` 为 `0`。 |
| `removeModel()` | 初始化前或停止期间移除模型。 |
| `setInputImageCrop(const ImageCrop& crop)` | 设置推理输入区域。 |
| `changedInputImagePara(const ImagePara& image_param)` | 修改输入图像参数。 |
| `getInputImageCrop()` / `getOutputImageCrop()` | 获取输入裁剪区域和模型等比例缩放后的输出区域。 |
| `getOutputMem()` | 获取推理输出内存。 |
| `getOutputAttr()` | 获取推理输出属性。 |
| `inference(std::shared_ptr<MediaBuffer> input_buffer)` | 初始化后手动执行一次推理。 |
| `init()` | 初始化模型和预处理链路。 |

注意：该类重载 `setBufferCount(uint16_t)` 为空实现。

## vo 输出模块

### ModuleAlsaPlayBack

头文件：`module/vo/module_alsaPlayBack.hpp`

ALSA 音频播放输出。

| API | 说明 |
| --- | --- |
| `ModuleAlsaPlayBack(const std::string& dev)` | 使用默认音频参数构造播放模块。 |
| `ModuleAlsaPlayBack(const std::string& dev, const SampleInfo& sample_info, AI_LAYOUT_E layout)` | 指定播放采样信息和通道布局。 |
| `changeDevice(const std::string& dev, const SampleInfo& sample_info, AI_LAYOUT_E layout)` | 停止状态下切换播放设备或音频参数。调用后需重新初始化模块。 |
| `init()` | 初始化 ALSA 播放设备；未手动提供有效 `SampleInfo` 时从音频输入通道读取。 |

角色：输出消费者，输入 PCM 音频并播放。

### DrmDisplayPlane

头文件：`module/vo/module_drmDisplay.hpp`

DRM 显示图层对象，可被多个 `ModuleDrmDisplay` 窗口共享。

图层类型 `PLANE_TYPE`：

- `PLANE_TYPE_OVERLAY`
- `PLANE_TYPE_PRIMARY`
- `PLANE_TYPE_CURSOR`
- `PLANE_TYPE_OVERLAY_OR_PRIMARY`

显示模式 `PLANE_DISPLAY_MODE`：

- `MULTI_WINDOW_DISPLAY`：多窗口共享一个图层，图层管理显存。
- `SINGLE_WINDOW_DISPLAY`：单窗口独占图层，图像直通显存。

布局模式 `LAYOUT_MODE`：

- `RELATIVE_LAYOUT`
- `ABSOLUTE_LAYOUT`

| API | 说明 |
| --- | --- |
| `DrmDisplayPlane(uint32_t fmt = V4L2_PIX_FMT_NV12, int screen_index = 0, uint32_t plane_zpos = 0xFF)` | 构造 DRM 图层。 |
| `setConnector(uint32_t conn_id)` | setup 前绑定显示器 connector id。 |
| `setup()` | 初始化图层资源。 |
| `setRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)` | 设置图层在屏幕上的区域。 |
| `getSize(uint32_t* w, uint32_t* h)` | 获取图层大小。 |
| `getScreenResolution(uint32_t* w, uint32_t* h)` | 获取屏幕分辨率。 |
| `setPlaneFullScreen()` / `restorePlaneFromFullScreen()` | 图层全屏和恢复。 |
| `setWindowLayoutMode(LAYOUT_MODE mode)` / `getWindowLayoutMode()` | 设置/获取窗口布局模式。 |
| `setWindowDisplayMode(PLANE_DISPLAY_MODE mode)` / `getWindowDisplayMode()` | 设置/获取图层窗口显示模式。 |
| `splitPlane(uint32_t w_parts, uint32_t h_parts)` | 将图层按横纵份数分割。 |
| `flushAllWindowRectUpdate()` | 刷新所有窗口区域更新。 |
| `setVisibility(bool isVisible)` | 设置图层可见性。 |

### ModuleDrmDisplay

头文件：`module/vo/module_drmDisplay.hpp`

DRM 显示输出模块。

| API | 说明 |
| --- | --- |
| `ModuleDrmDisplay(const ImagePara& input_para = ImagePara(), std::shared_ptr<DrmDisplayPlane> plane = nullptr)` | 构造 DRM 显示窗口，可传入共享图层。 |
| `setPlanePara(...)` | 设置图层格式、plane id、类型、zpos、linear 和 connector。提供多个重载。 |
| `move(uint32_t x, uint32_t y)` | 移动图层。 |
| `resize(uint32_t w, uint32_t h)` | 调整图层大小。 |
| `setPlaneRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)` | 设置图层位置和大小。 |
| `setWindowRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)` | 设置图层内窗口位置和大小。 |
| `setImageRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h)` | 设置输入图像显示区域。 |
| `setPlaneDisplayMode(DrmDisplayPlane::PLANE_DISPLAY_MODE mode)` | 初始化后设置图层显示模式。 |
| `getPlaneSize(uint32_t* w, uint32_t* h)` / `getWindowSize(uint32_t* w, uint32_t* h)` | 获取图层/窗口大小。 |
| `getScreenResolution(uint32_t* w, uint32_t* h)` | 获取屏幕分辨率。 |
| `setWindowVisibility(bool isVisible)` | 设置窗口可见性。 |
| `setWindowFullScreen()` / `restoreWindowFromFullScreen()` | 窗口铺满屏幕和恢复。 |
| `setWindowFullPlane()` / `restoreWindowFromFullPlane()` | 窗口铺满图层和恢复。 |
| `setWindowRelativeRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, bool sync = true)` | 设置相对布局区域；`sync=false` 时延迟到 `flushRelativeUpdate()` 生效。 |
| `flushRelativeUpdate()` | 应用延迟的相对区域更新。 |
| `init()` | 初始化显示资源；输入图像参数为空时从视频输入通道读取。 |

废弃接口：`getDisplayPlaneSize()`、`setPlaneSize()`、`setWindowSize()`，分别由 `getPlaneSize()`、`setPlaneRect()`、`setWindowRect()` 替代。

### ModuleFFmpegMux

头文件：`module/vo/module_ffmpegMux.hpp`

基于 FFmpeg 的输出封装模块，支持文件、网络等输出。

| API | 说明 |
| --- | --- |
| `ModuleFFmpegMux(const std::string& uri, const std::string& format)` | 构造输出封装器，`uri` 为输出地址，`format` 为封装格式。 |
| `changeSource(const std::string& uri, const std::string& format)` | 停止状态下切换输出地址和格式。调用后需重新初始化模块。 |
| `setFormatOption(const std::string& key, const std::string& value, int flags)` | 设置 FFmpeg format option；`key` 为空时清空全部选项。 |
| `getFormatOption(const std::string& key, int flags)` | 查询 format option。 |
| `setVideoParameter(int width, int height, media_codec_t type, uint32_t v4l2_fmt = 0)` | 初始化前设置视频参数；不设置则尝试从生产者获取。 |
| `setAudioParameter(int channel_count, int bit_per_sample, int sample_rate, media_codec_t type, SampleFormat sample_fmt = SAMPLE_FMT_NONE)` | 初始化前设置音频参数。 |
| `setExtraBuffer(const std::shared_ptr<MediaBuffer>& extra_buffer)` | 初始化前设置媒体参数及附加数据。 |
| `setInputBuffer(const std::shared_ptr<MediaBuffer>& input_buffer)` | 初始化后手动输入媒体数据。 |
| `init()` | 初始化输出封装器并准备写入；未手动配置的音视频轨道从输入通道读取参数和附加数据。 |

角色：输出消费者，输入编码后的音视频包并封装输出。

参数路径 `destination`、`ffmpeg/options`、`video` 和 `audio` 均属于
`RECONFIGURE`。它们只在 `CREATED` 或 `STOPPED` 且工作线程已经退出时接受修改；
成功后会关闭旧 FFmpeg 输出上下文并使初始化失效，调用方应重新执行 `init()`。

### ModuleFileWriter

头文件：`module/vo/module_fileWriter.hpp`

文件写入输出，支持裸流写入及 mp4、mkv、flv、ts、ps 等封装格式。

| API | 说明 |
| --- | --- |
| `ModuleFileWriter(const std::string& path)` | 构造文件写入器。 |
| `ModuleFileWriter(const ImagePara& para, const std::string& path)` | 指定视频参数构造文件写入器。 |
| `changeFileName(const std::string& file_name)` | 停止状态下切换输出文件名。调用后需重新初始化模块。 |
| `setVideoParameter(int width, int height, media_codec_t type)` | 初始化前设置视频参数；多流混合封装时各流应提前设置。 |
| `setAudioParameter(int channel_count, int bit_per_sample, int sample_rate, media_codec_t type)` | 初始化前设置音频参数。 |
| `setExtraBuffer(MEDIA_BUFFER_TYPE media_type, const std::shared_ptr<MediaBuffer>& extra_buffer)` | 设置指定媒体流附加数据。 |
| `setInputBuffer(const std::shared_ptr<MediaBuffer>& input_buffer)` | 手动写入媒体数据。 |
| `init()` | 初始化文件写入器；未手动配置的音视频参数和附加数据从输入通道读取。 |

角色：输出消费者，输入编码后的音视频包并写入文件。

参数路径 `path`、`video` 和 `audio` 均属于 `RECONFIGURE`。运行中、`EOS` 或
`ABNORMAL` 但工作线程尚未退出时返回 `-EBUSY`；停止后修改会释放旧 writer 并要求
重新执行 `init()`。旧的公开设置接口仍保留，并与编码路径串行访问 writer。

### ModuleGB28181Client

头文件：`module/vo/module_gb28181Client.hpp`

GB28181 客户端，支持向 GB28181 服务器推流。

| API | 说明 |
| --- | --- |
| `ModuleGB28181Client(const std::string& userId, const std::string& userAgent, SIP_TRANSPORT_TYPE transport = TRANSPORT_TYPE_TCP, int port = 0)` | 构造 GB28181 客户端。 |
| `setServerConfig(const std::string& serverId, const std::string& serverRealm, const std::string& serverIp, unsigned short serverPort = 5060, int expiry = 3600)` | 设置 SIP 服务器配置。 |
| `setAuthenticationInfo(const std::string& username, const std::string& password)` | 设置认证信息。 |
| `setDeviceInfo(const std::string& deviceName, const std::string& manufacturer, const std::string& channel)` | 设置设备信息和通道 ID。 |
| `setFirewallip(const std::string& firewallip)` | 设置 NAT 公网地址。 |
| `setAutoMasquerade(int automasquerade)` | 设置自动地址伪装。 |
| `setKeepaliveDuration(int seconds)` | 设置保活间隔。 |
| `init()` | 初始化并注册客户端；未手动指定视频格式时从视频输入通道读取编码信息。 |

角色：输出消费者，输入编码媒体数据并按 GB28181 协议推送。

### ModuleRendererVideo

头文件：`module/vo/module_rendererVideo.hpp`

OpenGL ES 视频渲染输出。窗口系统通过内部显示后端接口隔离：未设置 `FFMEDIA_DISPLAY_BACKEND` 时，自动选择wayland或x11。可通过 `FFMEDIA_DISPLAY_BACKEND=wayland` 或 `FFMEDIA_DISPLAY_BACKEND=x11` 显式指定后端。

| API | 说明 |
| --- | --- |
| `ModuleRendererVideo(const ImagePara para = ImagePara(), const std::string& title = "")` | 构造视频渲染窗口。 |
| `setWindowRect(int x, int y, uint32_t w, uint32_t h)` | 设置窗口位置和大小。 |
| `setImageRect(int x, int y, uint32_t w, uint32_t h)` | 设置图像在窗口中的显示区域。 |
| `setWindowVisibility(bool isVisible)` | 设置窗口可见性。 |
| `changeOutputResolution(int width, int height)` | 停止状态下修改输出分辨率并清除窗口。 |
| `init()` | 初始化显示后端及 EGL/GLES 渲染资源；输入图像参数为空时从视频输入通道读取。 |

角色：输出消费者，输入图像帧并渲染到窗口。

### ModuleRtmpServer

头文件：`module/vo/module_rtmpServer.hpp`

RTMP 服务器输出模块。

| API | 说明 |
| --- | --- |
| `ModuleRtmpServer(const char* path, int port)` | 构造 RTMP 推流服务。 |
| `ModuleRtmpServer(const ImagePara& para, const char* path, int port)` | 指定视频参数构造 RTMP 推流服务。 |
| `setMaxClientCount(int count)` / `getMaxClientCount()` | 设置/获取最大客户端连接数。 |
| `getCurClientCount()` | 获取当前客户端连接数。 |
| `setMaxTimeOutCount(int count)` / `getMaxTimeOutCount()` | 设置/获取最大超时次数。 |
| `setTimeOutSec(int sec, int usec)` | 设置超时时间。 |
| `init()` | 初始化 RTMP 服务；未手动指定视频格式时从视频输入通道读取编码信息。 |

角色：输出消费者，接收编码媒体数据并向 RTMP 客户端分发。

### ModuleRtspServer

头文件：`module/vo/module_rtspServer.hpp`

RTSP 服务器模块，支持 TCP 和 UDP 推流。一个实例可直接连接视频和音频生产者，
实现音视频同时推流；仅连接其中一种生产者时也可用于纯视频或纯音频推流。

| API | 说明 |
| --- | --- |
| `ModuleRtspServer(const std::string& path, int port)` | 构造 RTSP 服务轨道。 |
| `ModuleRtspServer(const ImagePara& para, const std::string& path, int port)` | 指定视频参数构造 RTSP 服务轨道。 |
| `setExtraBuffer(const std::shared_ptr<MediaBuffer>& extra_buffer)` | 设置媒体参数及附加数据，并创建媒体轨道。 |
| `setAuthInfo(const std::string& realm, const std::string& username, const std::string& password)` | 初始化前设置身份认证信息。 |
| `init()` | 初始化 RTSP 服务；未调用 `setExtraBuffer()` 时从输入通道创建音视频轨道并读取附加数据。 |

别名：`ModuleRtspServerVideoTrack`。

角色：输出消费者，用于单路或音视频组合 RTSP 推流。

## Python 绑定注意事项

Python 绑定由 `module/pymodule.cpp` 提供，名称通常与 C++ API 一致，但应注意：

- 数据 Hook 使用 `setMediaBufferConsumeHooker()` / `setMediaBufferProduceHooker()`，Python
  回调签名为 `(module_name, queue_size_or_index, media_buffer)`；仅能在 `start()` 前设置，
  返回值表示是否设置成功。
- 状态 Hook 使用 `setMediaStatusChangeHooker()`，回调签名为 `(module_name, status)`；同样
  仅能在 `start()` 前设置并返回设置结果。
- `setOutputDataCallback()`、`setStatusChangeCallback()` 已不再导出。
- `addExternalConsumer(name, callback)` 只接收名称和一个三参数媒体回调，并返回外部消费模块。
- `setProductor()` 仍是兼容接口但不返回连接结果；新代码使用 `connectProducer()`。
- `ModuleMppDec()` 默认使用空 `ImagePara`；`ModuleMppEnc(EncodeType)` 的 `input_para` 也有
  `ImagePara()` 默认值。两者都可在 `connectProducer()` 成功后从输入通道自动配置参数。
- `MediaBuffer.clone()` 返回独立载荷副本，适合在回调返回后继续使用；Python 不再提供缓冲池
  export/import 接口。

示例：

```python
import ff_pymedia as ff

def on_buffer(name, value, buffer):
    owned = buffer.clone()
    if owned is not None:
        print(name, owned.getActiveSize())

source = ff.ModuleFileReader("input.mp4", False)
if source.init() < 0:
    raise RuntimeError("failed to initialize source")

decoder = ff.ModuleMppDec()
if decoder.connectProducer(source) < 0:
    raise RuntimeError("failed to connect decoder")

decoder.setMediaBufferProduceHooker(on_buffer)
```

## 常见接入模式

### 源到处理到输出

```cpp
using namespace FFMedia;

auto cam = std::make_shared<ModuleCam>("/dev/video0");
cam->setOutputImagePara(
    ImagePara{1920, 1080, 1920, 1080, V4L2_PIX_FMT_NV12});
cam->init();

auto enc = std::make_shared<ModuleMppEnc>(MEDIA_CODEC_VIDEO_H264);
if (enc->connectProducer(cam) < 0)
    return -1;
enc->init();

auto writer = std::make_shared<ModuleFileWriter>("out.h264");
if (writer->connectProducer(enc, MediaChannelSelection({0})) < 0)
    return -1;

writer->init();

cam->start();
std::getchar();
cam->stop();
```

### 手动注入内存帧

```cpp
using namespace FFMedia;

ImagePara para{1280, 720, 1280, 720, V4L2_PIX_FMT_NV12};
auto mem = std::make_shared<ModuleMemReader>(para);
mem->init();

auto rga = std::make_shared<ModuleRga>(
    ImagePara{640, 360, 640, 360, V4L2_PIX_FMT_NV12},
    RGA_ROTATE_NONE);
if (rga->connectProducer(mem) < 0)
    return -1;

rga->init();
mem->start();

mem->setInputBuffer(frame_ptr, frame_size, frame_fd, pts_us);
mem->waitProcess(1000);
mem->setProcessStatus(ModuleMemReader::PROCESS_STATUS_EXIT);
mem->stop();
```

### 多通道源自动选择视频解码输入

```cpp
using namespace FFMedia;

auto source = std::make_shared<ModuleRtspClient>(
    "rtsp://example/live", RTSP_STREAM_TYPE_TCP, true, true);
if (source->init() < 0)
    return -1;

// ModuleMppDec 已声明压缩视频要求。即使 source 同时发布音频，
// connectProducer() 也只会连接匹配的 H264/H265/VP8/VP9 等视频通道。
auto decoder = std::make_shared<ModuleMppDec>();
if (decoder->connectProducer(source) < 0)
    return -1;
if (decoder->init() < 0)
    return -1;

MediaInputChannel input;
if (decoder->getInputMediaChannel(0, input)) {
    printf("producer=%s, output_channel=%u\n",
           input.producer_name.c_str(), input.producer_channel_id);
}

source->start();
std::getchar();
source->stop();
```

### 附加数据传递

编码、封装、RTSP/RTMP 等模块常需要 SPS/PPS、AAC config 等附加数据。生产者初始化输出
通道时会将附加数据保存在 `MediaChannelInfo::extra_data`；`connectProducer()` 保存完整通道
描述，消费者可在 `MediaInputChannel::media.extra_data` 中自动获取。

```cpp
auto encoder = std::make_shared<ModuleMppEnc>(MEDIA_CODEC_VIDEO_H264);
if (encoder->connectProducer(raw_source) < 0 || encoder->init() < 0)
    return -1;

auto muxer = std::make_shared<ModuleFFmpegMux>("output.mp4", "mp4");
if (muxer->connectProducer(encoder) < 0)
    return -1;

// init() 从已匹配输入通道读取 codec、图像参数和 extra_data。
if (muxer->init() < 0)
    return -1;
```

当前会自动读取输入通道附加数据的模块包括 `ModuleAacDec`、`ModuleFFmpegMux`、
`ModuleFileWriter` 和 `ModuleRtspServer`。其中封装和输出模块还会
结合输入通道的 `image_para` 或 `sample_info` 自动创建媒体轨道。

兼容和手动配置接口仍然保留：

- 输入解封装模块：`ModuleFileReader::getExtraBuffer()`、`ModuleFFmpegDemux::getExtraBuffer()`、`ModuleRtspClient::getExtraBuffer()`。
- 编码模块：`ModuleMppEnc::getExtraBuffer()`、`ModuleAacEnc::getExtraBuffer()`。
- 输出模块：`ModuleFileWriter::setExtraBuffer()`、`ModuleFFmpegMux::setExtraBuffer()`、`ModuleRtspServer::setExtraBuffer()`。

正常管线无需手动转发附加数据。只有没有通过 `connectProducer()` 建立通道、手动调用
`setInputBuffer()`，或需要覆盖通道自动配置时，才应在输出模块 `init()` 前调用
`setExtraBuffer()`。
