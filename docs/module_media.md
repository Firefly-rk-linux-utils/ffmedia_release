# FFMedia 核心媒体模块说明

本文档说明 `module/` 目录下构成 FFMedia 管线基础设施的四个核心类及多媒体通道模型：

| 核心类 | 文件 | 职责 |
| --- | --- | --- |
| `MediaConsumer` | `module/ff_media_consumer.hpp` | 定义下游接收媒体数据的最小接口。 |
| `MediaProducer` | `module/ff_media_producer.hpp` | 管理消费者、保存通道路由并向下游分发数据。 |
| `MediaHookable` | `module/ff_media_hookable.hpp` | 提供消费、生产和状态变化观察钩子。 |
| `ModuleMedia` | `module/module_media.hpp` | 组合生产、消费、钩子、线程、队列、缓冲池、通道匹配和生命周期管理。 |

相关的通道数据结构定义在 `module/media_channel.hpp`，实际媒体数据由
`base/media_buffer.hpp` 中的 `MediaBuffer` 承载。本文描述以当前实现为准。

## 1. 总体架构

`ModuleMedia` 同时继承 `MediaProducer`、`MediaConsumer` 和 `MediaHookable`：

```text
                         MediaHookable
                              |
                              | 数据/状态观察
                              v
MediaProducer <--------- ModuleMedia ---------> MediaConsumer
    |                         |                       |
    | consumers + routes     | 工作线程              | receiveMediaBuffer()
    | pushMediaBuffer()       | 输入队列              |
    |                         | 输出缓冲池             |
    +-------------------------+-----------------------+
                              |
                              v
                  具体 VI / VP / VO 派生模块
```

典型管线如下：

```text
源模块（SRC）              处理模块（PRC）              输出模块（PRC）
RTSP/File/Camera  ------>  Decoder/RGA/Encoder  ------> Display/Mux/Server
        生产                       消费 + 生产                     消费
```

一个生产者可以连接多个消费者，一个消费者也可以连接多个生产者。生产者输出通道与
消费者逻辑输入之间的匹配关系在连接时建立，运行时通过 `MediaBufferContext` 传递。

管线应保持为无环有向图。`start()`、`stop()` 和 `dumpPipe()` 都会递归访问下游，循环连接
会导致无限递归或生命周期问题。

## 2. 核心数据模型

### 2.1 `MediaBuffer`

`MediaBuffer` 是管线中实际传输的数据对象，可表示视频帧、音频帧、编码包或其他数据。
与本文相关的关键字段包括：

- 数据区域：`data/size` 表示完整内存，`active_data/active_size` 表示当前有效载荷。
- 媒体信息：媒体类型、编码类型、图像参数、音频采样参数和附加数据。
- 时间信息：PTS、DTS、EOS 和 flags。
- 通道标识：`media_channel_id` 表示生产者本地的输出通道 ID。
- 池状态：`STATUS_CLEAN` 表示可复用，`STATUS_DIRTY` 表示正在流转或被持有。
- 引用计数：记录生产者、下游消费者和外部持有者对 Buffer 的占用。

`std::shared_ptr<MediaBuffer>` 只负责 C++ 对象生命周期；FFMedia 自己的 `ref_count` 负责
判断池内 Buffer 是否可以重新用于下一次生产，两者不可混为一谈。

### 2.2 `MediaChannelInfo`

描述生产者发布的一路输出：

| 字段 | 说明 |
| --- | --- |
| `id` | 生产者内部唯一的输出通道 ID。 |
| `name` | 便于识别的通道名。 |
| `media_type` | `BUFFER_TYPE_VIDEO`、`BUFFER_TYPE_AUDIO` 或 `BUFFER_TYPE_ETC`。 |
| `codec` | 原始视频、H.264、H.265、AAC 等媒体编码类型。 |
| `image_para` | 视频宽高、stride 和像素格式。 |
| `sample_info` | 音频采样格式、声道数、采样率和采样数。 |
| `extra_data` | SPS/PPS、AAC config 等初始化附加数据。 |

发布视频和音频通道示例：

```cpp
MediaChannelInfo video;
video.id = 4;
video.name = "main-video";
video.media_type = BUFFER_TYPE_VIDEO;
video.codec = MEDIA_CODEC_VIDEO_H264;
video.image_para = ImagePara(1920, 1080, 1920, 1080, V4L2_PIX_FMT_H264);

MediaChannelInfo audio;
audio.id = 8;
audio.name = "audio";
audio.media_type = BUFFER_TYPE_AUDIO;
audio.codec = MEDIA_CODEC_AUDIO_AAC;
audio.sample_info.fmt = SAMPLE_FMT_S16;
audio.sample_info.channels = 2;
audio.sample_info.sample_rate = 48000;

setOutputMediaChannels({video, audio});
```

### 2.3 `MediaChannelRequirement`

描述消费者一个逻辑输入所接受的格式：

| 字段 | 说明 |
| --- | --- |
| `input_id` | 消费者内部的逻辑输入 ID。 |
| `name` | 逻辑输入名称。 |
| `media_type` | 所需媒体类型；`BUFFER_TYPE_ETC` 表示不限制。 |
| `codecs` | 允许的编码列表；空列表表示不限制。 |
| `pixel_formats` | 允许的视频像素格式；空列表表示不限制。 |
| `sample_formats` | 允许的音频采样格式；空列表表示不限制。 |
| `allow_multiple` | 是否允许多个生产者或输出通道占用该逻辑输入。 |

`matches()` 会依次检查媒体类型、编码、像素格式和音频采样格式。压缩视频解码输入
可以声明为：

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

### 2.4 选择、匹配结果和运行时上下文

`MediaChannelSelection` 用于限制一次连接可使用的生产者输出通道。`output_ids` 为空表示
选择全部输出，非空时其中每个 ID 都必须存在。

`MediaInputChannel` 是连接成功后的配置结果，包含：

- 消费者逻辑 `input_id`；
- 生产者 `producer_channel_id`；
- 生产者弱引用及名称；
- 被匹配输出通道的完整 `MediaChannelInfo`。

因此生产者发布在 `MediaChannelInfo::extra_data` 中的 SPS/PPS、AAC config 等附加数据，也会
随通道匹配结果保存在 `MediaInputChannel::media.extra_data`。消费者可在 `init()` 中直接从
`getInputMediaChannels()` 或 `getInputMediaChannel()` 读取，无需再次调用生产者的
`getExtraBuffer()`。

`MediaChannelRoute` 是 `MediaProducer` 分发 Buffer 时使用的轻量路由：

```text
producer_channel_id  ------>  consumer_input_id
```

`MediaBufferContext` 是送到消费者的运行时信封：

```cpp
struct MediaBufferContext {
    std::shared_ptr<MediaBuffer> buffer;
    MediaChannelId input_id;
};
```

`input_id` 属于连接和消费者，不属于共享 Buffer。这样同一个 Buffer 扇出到多个消费者时，
每个消费者都可以收到自己的逻辑输入 ID，而不需要修改 Buffer 元数据。

## 3. `MediaConsumer`

`MediaConsumer` 是最小消费接口：

```cpp
class MediaConsumer {
public:
    virtual void receiveMediaBuffer(const MediaBufferContext& context) = 0;
};
```

它不管理线程、队列或 Buffer 引用。自定义的纯 `MediaConsumer` 实现必须自行决定是否同步
处理、是否排队以及如何管理 Buffer 生命周期。

`ModuleMedia` 对该接口的实现会：

1. 锁定输入队列。
2. 检查队列容量，队列满时丢弃新数据。
3. 对非空 Buffer 增加 FFMedia 引用计数。
4. 将 `MediaBufferContext` 压入队列。
5. 唤醒模块工作线程。

默认输入队列容量为 `1024`，可通过 `setInputBufferQueueSize()` 修改。队列满时 Buffer
不会入队，也不会增加引用计数。

兼容接口 `receiveMediaBuffer(const std::shared_ptr<MediaBuffer>&)` 会用 Buffer 自身的
`media_channel_id` 初始化 `MediaBufferContext::input_id`。多生产者或通道路由场景应优先
使用带上下文的接口。

## 4. `MediaProducer`

### 4.1 消费者管理

`MediaProducer` 保存：

- `consumers`：消费者的 `shared_ptr` 列表，生产者会持有消费者生命周期。
- `consumer_routes`：以消费者对象地址为键的通道路由表。
- `productor_mutex`：保护消费者列表、路由和分发过程。

公开接口：

| API | 说明 |
| --- | --- |
| `addConsumer(consumer)` | 添加消费者，并使用 `ANY -> ANY` 的兼容路由。 |
| `addConsumer(consumer, routes)` | 添加消费者或替换该消费者已有的路由。 |
| `removeConsumer(consumer)` | 删除消费者及其路由。 |

同一个消费者重复添加不会在列表中产生重复项，但新路由会覆盖旧路由。

### 4.2 Buffer 分发

派生类通过受保护的接口分发数据：

```cpp
pushMediaBuffer(buffer, producer_channel_id);
```

分发过程为：

1. 遍历所有消费者。
2. 查找该消费者的路由表。
3. 找到第一条匹配当前生产者输出通道的路由。
4. 将消费者逻辑输入 ID 写入 `MediaBufferContext`。
5. 调用消费者的 `receiveMediaBuffer()`。

对标准 `ModuleMedia` 消费者而言，`pushMediaBuffer()` 只同步完成“入队”，真正的
`doConsume()` 在下游工作线程中异步执行。若直接实现 `MediaConsumer` 并在
`receiveMediaBuffer()` 内处理，处理逻辑则会运行在生产者调用线程中。

`MediaProducer::pushMediaBuffer()` 本身只负责路由，不会设置 Buffer 的池状态或初始引用
计数。标准 `ModuleMedia` 会在 `produceOneBuffer()` 中完成这些工作；绕过
`ModuleMedia` 直接实现生产者时，派生类必须自行建立一致的 Buffer 生命周期规则。

当前实现分发时持有 `productor_mutex`，因此自定义 `receiveMediaBuffer()` 应避免耗时、
反向修改同一生产者的消费者关系，或执行可能造成锁循环的操作。

## 5. `MediaHookable`

### 5.1 状态模型

`MediaStatus` 包含：

| 状态 | 含义 |
| --- | --- |
| `CREATED` | 对象已创建或内部状态已重置。 |
| `STARTED` | 工作线程已启动，或最近一次消费/生产成功。 |
| `EOS` | 消费或生产到达流结束。 |
| `STOPPED` | 已调用 `stop()` 并结束工作线程。 |
| `ABNORMAL` | `setup()`、消费或生产失败。 |

状态只有发生变化时才会调用状态钩子。

### 5.2 三类钩子

| API | 调用位置 |
| --- | --- |
| `setMediaBufferConsumeHooker()` | 工作线程取出输入 Buffer 后、调用 `doConsume()` 前。 |
| `setMediaBufferProduceHooker()` | `doProduce()` 完成且 Buffer 已标记为 `DIRTY`、初始引用计数设为 `1` 后，向下游分发前。 |
| `setMediaStatusChangeHooker()` | `MediaStatus` 发生变化时。 |

Buffer 钩子类型为：

```cpp
using MediaBufferHooker = std::function<void(
    const std::string& module_name,
    int value,
    std::shared_ptr<MediaBuffer> buffer)>;
```

第二个整数参数不要统一理解为“队列长度”：

- 消费钩子中是取出当前输入后的剩余队列长度。
- 生产钩子中当前实现传递的是前移后的输出环形队列头索引。
- `addExternalConsumer()` 回调中是外部消费模块当时的输入队列长度。

钩子在模块工作线程内同步执行，并且调用期间持有对应的 hook mutex。钩子应尽量短小，
不要在钩子内部重新设置同类钩子，也不要执行可能等待当前管线的阻塞操作。

生产钩子执行时 Buffer 已具有生产者的初始 FFMedia 引用，因此可以在回调返回前调用
`holdOutputBuffer()`。钩子仍运行在模块工作线程中，更适合做同步观察、日志或轻量数据处理；
耗时任务优先使用外部消费者，异步使用完成后调用 `releaseOutputBuffer()`。

## 6. `ModuleMedia`

### 6.1 模块类型

`ModuleType` 分为：

- `SRC`：源模块，不允许设置上游生产者，工作线程直接执行生产流程。
- `PRC`：处理模块，默认类型，等待输入后执行消费流程，并可继续生产输出。

源模块的派生类通常在构造函数中设置：

```cpp
module_type = ModuleType::SRC;
```

在 `SRC` 模块上调用 `connectProducer()` 会抛出 `std::invalid_argument`。

### 6.2 对象创建与生命周期

`ModuleMedia` 继承 `std::enable_shared_from_this`。连接、移除生产者和添加外部消费者等
操作会调用 `shared_from_this()`，因此模块必须由 `std::shared_ptr` 管理：

```cpp
auto module = std::make_shared<MyModule>();
```

不要把栈对象或尚未交给 `shared_ptr` 管理的对象接入管线。

推荐生命周期顺序：

1. 创建并配置源模块。
2. 调用源模块 `init()`，使其发布准确的输出通道信息。
3. 创建下游模块并声明输入要求。
4. 调用下游的 `connectProducer()` 完成匹配。
5. 调用下游 `init()`，让其使用匹配后的输入参数初始化。
6. 重复连接后续节点。
7. 从源节点调用 `start()`，递归启动整条下游管线。
8. 从源节点调用 `stop()`，递归停止整条下游管线。
9. 释放模块对象。

```text
producer.init()
      |
consumer.connectProducer(producer)
      |
consumer.init()
      |
source.start()  --->  递归启动下游
      |
source.stop()   --->  递归停止下游
```

`start()` 本身具有线程存在检查，同一节点被共享管线多次递归访问时不会重复创建工作线程。

当前 `module/vi`、`module/vp`、`module/vo` 中的具体 `ModuleMedia` 派生类都会在析构函数入口
调用 `stop()`，确保释放设备、编解码器、显示器等派生成员前，模块工作线程已经退出。
这是一层异常路径保护，正常流程仍应从源节点显式调用 `stop()`，以确定顺序递归停止整条
下游管线并及时观察停止错误或状态变化。

`ModuleMedia` 基类析构函数本身只重置缓冲回调，不调用 `stop()`。新增自定义派生类时，也应
在派生类析构函数的第一步调用 `stop()`；如果等到基类析构阶段，虚函数分派已经无法安全调用
派生类的 `teardown()`。

### 6.3 连接与通道匹配

推荐使用：

```cpp
int ret = consumer->connectProducer(producer);
int ret = consumer->connectProducer(producer, MediaChannelSelection({4, 8}));
```

`setProductor(producer)` 是兼容接口。无返回值版本会在内部记录连接错误日志，但调用方无法
可靠处理失败；新代码应使用返回 `int` 的 `connectProducer()`。

连接算法如下：

1. 检查生产者非空，并拒绝给 `SRC` 模块设置生产者。
2. 获取生产者的全部 `MediaChannelInfo`。
3. 若 selection 非空，先验证其中所有通道 ID 均存在。
4. 消费者未声明 requirement 时，把所有选中输出按“输出 ID -> 同值输入 ID”连接。
5. 声明了 requirement 时，按输出通道顺序查找第一项匹配且可占用的逻辑输入。
6. 生成 `MediaChannelRoute` 和 `MediaInputChannel`。
7. 把消费者和路由注册到生产者，并记录消费者侧的生产者弱引用。
8. 若兼容字段 `input_para` 尚未完整配置，用首个匹配视频输入的参数补齐。

匹配结果复制完整的 `MediaChannelInfo`，包括 `image_para`、`sample_info` 和 `extra_data`。
解码、封装、文件写入和流媒体输出模块可据此自动配置媒体参数与附加数据。

返回值：

| 返回值 | 含义 |
| --- | --- |
| `0` | 至少建立了一条有效路由。 |
| `-EINVAL` | 生产者为空。 |
| `-ENOENT` | selection 中包含生产者不存在的输出通道 ID。 |
| `-ENOTSUP` | 所选输出没有任何一路满足消费者要求。 |
| `-EBUSY` | 匹配到了 `allow_multiple=false` 且已被其他生产者占用的输入。 |
| 异常 | `SRC` 模块设置生产者时抛出 `std::invalid_argument`。 |

匹配规则的几个重要细节：

- 一个输出通道只会匹配第一项可用 requirement。
- `allow_multiple=false` 时，一个逻辑输入只接收一条匹配输出。
- 同一生产者重复连接会更新该消费者在生产者侧的路由和消费者侧的输入配置。
- `allow_multiple=true` 时，`getInputMediaChannel(input_id, ...)` 只能返回第一项；需要查看
  所有来源时应遍历 `getInputMediaChannels()`。
- `removeProductor(producer)` 移除指定上游；传入空指针会移除全部上游及输入配置。

### 6.4 新旧通道接口兼容关系

推荐多通道接口：

| 生产者接口 | 消费者接口 |
| --- | --- |
| `setOutputMediaChannels()` | `setInputMediaChannelRequirements()` |
| `addOutputMediaChannel()` | `addInputMediaChannelRequirement()` |
| `getOutputMediaChannels()` | `getInputMediaChannels()` |
| `getOutputMediaChannel()` | `getInputMediaChannel()` |

旧的单路视频接口仍然有效：

- `setOutputImagePara()` 会同步新增或更新 ID 为 `0`、名称为 `video` 的视频输出通道。
- 如果没有显式输出通道，`getOutputMediaChannels()` 会根据 `media_type` 和
  `output_para` 构造 ID 为 `0` 的兼容通道。
- `setOutputMediaChannels()` 会把第一个视频通道的图像参数同步到 `output_para`。
- 连接成功时，若 `input_para` 的宽、高或格式尚未完整设置，会从首个匹配视频通道同步；
  已手动设置完整参数时不会覆盖。

由于存在兼容回退，`clearOutputMediaChannels()` 只是清空显式通道列表；随后调用
`getOutputMediaChannels()` 仍可能得到由旧字段生成的默认通道。

### 6.5 工作线程和处理流程

每个 `ModuleMedia` 最多创建一个工作线程。

源模块流程：

```text
setup()
   |
waitProduceBuffer()
   |
doProduce(output)
   |
produceOneBuffer() -> 标脏/设引用 -> produce hook -> pushMediaBuffer()
   |
循环，直至 stop()
   |
clear cache -> clear input queue -> teardown()
```

处理模块流程：

```text
setup()
   |
等待 input_buffer_queue
   |
consume hook
   |
doConsume(input context, output buffer)
   |
按 ConsumeResult 决定是否调用 doProduce()
   |
produce hook -> pushMediaBuffer()
   |
释放输入 Buffer 引用并继续循环
```

输入队列等待超时时间当前固定为 5 秒，超时只记录日志并继续等待。输出池当前 Buffer 尚未
回收时，生产线程会等待其状态恢复为 `STATUS_CLEAN`。

派生模块可覆盖：

| 扩展点 | 用途 |
| --- | --- |
| `setup()` | 工作线程进入主循环前准备资源；返回 `false` 会进入 `ABNORMAL`。 |
| `teardown()` | 工作线程退出后释放线程相关资源。 |
| `doConsume(buffer, output)` | 兼容的单 Buffer 消费接口。 |
| `doConsume(context, output)` | 可识别逻辑输入 ID 的多通道消费接口。 |
| `doProduce(output)` | 填充或取得下一份输出数据。 |
| `initBuffer()` | 自定义缓冲池创建方式。 |
| `bufferReleaseCallBack()` | Buffer 引用归零、恢复为空闲前释放模块私有资源。 |

默认的上下文消费接口会转发到旧接口：

```cpp
ConsumeResult doConsume(const MediaBufferContext& input,
                        std::shared_ptr<MediaBuffer>& output) override
{
    return doConsume(input.buffer, output);
}
```

因此旧派生模块无需立即修改。需要区分音频、视频或多个生产者时，应覆盖上下文版本。

### 6.6 消费和生产结果

`ConsumeResult` 的主循环行为：

| 结果 | 行为 |
| --- | --- |
| `CONSUME_SUCCESS` | 本次消费成功，随后执行一次生产。 |
| `CONSUME_NEED_REPEAT` | 执行一次生产，但保留当前输入并再次消费。适合一个输入产生多份输出。 |
| `CONSUME_SKIP` | 丢弃本次输入，不生产。 |
| `CONSUME_BYPASS` | 不生产；若配置了外部消费回调，则调用该回调。 |
| `CONSUME_EOS` | 状态切换为 `EOS`，不生产。 |
| `CONSUME_FAILED` | 状态切换为 `ABNORMAL`，不生产。 |
| `CONSUME_WAIT_FOR_CONSUMER` | 已定义，但当前主循环没有专门处理，会进入默认错误分支。 |
| `CONSUME_WAIT_FOR_PRODUCTOR` | 已定义，但当前主循环没有专门处理，会进入默认错误分支。 |

`ProduceResult` 的主循环行为：

| 结果 | 行为 |
| --- | --- |
| `PRODUCE_SUCCESS` | 发布当前输出，然后回到正常循环。 |
| `PRODUCE_CONTINUE` | 发布当前输出，并立即再次生产。 |
| `PRODUCE_EMPTY` | 当前没有输出，不发布。 |
| `PRODUCE_BYPASS` | 发布当前输出，但不主动把状态切换为 `STARTED`。 |
| `PRODUCE_EOS` | 状态切换为 `EOS`，不发布。 |
| `PRODUCE_FAILED` | 状态切换为 `ABNORMAL`，不发布。 |

基类默认 `doConsume()` 和 `doProduce()` 都返回 `BYPASS`。默认消费的 bypass 不是把输入
原样转发到下游，而是仅用于 `addExternalConsumer()` 创建的回调消费模块。

### 6.7 输出缓冲池与引用计数

`ModuleMedia` 使用 `buffer_pool` 保存拥有的 Buffer，使用 `buffer_ptr_queue` 作为指向这些
Buffer 的环形输出队列。`buffer_count` 决定池大小：

- 视频压缩格式默认创建 `MALLOC_BUFFER`。
- 视频原始格式默认创建可缓存 DRM Buffer。
- 音频或其他类型默认创建普通 `MediaBuffer`。
- 显式设置 `buffer_size` 时按该大小申请；视频未设置时按 `output_para` 计算。

派生模块必须在初始化中配置有效池大小并调用 `initBuffer()`，否则生产线程无法取得有效的
输出 Buffer。

一次正常扇出的引用变化如下：

```text
produceOneBuffer()
  Buffer = DIRTY, ref = 1             # 生产者临时持有
            |
            +--> consumer A 入队，ref + 1
            +--> consumer B 入队，ref + 1
            |
  生产者发布结束，ref - 1
            |
  A 消费结束，ref - 1
  B 消费结束，ref - 1 -> 0
            |
  onRefZero() -> bufferReleaseCallBack()
              -> Buffer = CLEAN
              -> 唤醒等待生产者
```

这套机制允许多个下游共享同一 Buffer，避免为每个消费者复制数据，同时确保所有消费者
释放前生产者不会覆盖该内存。

外部代码需要在回调结束后继续使用输出 Buffer 时，应成对调用：

```cpp
if (ModuleMedia::holdOutputBuffer(buffer) == 0) {
    // 交给异步任务
    asyncUse(buffer, [buffer] {
        ModuleMedia::releaseOutputBuffer(buffer);
    });
}
```

注意：

- `holdOutputBuffer()` 只接受当前 FFMedia 引用计数大于 0 的 Buffer。
- 每次成功 hold 必须且只能对应一次 release。
- 只保存一份 `shared_ptr` 并不能阻止缓冲池复用，必须操作 FFMedia 引用计数。
- 重复 release、未 hold 直接 release 或长期不 release 会导致计数错误、数据覆盖或生产阻塞。

旧的 `exportBufferFromBufferPool()` 和 `importBufferToBufferPool()` 已从 `ModuleMedia` 及相关
派生模块移除。外部代码不应直接改变模块缓冲池成员关系；需要独立拥有载荷时使用
`MediaBuffer::clone()`，需要零拷贝短期持有池内 Buffer 时使用 `holdOutputBuffer()` /
`releaseOutputBuffer()`。

### 6.8 外部消费者

`addExternalConsumer(name, callback)` 会创建一个默认 `ModuleMedia` 作为当前模块的下游，
并把 callback 保存为该下游的外部消费函数：

```cpp
auto external = producer->addExternalConsumer(
    "analysis",
    [](const std::string& name, int queue_size,
       std::shared_ptr<MediaBuffer> buffer) {
        // 同步读取 Buffer
    });
```

它与普通钩子的主要区别是：

- 一个模块只有一组消费/生产钩子，但可以添加多个外部消费者。
- 外部消费者有自己的输入队列和工作线程，不在上游生产线程中执行 callback。
- 外部消费者参与正常引用计数和背压等待。
- 返回对象可用于设置队列大小、观察状态或主动管理连接，建议调用方保存。

### 6.9 调试接口

`dumpPipe()` 从当前节点递归打印：

- 模块名称；
- 已匹配输入及其生产者；
- 输入 requirement；
- 输出通道及视频、音频参数。

`dumpPipeSummary()` 打印每个节点的运行统计：

- `In Empty`：工作线程发现输入队列为空并进入等待的累计次数。
- `Out Full`：输出环形队列头尚未回收、生产线程进入等待的累计次数。

这两个计数是累计阻塞事件，不是当前队列长度。

## 7. 完整连接示例

以下示例展示 RTSP 多通道源、视频解码和显示的推荐配置顺序：

```cpp
#include <cstdio>
#include <memory>

#include "module/vi/module_rtspClient.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vo/module_drmDisplay.hpp"

using namespace FFMedia;

int main()
{
    auto source = std::make_shared<ModuleRtspClient>(
        "rtsp://host/path", RTSP_STREAM_TYPE_TCP, true, true);
    if (source->init() < 0)
        return 1;

    for (const auto& channel : source->getOutputMediaChannels()) {
        std::printf("channel=%u name=%s type=%d codec=%d\n",
                    channel.id, channel.name.c_str(),
                    channel.media_type, channel.codec);
    }

    auto decoder = std::make_shared<ModuleMppDec>();
    if (decoder->connectProducer(source) < 0)
        return 2;
    if (decoder->init() < 0)
        return 3;

    auto display = std::make_shared<ModuleDrmDisplay>();
    if (display->connectProducer(decoder) < 0)
        return 4;
    if (display->init() < 0)
        return 5;

    source->dumpPipe();
    source->start();
    std::getchar();
    source->stop();
    source->dumpPipeSummary();
    return 0;
}
```

`ModuleMppDec` 声明压缩视频 requirement，因此连接 RTSP 源时会自动忽略不匹配的音频
通道。显示模块再从解码器发布的原始视频通道获得图像参数。

## 8. 多输入消费示例

需要区分多个逻辑输入的模块应覆盖上下文版本：

```cpp
ConsumeResult doConsume(const MediaBufferContext& input,
                        std::shared_ptr<MediaBuffer>& output) override
{
    MediaInputChannel configured;
    if (!getInputMediaChannel(input.input_id, configured))
        return CONSUME_SKIP;

    if (configured.media.media_type == BUFFER_TYPE_VIDEO)
        return consumeVideo(input.buffer, output);

    if (configured.media.media_type == BUFFER_TYPE_AUDIO)
        return consumeAudio(input.buffer, output);

    return CONSUME_SKIP;
}
```

例如 Mux 模块可以声明两个 requirement：

```cpp
MediaChannelRequirement video;
video.input_id = 0;
video.name = "video";
video.media_type = BUFFER_TYPE_VIDEO;

MediaChannelRequirement audio;
audio.input_id = 1;
audio.name = "audio";
audio.media_type = BUFFER_TYPE_AUDIO;

setInputMediaChannelRequirements({video, audio});
```

随后可分别连接视频生产者和音频生产者。若每个逻辑输入只允许一路生产者，保持
`allow_multiple=false` 即可。

## 9. API 速查

### 9.1 连接和通道

| API | 建议时机 | 说明 |
| --- | --- | --- |
| `setProductor(std::shared_ptr<ModuleMedia>)` | 消费者 init 前 | 无返回值的兼容连接接口。 |
| `setProductor(producer, selection)` | 消费者 init 前 | 按 selection 连接并返回结果。 |
| `connectProducer(producer, selection)` | 消费者 init 前 | 推荐接口，匹配通道并建立上下游关系。 |
| `removeProductor(producer)` | 停止或重配置时 | 移除指定上游；空指针表示全部。 |
| `setOutputMediaChannels(channels)` | 生产者初始化时 | 替换全部显式输出通道。 |
| `addOutputMediaChannel(channel)` | 生产者初始化时 | 新增通道，相同 ID 时更新。 |
| `clearOutputMediaChannels()` | 切换媒体源时 | 清空显式输出通道。 |
| `getOutputMediaChannels()` | 生产者初始化后 | 查询全部输出，必要时生成兼容通道。 |
| `getOutputMediaChannel(id, channel)` | 生产者初始化后 | 按输出 ID 查询。 |
| `setInputMediaChannelRequirements(requirements)` | 建立连接前 | 替换全部输入要求。 |
| `addInputMediaChannelRequirement(requirement)` | 建立连接前 | 新增要求，相同 input ID 时更新。 |
| `getInputMediaChannelRequirements()` | 任意 | 查询全部输入要求。 |
| `getInputMediaChannels()` | 连接成功后 | 查询全部实际匹配结果。 |
| `getInputMediaChannel(input_id, channel)` | 连接成功后 | 查询指定逻辑输入的第一项匹配。 |

### 9.2 生命周期、队列和缓冲

| API | 建议时机 | 说明 |
| --- | --- | --- |
| `init()` | 启动前 | 派生模块初始化资源。 |
| `start()` / `stop()` | 管线配置完成后 | 递归启停当前节点及下游；具体派生模块析构时会自动调用 `stop()`。 |
| `setBufferCount()` / `setBufferSize()` | init 前 | 配置输出缓冲池。 |
| `getBufferCount()` / `getBufferSize()` | init 后 | 查询缓冲数量和单个缓冲大小。 |
| `getBufferFromIndex()` | init 后 | 查询池内 Buffer。 |
| `holdOutputBuffer()` / `releaseOutputBuffer()` | Buffer 有效期间 | 跨回调持有和释放 Buffer。 |
| `setInputImagePara()` / `getInputImagePara()` | init 前 / 查询 | 旧版单视频输入参数接口。 |
| `setOutputImagePara()` / `getOutputImagePara()` | init 前 / 查询 | 旧版单视频输出参数接口。 |
| `setInputBufferQueueSize()` | 启动前优先 | 配置输入队列上限。 |
| `getInputBufferQueueSize()` | 任意 | 查询输入队列上限。 |
| `receiveMediaBuffer(buffer)` | 内部或手动注入 | 按 Buffer 通道 ID 构造上下文并入队。 |
| `receiveMediaBuffer(context)` | 内部或手动注入 | 将带逻辑输入 ID 的上下文入队。 |
| `clearInputBufferQueue()` | 停止或重置时 | 释放尚未消费的输入引用。 |
| `getName()` | 任意 | 查询模块名称。 |
| `getModuleStatus()` | 任意 | 查询模块状态。 |
| `getMediaType()` | 任意 | 查询模块的媒体类型兼容字段。 |
| `setSynchronize()` | 启动前优先 | 保存音视频同步器，供具体派生模块使用。 |

### 9.3 钩子和诊断

| API | 说明 |
| --- | --- |
| `setMediaBufferConsumeHooker()` | 观察消费前数据。 |
| `setMediaBufferProduceHooker()` | 观察生产完成数据。 |
| `setMediaStatusChangeHooker()` | 观察状态变化。 |
| `addExternalConsumer()` | 添加具有独立队列和线程的回调消费者。 |
| `dumpPipe()` | 打印拓扑和通道配置。 |
| `dumpPipeSummary()` | 打印输入空等待和输出满等待统计。 |

## 10. 使用建议与常见问题

### 连接成功但没有数据

依次检查：

1. 是否先初始化生产者，使输出通道已经发布。
2. `connectProducer()` 是否返回 `0`。
3. 派生生产者是否给每个 Buffer 设置了正确的 `media_channel_id`。
4. 是否从源节点调用了 `start()`。
5. `doConsume()` / `doProduce()` 是否返回了预期结果。
6. 输入队列是否满而持续丢帧。

### 通道匹配失败

- `-ENOENT`：检查 selection 中的 ID 是否来自 `getOutputMediaChannels()`。
- `-ENOTSUP`：比较媒体类型、codec、像素格式和采样格式。
- `-EBUSY`：检查同一逻辑输入是否已被其他生产者占用，必要时设置
  `allow_multiple=true` 或使用不同 `input_id`。

### 管线长时间卡在输出

使用 `dumpPipeSummary()` 检查 `Out Full`。常见原因包括：

- 外部代码 hold 后没有 release。
- 某个消费者阻塞或没有完成输入引用释放。
- 输出缓冲池太小，不足以覆盖下游处理延迟。

### 回调中异步使用数据后内容变化

只保存 `shared_ptr` 不会阻止池内存复用。应在 Buffer 引用有效的消费回调或外部消费者
回调中调用 `holdOutputBuffer()`，异步任务完成后调用 `releaseOutputBuffer()`。

### 推荐实践

- 新代码优先使用 `connectProducer()` 和多通道接口。
- 在生产者 `init()` 后、消费者 `init()` 前建立连接。
- 正常退出时仍从源节点显式调用 `stop()`；派生析构中的自动停止只作为安全兜底。
- 用 `MediaBufferContext::input_id` 区分消费者输入，不要改写共享 Buffer 来表达消费者路由。
- Hook 用于轻量同步观察；耗时处理使用正式下游模块或外部消费者。
- 所有成功的 hold 都必须有明确、唯一的 release 路径。
- 启动前完成拓扑配置，运行期间避免动态修改消费者和通道路由。
- 调试时同时查看 `dumpPipe()` 的匹配结果和 `dumpPipeSummary()` 的阻塞统计。
