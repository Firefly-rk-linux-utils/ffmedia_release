# FFMedia API

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

`<ffmedia/ffmedia.hpp>` 是稳定的聚合头，但其中的音频、FFmpeg、OpenGL 和推理模块会根据
构建选项条件包含。使用可选模块前，应确认 SDK 配置中的 `FFMedia_WITH_AUDIO`、
`FFMedia_WITH_FFMPEG`、`FFMedia_WITH_OPENGL` 和 `FFMedia_WITH_INFERENCE`，并链接同一构建
产生的库；未启用的组件不会出现在安装头文件或导出符号中。

编译和部署时必须同时匹配 `FFMEDIA_MODULE_ABI_VERSION`、运行期
`ffmedia_module_abi_version()`、`libff_media.so.<abi>` SONAME，以及
`FFMEDIA_GLIBCXX_USE_CXX11_ABI`。不要混用不同 SDK 包中的头文件、CMake 元数据和动态库。

## 阅读约定

- 头文件路径以安装 SDK 的 `include/ffmedia/` 为准；源码树中的相对路径仅用于定位实现。
- 除非另有说明，返回 `0` 表示成功，负 errno 表示失败；查询类布尔接口返回 `true` 表示找到或可用。
- “初始化前”包含 `CREATED` 状态；“停止后”表示 `stop()` 返回且状态为 `STOPPED`。
- 模块参数属于独立的路径 API，完整说明见 [FFMedia 模块参数系统](ffmedia_parameters.md)。
- 旧接口保留用于兼容已有程序；新程序优先使用带返回值的 `connectProducer()` 和参数系统。

## 典型生命周期

应用通常按以下顺序使用模块：

```text
创建模块 -> 设置参数 -> 初始化生产者 -> connectProducer() -> 初始化消费者 -> start() -> stop()
```

实际连接顺序应先初始化生产者以发布输出通道，再连接并初始化消费者。配置或连接失败时，
应检查返回的负 errno，不要继续启动不完整的管线。

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

#### MediaChannelSelection、MediaChannelRoute 和 MediaBufferContext

| 类型/API | 说明 |
| --- | --- |
| `MEDIA_CHANNEL_ID_DEFAULT` | 默认通道 ID，值为 `0`。 |
| `MEDIA_CHANNEL_ID_ANY` | 通道未指定时使用的特殊值，值为 `UINT32_MAX`。 |
| `MediaChannelSelection()` | 默认选择生产者的全部输出通道。 |
| `MediaChannelSelection({ids...})` | 只选择列出的生产者输出通道。 |
| `accepts(id)` | 判断一个生产者通道是否被本次选择接受。 |
| `MediaChannelRoute(producer_id, input_id)` | 创建一条显式的生产者到消费者输入路由。 |
| `MediaBufferContext` | 消费入口参数，包含 `buffer` 和消费者逻辑 `input_id`。 |

#### 连接、匹配和输入配置

- `MediaChannelSelection::output_ids` 指定本次连接可选择的生产者输出通道；列表为空表示选择生产者全部输出通道。
- `MediaChannelRoute` 的 `producer_channel_id` 和 `consumer_input_id` 分别表示生产者输出通道
  与消费者逻辑输入；`MEDIA_CHANNEL_ID_ANY` 表示由连接逻辑自动选择。
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

### MediaProducer / MediaConsumer

头文件：`module/ff_media_producer.hpp`、`module/ff_media_consumer.hpp`

这两个基类描述底层生产/消费关系，通常由 `ModuleMedia` 统一使用，应用一般不需要直接继承。

| API | 说明 |
| --- | --- |
| `MediaProducer::addConsumer(std::shared_ptr<MediaConsumer>)` | 添加消费者并使用通配路由；通道匹配通常由 `ModuleMedia::connectProducer()` 预先完成。 |
| `MediaProducer::addConsumer(std::shared_ptr<MediaConsumer>, routes)` | 按 `MediaChannelRoute` 显式指定生产者通道到消费者输入的路由。 |
| `MediaProducer::removeConsumer(std::shared_ptr<MediaConsumer>)` | 移除消费者。 |
| `MediaConsumer::receiveMediaBuffer(MediaBufferContext&&)` | 消费者接收入口；`MediaBufferContext` 同时携带 Buffer 和逻辑输入 ID。 |

`MediaProducer` 的 `pushMediaBuffer()`、快照回收和 `MediaConsumer` 的接收入口属于模块实现侧接口，
不应由应用直接调用或依赖其内部快照实现。生产者向多个消费者分发时，每个消费者收到同一帧的
独立 `MediaBufferContext`，但载荷通常仍由 `shared_ptr` 共享。

### Synchronize

头文件：`base/ff_synchronize.hpp`

`Synchronize` 提供音视频或绝对时钟同步。所有时间戳和延迟单位均为微秒；`updateVideo()` 返回
正数表示应等待的时长，返回 `0` 表示立即显示，负数表示帧过晚可丢弃。

`Synchronize::Clock` 包含 `start_time` 和 `current_pts` 两个微秒字段。`TimeSource` 的签名为
`std::function<int64_t()>`；未覆盖时使用单调时钟。

同步类型：

- `SYNCHRONIZETYPE_VIDEO`
- `SYNCHRONIZETYPE_AUDIO`
- `SYNCHRONIZETYPE_ABSOLUTE`

| API | 说明 |
| --- | --- |
| `Synchronize(SynchronizeType type)` | 创建指定主时钟类型的同步器。 |
| `getCurrentTime()` | 获取同步器时间源的当前微秒值。 |
| `reset()` | 清除时钟、首帧和统计状态。 |
| `getMasterClock()` / `getMasterTime()` | 获取主时钟结构或当前主时钟值。 |
| `setClockTime(type, pts)` / `getClockTime(clock)` | 设置指定时钟的 PTS，或读取时钟相对当前时间的值。 |
| `setMasterType(type)` / `getMasterType()` | 设置/查询主时钟类型。 |
| `setRefrshS(max_us, min_us)` | 设置视频最大等待时长和过晚帧返回值。接口名称保留原拼写。 |
| `setFirstFrameDuration(duration_us)` | 设置首帧默认持续时间。 |
| `setDiscontinuityThreshold(threshold_us)` | 设置时间戳或墙钟跳变阈值，超过阈值会重建时间线。 |
| `setAudioCorrection(gain, max_ratio, deadband_us)` | 配置渐进式音频速率校正。`gain` 和 `max_ratio` 范围为 `0..1`。 |
| `setTimeSource(source)` | 覆盖单调时钟源，适合仿真和确定性测试。 |
| `updateVideo(pts, duration)` | 根据视频 PTS 和帧时长计算等待/丢帧结果。 |
| `updateAudio(samples, samplerate, pts)` | 根据音频采样数、采样率和 PTS 计算播放修正。 |
| `updateAudioWithDeviceClock(...)` | 使用播放设备已排队采样数和设备运行状态更新音频时钟。 |
| `updateAudioByBytesSize(bytes, samplerate, channels, bits_per_sample, pts)` | 按 PCM 字节数更新音频时钟。 |
| `getLastVideoDifference()` / `getLastAudioDifference()` | 获取最近一次视频/音频时钟差值。 |
| `getVideoDropCount()` / `getDiscontinuityCount()` | 获取视频丢帧数和时间线重建次数。 |

将同步器传给 `ModuleMedia::setSynchronize()` 后，由具体播放或网络模块在消费路径中使用；同步器
对象应在所有使用它的模块停止后再释放。

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
| `static holdOutputBuffer(const std::shared_ptr<MediaBuffer>& obuf)` | 不建议使用 | Deprecated no-op，始终返回 `0`，不保存引用；直接保留 `shared_ptr`。 |
| `static releaseOutputBuffer(const std::shared_ptr<MediaBuffer>& obuf)` | 不建议使用 | Deprecated no-op，始终返回 `0`，不释放引用；直接重置或释放 `shared_ptr`。 |
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
| `getMetrics(MediaMetrics& out)` | 任意 | 无锁获取 best-effort 运行统计快照；失败返回负 errno。 |
| `resetMetrics()` | 建议停止后 | 清零运行统计，不改变模块配置、队列或 Buffer 池。 |
| `getMediaType()` | 初始化后 | 获取模块的兼容媒体类型字段。 |
| `setSynchronize(std::shared_ptr<Synchronize> syn)` | 任意 | 设置音视频同步对象。 |
| `addExternalConsumer(const std::string& name, MediaBufferHooker cb)` | 任意 | 添加外部消费者回调，返回一个外部消费模块。 |
| `dumpPipe()` / `dumpPipeSummary()` | 调试 | `dumpPipe()` 打印管线结构及各节点的输入通道、输入要求和输出通道完整参数；`dumpPipeSummary()` 打印运行统计信息。 |
| `receiveMediaBuffer(const std::shared_ptr<MediaBuffer>& buffer)` | 内部/手动输入 | 向模块输入队列压入缓冲。 |
| `receiveMediaBuffer(MediaBufferContext&& context)` | 内部/手动输入 | `final` 接收入口；向模块输入队列 move 一个带逻辑输入 ID 的上下文。派生类覆盖 `doConsume()`，不得覆盖此入口。 |
| `clearInputBufferQueue()` | 停止或重置时 | 清理未消费输入缓冲。 |
| `setInputBufferQueueSize(size_t size)` / `getInputBufferQueueSize()` | 任意 | 设置/获取输入队列容量，默认 `1024`，满队列时新缓冲会被丢弃。 |

### MediaMetrics

头文件：`base/media_metrics.hpp`

`MediaMetrics` 是 `ModuleMedia::getMetrics()` 返回的独立快照，不会改变 `MediaBuffer`、
`MediaChannelInfo` 或 `ModuleMedia` 的既有布局。指标关闭时只保留当前队列深度，不更新计数和高水位。

| 字段 | 说明 |
| --- | --- |
| `input_frames` / `output_frames` | 已进入工作线程的非空输入和已发布的帧数；队列满拒绝只计入输入丢帧。 |
| `queue_depth` / `queue_peak_depth` | 当前输入队列深度和统计周期内峰值。 |
| `processing_samples` / `processing_time_new_usec` / `processing_time_avg_usec` / `processing_time_max_usec` | `doConsume()` 与 `doProduce()` 合并后的样本数、最新耗时、平均耗时和最大耗时；同一次处理中的 consume 及其后续 produce 记为一个样本，不包含等待输出 Buffer 池的时间，源模块空轮询和 EOS 轮询不计入样本。 |
| `buffer_pool_exhausted` | 等待已配置输出 Buffer 池超时或停止时无可用 Buffer 的次数。 |
| `input_dropped_frames` / `processing_dropped_frames` / `output_dropped_frames` | 按输入队列、处理结果和输出发布阶段划分的丢帧数；三者之和为总丢帧数。 |
| `error_count` | 处理、输出、启动或回调异常次数。 |

默认参数 `metrics/enabled=true`。设置为 `false` 后停止更新计数器和高水位，队列当前深度仍会
保持可查询；重新启用后继续累积。建议在模块停止后调用 `resetMetrics()` 获取确定性结果。调用方应使用
默认构造函数初始化 `size/version`；库会按 `size` 只复制兼容的前缀，未知版本返回 `-ENOTSUP`。当前
结构版本为 `1`。

#### 派生类开发

`ModuleMedia` 的派生类接口、输出池提交、资源代次和配置锁要求属于开发指南，而不是应用层
API 的一部分。请参阅 [ModuleMedia 派生模块开发指南](module_media.md)。

### MediaParameter

头文件：`module/ff_media_parameter.hpp`

`MediaParameter` 提供统一的参数查询、读取和写入 API。参数路径、类型、对象事务、权限、
生命周期和并发规则详见 [FFMedia 模块参数系统](ffmedia_parameters.md)。

应用层最常用的接口是 `queryParameters()`、`queryParameter()`、`getParameter()`、
`getParameterAsString()`、`setParameter()` 和 `setParameterFromString()`。模块参数根路径
索引也集中维护在参数系统文档中。

| API | 说明 |
| --- | --- |
| `hasParameter(path)` | 判断路径是否存在。 |
| `queryParameter(path, info)` | 查询单个叶子或 OBJECT 节点的完整元数据；成功返回 `0`。 |
| `queryParameters(parent_path = "")` | 枚举根节点或指定 OBJECT 的直接子节点。 |
| `parameterRevision()` | 获取实例参数版本；每次成功写入递增。 |
| `getParameter(path, value)` | 按 `ParameterValue`、`bool`、`int64_t`、`double`、`std::string` 或 `ParameterObject` 读取。 |
| `getParameterAsString(path, value)` | 将参数按 schema 类型转换为字符串读取。 |
| `setParameter(path, value)` | 按上述 C++ 类型写入；OBJECT 支持完整对象或部分 patch。 |
| `setParameterFromString(path, value)` | 从字符串解析并写入，枚举支持名称或整数文本。 |

失败时读取/写入接口返回负 errno；参数不存在、类型不匹配、越界、只读或当前状态不允许写入
时不会修改已有值。参数对象的原子提交、跨字段校验和并发规则以
[FFMedia 模块参数系统](ffmedia_parameters.md) 为准。

自定义模块可包含 `module/ff_media_parameter_helpers.hpp`，使用 `imageParaParameter()`、
`imageCropParameter()`、`sampleInfoParameter()` 及对应的 `ParameterObject` 转换函数声明常见
结构化参数；binding 辅助模板和原子对象规则见参数系统文档及派生模块开发指南。

## 公共类型与枚举

### 图像与音频参数

头文件：`base/pixel_fmt.hpp`、`base/ff_type.hpp`

| 类型 | 字段/API | 说明 |
| --- | --- | --- |
| `ImagePara` | `width`、`height` | 可见图像尺寸，单位像素。 |
| `ImagePara` | `hstride`、`vstride` | 水平/垂直 stride；硬件缓冲可能大于可见尺寸。 |
| `ImagePara` | `v4l2Fmt` | V4L2 pixel format fourcc。 |
| `ImagePara` | `compression` | `ImageCompression::Linear` 或 `Afbc16x16`。 |
| `ImagePara` | `dump()` | 将当前图像尺寸、stride、格式和压缩方式输出到 FFMedia 日志。 |
| `ImageCrop` | `x`、`y`、`w`、`h` | 图像区域；坐标和尺寸均为像素。 |
| `SampleInfo` | `fmt`、`channels`、`sample_rate`、`nb_samples` | 音频采样格式、声道数、采样率和单帧采样数。 |
| `Rect` | `x`、`y`、`w`、`h` | DRM 窗口/图层区域；定义在 `module/vo/module_drmDisplay.hpp`。 |

常用像素格式工具包括 `v4l2GetFmtName()`、`drmGetFmtName()`、`v4l2ToDrmFormat()`、
`v4l2GetFrameSize()`、`v4l2GetFmtByName()`、`getCenterCrop()`、`getLetterboxCrop()` 和
`v4l2fmtIsCompressed()`。这些函数只负责格式/尺寸计算，不会修改模块配置；硬件格式支持
仍取决于具体模块和目标平台。

### 编解码和运行时枚举

`base/ff_type.hpp` 定义 `DecodeType`、`EncodeType`、`media_codec_t`、`EncodeRcMode`、
`EncodeProfile`、`EncodeQuality`、`RgaRotate`、`SampleFormat`、`AI_LAYOUT_E` 和
`SIP_TRANSPORT_TYPE`。枚举值可直接用于 C++ API；参数系统中的枚举参数则应优先使用
`queryParameter()` 返回的 `enum_values`，避免依赖数值顺序。

`ModuleType`（`SRC`、`PRC`）用于标识模块角色，`BUFFER_TYPE_VIDEO`、`BUFFER_TYPE_AUDIO` 和
`BUFFER_TYPE_ETC` 用于标识 Buffer/通道媒体类型。`ModuleMedia::getMediaType()` 返回的是兼容
字段；多通道模块应以 `getOutputMediaChannels()` 的每路 `media_type` 和 `codec` 为准。

### 错误与线程规则

配置、连接和初始化接口通常返回 `0` 或负 errno；旧的 `void` setter 无法报告失败，建议新代码
使用对应的参数系统或带返回值的新接口。模块工作线程会同步执行消费/生产 Hook 和应用处理回调，
回调应快速返回；耗时任务应移交应用自己的线程或队列。除头文件明确说明外，模块对象不可复制，
应通过 `std::shared_ptr` 和 `makeMediaModule()` 管理生命周期。

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
| `copyMetadata(const MediaBuffer& other)` | 复制帧元数据，不复制载荷、资源句柄、引用计数或回调。 |
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
| `fillWithColor(r, g, b)` / `fillWithColor(r, g, b, x, y, w, h)` | 填充整帧或指定矩形为 RGB 颜色；仅适用于支持的线性图像格式。 |
| `initWithExternalBuffer(void* data, size_t size, int fd)` | 使用外部内存和 fd 初始化缓冲。 |
| `copyMetadata(const MediaBuffer& other)` | 复制帧元数据，不复制载荷、资源句柄、引用计数或回调。 |

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

`AppSourceOptions` 的 `queue_capacity` 默认是 `4`，`queue_policy` 可选 `BLOCK`、`DROP_NEWEST`
和 `DROP_OLDEST`。`AppProcessorOptions::output_buffer_count` 默认是 `4`。

`ModuleAppProcessor` 是同步回调处理节点。回调在模块自己的工作线程中执行，可以返回：

- `FORWARD`：零拷贝转发当前 FFMedia Buffer；
- `REPLACE`：用应用提供的 `AppFrame` 或 `MediaBuffer` 替换输入；
- `DROP`：丢弃当前输入；
- `ERROR`：将模块切换为异常状态。

应用处理回调通过 `AppProcessResult::forward()`、`replace()`、`drop()` 和 `error()` 构造返回值；
`forward()` 可指定输出通道，`replace()` 可提交 `AppFrame` 或已有 `MediaBuffer`，`error()` 接受
负 errno 作为错误码。

`AppProcessResult` 包含 `action`、`output` 和 `error_code` 三个字段；`output` 仅在
`REPLACE` 时使用，`error_code` 仅在 `ERROR` 时使用。未显式返回结果时不应依赖默认的 `DROP`。

输入 Buffer 默认只读。由于输入可能同时发送给多个下游，应用不应在回调中原地修改共享
载荷。需要修改数据时应返回 `REPLACE`；外部替换载荷必须使用 `HOLD_OWNER` 或 `COPY`。

`AppFrame` 的 `channel_id` 默认是 `MEDIA_CHANNEL_ID_ANY`；使用裸内存时填写 `data`、`size`
和可选 `dmabuf_fd`，使用已有 Buffer 时填写 `buffer`。`pts_us`/`dts_us` 单位为微秒，`owner`
用于 `HOLD_OWNER` 模式保持外部资源存活。`submit()` 成功后若返回了 ticket，应调用 `wait()`
确认所有下游已经释放该帧；超时、队列策略丢弃和 flush 的返回值均应由调用方处理。

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
| `setMaxTimeOutCount(int count)` | 已废弃的兼容接口，不改变当前超时策略。 |
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

输入通道由 `setModuleStackParams()` 根据 `input_id` 自动创建或更新 requirement，使用标准 `connectProducer()` 和 `removeProductor()` 管理连接。角色：多输入处理组件，内部使用线程池和 RGA 更新拼接缓存；每个启用通道只保留一个在途处理任务，忙碌时跳过新帧。定时线程通过 Clock Buffer 传递单调时钟 PTS，`doConsume()` 收到时钟后直接生成并输出快照。运行期参数和输入布局配置见 [模块参数系统](ffmedia_parameters.md)。

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

参数路径、默认值、枚举和生效时机见 [模块参数系统](ffmedia_parameters.md)。`decode-type`
默认使用 `auto` 并根据输入格式 fourcc 选择解码类型，也可在初始化前显式指定。普通解码配置
与 `init()` 使用同一模块锁，运行期 timeout 使用原子快照。

输入要求会自动匹配 MPEG1、MPEG2、MPEG4、H264、H265、VP8、VP9、MJPEG 压缩视频通道，不会连接同一生产者的音频通道。输出通道为 ID `0` 的 RAW 视频。

### ModuleMppEnc

头文件：`module/vp/module_mppenc.hpp`

MPP 视频编码模块，支持 H264、H265、MJPEG。

| API | 说明 |
| --- | --- |
| `ModuleMppEnc(media_codec_t type, int fps = 30, ...)` / `ModuleMppEnc(EncodeType type, const ImagePara& input_para = ImagePara(), ...)` | 构造视频编码器；支持 H264、H265、MJPEG，推荐使用 `media_codec_t`，并保留 `EncodeType` 兼容接口。`media_codec_t` 接口的输入图像参数由匹配输入通道自动配置。 |
| `setDuration(int64_t duration)` | 设置输出时间戳间隔，单位微秒；小于等于 `0` 时使用输入时间戳。默认为0。 |
| `changeEncodeParameter(media_codec_t type, ...)` | 停止状态下修改编码类型、帧率、GOP、码率、码控模式、质量系数和 profile。调用后需重新初始化模块。 |
| `setIntraRefresh(bool intra_refresh, int refresh_mode, int refresh_num)` | 设置帧内刷新；`refresh_mode` 为 `0` 行刷新、`1` 列刷新。停止状态下存在编码器时立即下发，运行中在下一帧编码前应用。 |
| `setSeiUserData(const std::vector<uint8_t>& payload, const std::vector<uint8_t>& uuid = {})` | 设置后续编码帧携带的自定义 unregistered SEI 用户数据；`payload` 不含 SEI 头，`uuid` 为空时使用 MPP 默认 UUID，否则必须为 16 字节。每次调用会替换后续帧配置，空 `payload` 表示清除当前自定义数据。 |
| `setIdrFrame()` | 设置下一帧为关键帧（IDR）。 |
| `setOutputTimeOut(int timeout_ms)` | 设置取编码输出超时时间。 |
| `setInputCachePoolSize(int size)` | 设置输入缓存池大小，必须小于生产者输出缓冲区数量，默认 `1`。 |
| `getExtraBuffer()` | 初始化后获取编码附加数据。 |
| `init()` | 初始化 MPP 编码器。 |

角色：处理组件，输入原始图像帧，输出压缩视频码流。

编码器参数路径和生命周期见 [模块参数系统](ffmedia_parameters.md)。`duration-usec` 从 `0`
切换为正值时，下一帧会以 MPP 返回的当前输入 PTS 重基，之后按配置间隔递增。

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

参数化的裁剪、旋转、镜像、Cover、混合和输出配置，以及各字段的范围、枚举和生效时机，
统一见 [FFMedia 模块参数系统](ffmedia_parameters.md) 的模块参数索引。

| API | 说明 |
| --- | --- |
| `ModuleImageProcessor()` | 默认构造；输入和输出参数可在连接后或通过参数系统配置。 |
| `ModuleImageProcessor(const ImagePara& output_para)` | 指定输出图像参数。 |
| `ModuleImageProcessor(const ImagePara& input_para, const ImagePara& output_para)` | 指定输入和输出图像参数。 |
| `init()` | 初始化 EGL/OpenGL ES 图像处理资源。 |

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

注意：该类重载 `setBufferCount(uint16_t)` 为空实现。参数路径 `input/*`、`model/*` 和
`interval` 见 [模块参数系统](ffmedia_parameters.md)。

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
| `setPlanePara(fmt, plane_id, plane_type, plane_zpos, plane_linear, conn_id)` | setup 前一次性设置图层格式、plane、类型、zpos、线性布局和 connector。 |
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
| `setDrmSyncEnabled(bool enabled)` / `isDrmSyncEnabled()` | 设置/获取单窗口输出是否在提交帧后等待 DRM vblank。多窗口模式不使用此模块级开关。 |
| `getPlaneSize(uint32_t* w, uint32_t* h)` / `getWindowSize(uint32_t* w, uint32_t* h)` | 获取图层/窗口大小。 |
| `getScreenResolution(uint32_t* w, uint32_t* h)` | 获取屏幕分辨率。 |
| `setWindowVisibility(bool isVisible)` | 设置窗口可见性。 |
| `setWindowFullScreen()` / `restoreWindowFromFullScreen()` | 窗口铺满屏幕和恢复。 |
| `setWindowFullPlane()` / `restoreWindowFromFullPlane()` | 窗口铺满图层和恢复。 |
| `setWindowRelativeRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, bool sync = true)` | 设置相对布局区域；`sync=false` 时延迟到 `flushRelativeUpdate()` 生效。 |
| `flushRelativeUpdate()` | 应用延迟的相对区域更新。 |
| `init()` | 初始化显示资源；输入图像参数为空时从视频输入通道读取。 |

废弃接口：`getDisplayPlaneSize()`、`setPlaneSize()`、`setWindowSize()`，分别由 `getPlaneSize()`、`setPlaneRect()`、`setWindowRect()` 替代。新代码应使用替代接口。

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

`destination`、`ffmpeg/options`、`video` 和 `audio` 是需要重新配置的参数；详细字段和
生命周期见 [模块参数系统](ffmedia_parameters.md)。成功修改后会关闭旧 FFmpeg 输出上下文，
调用方应重新执行 `init()`。

### ModuleRecorder

头文件：`module/vo/module_recorder.hpp`

基于 FFmpeg muxer 的分片录像输出模块，提供连续/事件录像、按时间或估算大小分片、分片
保留策略、事件索引和崩溃后恢复。该模块仅在启用 FFmpeg 支持时编译。

输出路径模式 `path_pattern` 可包含一个 printf 风格整数占位符，例如
`/var/record/clip_%06u.mp4`；分片序号从已有索引继续递增。占位符支持
`d/i/u/o/x/X` 转换、标志、宽度和固定精度，不支持 `*`。若不包含占位符，第一个分片
使用原始路径，后续分片在扩展名前追加 `_1`、`_2` 等序号；`%%` 表示字面量 `%`。
目标文件已存在时会继续尝试下一个序号。

#### 录像模式、状态和分片记录

| 类型/枚举 | 值 | 说明 |
| --- | --- | --- |
| `RecorderMode::CONTINUOUS` | 0 | 连续录像，所有包写入分片；事件触发只标记当前分片。 |
| `RecorderMode::EVENT` | 1 | 事件录像；缓存事件前窗口，`triggerEvent()` 后才写出事件分片。 |
| `RecorderStatus::IDLE` | 0 | 未运行或已正常停止。 |
| `RecorderStatus::ARMED` | 1 | EVENT 模式已启动并缓存数据，尚未触发事件。 |
| `RecorderStatus::RECORDING` | 2 | 正在写当前分片。 |
| `RecorderStatus::FAULTED` | 3 | setup、写包、索引或停止流程出错；用 `lastError()` 查询错误。 |

`RecorderSegment` 是 `getSegments()` 返回的分片记录：

| 字段 | 说明 |
| --- | --- |
| `sequence` | 分片序号，同时用于渲染路径模式。 |
| `path` | 分片文件路径。 |
| `start_us` / `end_us` | 分片内最早/最晚包时间戳，单位微秒；取 PTS/DTS 中可用值。 |
| `size_bytes` | 分片关闭后的文件大小；打开期间为已写入数据的估算值。 |
| `event_id` | 事件标识；未指定时可为空。 |
| `event` | 是否为事件分片或被事件标记。 |
| `complete` | 分片是否正常关闭且至少写入一个包。 |

#### 公开接口

| API | 说明 |
| --- | --- |
| `ModuleRecorder(path_pattern = "", format = "")` | 构造录像器；`format` 为 FFmpeg muxer 短名称，空值按输出扩展名推断。 |
| `changePathPattern(path_pattern)` | 停止且工作线程退出后切换路径模式，等待旧索引写完并清空内存索引；空路径返回 `-EINVAL`。未显式设置索引时同步切换默认索引路径。 |
| `changeFormat(format)` | 停止且工作线程退出后切换输出格式，需重新初始化。 |
| `setSegmentLimit(value, flags)` | 设置自动分片限制。`flags` 只能为 `SEGMENT_DURATION_MS` 或 `SEGMENT_SIZE_BYTES`，`value=0` 禁用；可运行期修改。 |
| `setRecorderMode(mode)` / `recorderMode()` | 设置/查询录像模式；运行中切换模式返回 `-EBUSY`。 |
| `setEventWindow(pre_duration_ms, post_duration_ms)` | 设置事件前缓存和事件后录制时长；可运行期修改，已触发事件使用触发时快照。 |
| `setRetainedSegments(segment_count)` | 设置保留的已关闭分片数量，`0` 表示不按数量删除；运行中修改返回 `-EBUSY`。 |
| `setStorageQuota(size_bytes)` | 设置已关闭分片总大小配额，`0` 表示不限制；超过后优先删除最旧分片，运行中修改返回 `-EBUSY`。 |
| `setIndexFile(path)` | 停止后设置索引文件；空路径恢复为 `<path_pattern>.ffrec`。 |
| `setFormatOption(key, value, flags)` / `getFormatOption(key, flags)` | 设置/查询传给每个分片 muxer/protocol 的 FFmpeg option；`key` 为空时 `value` 按 `key=value,...` 字典整体替换。 |
| `setVideoParameter(width, height, type, v4l2_fmt = 0)` | 录像前显式设置视频参数；不设置则从输入通道或首个包获取。 |
| `setExtraBuffer(media_type, extra_buffer)` | 录像前设置视频或音频 extradata；有效附加数据可反推未显式配置的媒体参数。 |
| `setAudioParameter(channel_count, bit_per_sample, sample_rate, type, sample_fmt = SAMPLE_FMT_NONE)` | 录像前显式设置音频参数；不设置则从输入通道或首个包获取。 |
| `init()` | 校验输出路径，从已连接输入通道补齐参数和 extradata，并加载分片索引。 |
| `setInputBuffer(input_buffer)` | 初始化后手动写入一个音视频包；输出消费者连接方式下由管线自动调用。 |
| `lastError()` | 返回最近一次 setup/doConsume/teardown 的负 errno；成功为 `0`。 |
| `splitRecording()` | 请求手动分片。有视频时在下一个关键帧边界执行；无视频或尚未见过关键帧时在可确定的包边界执行。 |
| `triggerEvent(event_id = "")` | CONTINUOUS 模式标记当前分片；若尚未打开分片则标记首个新分片。EVENT 模式使用事件前缓存启动新分片。 |
| `getRecorderStatus()` | 原子读取当前状态。 |
| `getSegments()` | 返回当前索引内分片记录快照，包含正在写入的分片。 |
| `recover()` | 未录像时合并索引与路径模式匹配到的现存文件，重建分片列表，并应用保留策略。 |

角色：输出消费者，可连接视频、音频或同时连接两路输入。音视频参数和附加数据优先从
输入通道取得；只有手动注入 buffer 或需要覆盖通道配置时才必须调用显式参数接口。

#### 行为说明

- CONTINUOUS 模式启动后立即进入 `RECORDING`。EVENT 模式启动后进入 `ARMED`，仅缓存
  数据；事件触发后进入 `RECORDING`，写出事件前窗口、触发包和事件后窗口，随后回到
  `ARMED`。若 EVENT 模式启动时事件前和事件后时长均为 0，则按 CONTINUOUS 模式运行。
- 时间分片按包时间戳计算，大小分片按媒体包 active size 估算。实际分片会尽量等待
  视频关键帧等边界，因此关闭点可能晚于设定阈值；无视频输入时不等待关键帧。
- 保留策略只统计已正常关闭且不在当前写入位置的分片；数量或配额超限后从最旧分片开始
  删除。索引和文件删除由后台维护线程执行，`stop()`、`changePathPattern()` 和
  `recover()` 会等待已排队维护操作完成。
- 索引默认为 `<path_pattern>.ffrec`，记录分片元数据并以临时文件加原子替换方式写入。
  `init()` 只加载索引；索引缺失或索引中缺少现存分片记录时，应在再次录像前调用
  `recover()` 合并二者。自动扫描现存文件要求路径模式包含整数占位符。
- 正常 `stop()` 会关闭当前分片并等待索引/删除维护完成。写包或维护失败会置为
  `FAULTED` 并停止继续写入；调用方可根据 `lastError()` 处理，之后需重新初始化或恢复。

`destination`、`recording`、`recorder-status` 和 `ffmpeg/options` 参数的完整路径、默认值
及生命周期见 [模块参数系统](ffmedia_parameters.md)。

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

`path`、`video` 和 `audio` 是需要重新配置的参数；详细字段和生命周期见
[模块参数系统](ffmedia_parameters.md)。运行中、`EOS` 或 `ABNORMAL` 但工作线程尚未退出时
返回 `-EBUSY`；停止后修改会释放旧 writer 并要求重新执行 `init()`。旧的公开设置接口仍保留。

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
| `setReceiveHandler(ReceiveHandler handler)` | 设置收到媒体数据时的回调。 |
| `setControlHandler(ControlHandler handler)` | 设置设备控制命令回调；返回负值会生成协议错误。 |
| `setRecordQueryHandler(RecordQueryHandler handler)` | 设置录像查询回调，由应用填充录像列表 XML。 |
| `setSessionHandler(SessionHandler handler)` | 设置会话播放/暂停/拖动回调。 |
| `setTlsCertificates(clientCert, clientKey, caCert, keyPassword)` | 配置 TLS 客户端证书、私钥、CA 和可选私钥密码。 |
| `setTlsVerify(int enabled)` | 开关 TLS 对端证书校验。 |
| `init()` | 初始化并注册客户端；未手动指定视频格式时从视频输入通道读取编码信息。 |

角色：输出消费者，输入编码媒体数据并按 GB28181 协议推送。

GB28181 回调运行在 SIP/RTP 工作线程中，必须快速返回；处理失败返回负值。证书、认证和网络
参数应在 `init()` 前设置，TLS 功能还要求构建时启用对应的 OpenSSL 支持。

回调类型签名为：

```cpp
using ReceiveHandler = std::function<void(const std::shared_ptr<MediaBuffer>&)>;
using ControlHandler = std::function<int(const std::string& command,
                                         const std::string& device_id,
                                         const std::string& xml)>;
using RecordQueryHandler = std::function<int(
    const std::string& device_id, const std::string& start_time,
    const std::string& end_time, std::string& record_list_xml)>;
using SessionHandler = std::function<int(const std::string& session_name,
                                         const std::string& device_id,
                                         int64_t range_start_us, double scale,
                                         bool paused)>;
```

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
- 当前 `ff_pymedia` 未导出 `ModuleAppSource`、`ModuleAppProcessor`、`AppFrame` 和应用处理回调类型；
  应用注入/回调处理节点请使用 C++ API，或通过已导出的 `ModuleMemReader` 和普通 Hook 接口实现。
- `ModuleGB28181Client` 当前 Python 绑定只导出服务器、认证、设备、NAT 和保活配置；SIP/RTP/TLS
  回调 setter 需使用 C++ API。
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
