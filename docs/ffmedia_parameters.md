# FFMedia 模块参数系统

本文档介绍 `ModuleMedia` 及其派生模块的参数系统。参数系统提供统一的路径、类型、默认值、
权限和生命周期语义，用于替代分散的 `setXXX()` 配置调用。

API 文档见 [FFMedia API](ffmedia_api.md)；自定义模块开发见
[ModuleMedia 派生模块开发指南](module_media.md)。

## 快速开始

公共头文件：`module/ff_media_parameter.hpp`；派生模块声明参数时可使用
`module/ff_media_parameter_helpers.hpp` 中的图像、裁剪和采样参数辅助函数。

每个模块都继承 `MediaParameter`。参数使用 `/` 分隔的路径访问，根参数直接使用名称，对象
成员使用完整路径：

```cpp
auto encoder = FFMedia::makeMediaModule<FFMedia::ModuleMppEnc>(
    FFMedia::MEDIA_CODEC_VIDEO_H264);

for (const auto& info : encoder->queryParameters())
    std::cout << info.name << ": " << info.description << "\n";

encoder->setParameter("encode/fps", 25);
encoder->setParameterFromString("encode/bitrate", "4096");

int64_t bitrate = 0;
encoder->getParameter("encode/bitrate", bitrate);
```

建议在 `init()` 前完成静态配置；只有标记为 `RUNTIME` 且允许当前状态的参数适合运行期修改。
精确的可写状态和默认值应以 `queryParameter()` 返回的 metadata 为准。

## 参数模型

### 类型和值

`ParameterType` 支持以下类型：

| 类型 | C++ 值 | 说明 |
| --- | --- | --- |
| `BOOLEAN` | `bool` | 开关值。 |
| `INTEGER` | `int64_t` 或可转换的整数 | 枚举、计数、fourcc 和超时等。 |
| `DOUBLE` | `double` | 帧率、比例等浮点值。 |
| `STRING` | `std::string` | 路径、URL、FFmpeg 选项等。 |
| `OBJECT` | `ParameterObject` | 结构化参数，可递归包含对象。 |

`ParameterValue` 是上述类型的统一容器。可用 `type()`、`valid()` 查询状态，用
`getBoolean()`、`getInteger()`、`getDouble()`、`getString()`、`getObject()` 读取值，
用 `toString()` 转换为文本。`fromString()` 按指定类型解析文本；整数按 C/C++ 基数规则
解析数值。通过 `MediaParameter::setParameterFromString()` 写入时，框架还会先用 schema 中的
`enum_values` 将枚举名称转换为数值。

`ParameterObject` 保存名称到值的映射，也可作为部分 patch 写入：

```cpp
FFMedia::ParameterObject crop({
    {"x", 100}, {"y", 50}, {"width", 1280}, {"height", 720},
});
processor->setParameter("transform/crop", crop);
```

未出现在 patch 中的成员保持当前值；对象成员仍会逐项进行类型、范围和权限校验。

### 元数据

`ParameterInfo` 描述一个参数或对象节点：

| 字段 | 说明 |
| --- | --- |
| `name` / `description` / `unit` | 名称、说明和单位；名称不包含 `/`。 |
| `type` | `ParameterType`；OBJECT 成员保存在 `object_members`。 |
| `default_value` | 默认值；OBJECT 默认值由成员默认值递归生成。 |
| `minimum` / `maximum` | INTEGER 或 DOUBLE 的可选范围。 |
| `enum_values` | INTEGER 参数的名称、数值和说明。 |
| `flags` | 读写、运行期和废弃标志。 |
| `writable_states` | 允许写入的模块状态掩码。 |
| `apply_mode` | 参数生效时机。 |
| `atomic` | OBJECT 是否作为一次事务提交。 |

参数标志包括 `PARAMETER_FLAG_READABLE`、`PARAMETER_FLAG_WRITABLE`、
`PARAMETER_FLAG_RUNTIME` 和 `PARAMETER_FLAG_DEPRECATED`。

`ParameterApplyMode` 含义如下：

| 值 | 含义 |
| --- | --- |
| `IMMEDIATE` | 写入成功后立即应用。 |
| `RECONFIGURE` | 参数改变通常需要停止工作线程或重新初始化资源。 |
| `NEXT_START` | 参数值保存到下一次 `start()` 使用；当前运行不重建资源。 |
| `CONSTRUCT_ONLY` | 只应在模块构造配置阶段设置。具体是否拒绝后续写入由模块 binding 决定。 |

父 OBJECT 的权限、状态和应用时机会继承到子节点；有效权限取父子限制的交集，应用时机取
更严格的值。`apply_mode` 是 schema 元数据，参数系统本身不自动执行停机或重启；实际能否
写入由 `writable_states` 和模块 setter 再次检查。

## 查询、读取和写入

| API | 用途 |
| --- | --- |
| `hasParameter(path)` | 判断路径是否存在。 |
| `queryParameter(path, info)` | 查询一个叶子或 OBJECT 的完整元数据。 |
| `queryParameters(parent_path)` | 枚举根节点或指定 OBJECT 的直接子节点。 |
| `parameterRevision()` | 获取实例参数版本；每次成功写入递增一次。 |
| `getParameter(path, value)` | 按 C++ 类型读取参数。 |
| `getParameterAsString(path, value)` | 读取文本形式的参数值。 |
| `setParameter(path, value)` | 按 C++ 类型写入参数。 |
| `setParameterFromString(path, text)` | 解析文本并写入参数。 |

窄整数、无符号整数和 enum 可用于数值型读取/写入。类型不匹配返回 `-EINVAL`，负数转换为
无符号数或数值超出范围返回 `-ERANGE`，未知路径返回 `-ENOENT`。

## 对象和事务

使用 `objectParameter()` 创建结构化参数。`atomic=true` 的 OBJECT 及其后代会合并为一次
提交，适合需要跨字段校验或重建资源的配置：

```cpp
module->setParameter("transform", FFMedia::ParameterObject({
    {"crop", {{"x", 100}, {"y", 50}, {"width", 1280}, {"height", 720}}},
    {"rotation", 90},
    {"mirror", true},
}));
```

原子对象的 getter 必须返回完整对象，setter 只在所有校验通过后调用一次；setter 返回负
errno 时整个对象保持不变。非原子 OBJECT patch 如果同时触及多个绑定 setter，会返回
`-ENOTSUP`，应改用原子 OBJECT。

模块开发者通过 `ParameterSchema` 声明参数，通过 binding 将参数连接到实例状态。schema 可
在模块类型之间共享，binding 属于单个实例：

```cpp
static const auto schema = FFMedia::makeParameterSchema({
    FFMedia::integerParameter("bitrate", 4000000, 1, 100000000,
                               "Target bitrate", "bit/s"),
});

installParameterSchema(
    schema,
    {FFMedia::bindParameter("bitrate", bitrate_)});
```

需要加锁、重建资源或执行跨字段校验时，使用 getter/setter binding；setter 返回负 errno
即可拒绝写入。完整对象使用 `bindParameterObject()`，并在 setter 中一次性提交新状态。
`ff_media_parameter_helpers.hpp` 提供 `imageParaParameter()`、`imageCropParameter()`、
`sampleInfoParameter()` 及对应的对象转换辅助函数。

## 生命周期和并发

- `CREATED`、`STOPPED` 通常用于配置 `RECONFIGURE`、`NEXT_START` 和 `CONSTRUCT_ONLY` 参数；
  个别模块还允许 `EOS` 或 `ABNORMAL` 状态，最终以 `writable_states` 为准。
- `STARTED`、`EOS`、`ABNORMAL` 下只能修改明确标记为 `RUNTIME` 且允许当前状态的参数。
- setter 仍应在模块自己的配置锁内再次检查状态，避免检查和提交之间发生竞态。
- getter/setter 回调在参数树内部锁之外执行，可在回调内读取其他参数，但不能嵌套写入。
- 回调内嵌套写入返回 `-EDEADLK`；其他线程在回调执行期间访问参数返回 `-EAGAIN`，调用方应稍后重试。
- getter 依赖形成循环时返回 `-ELOOP`。元数据查询不受回调执行影响。

## 公共对象字段

模块表中的图像、裁剪、采样和矩形对象使用以下统一字段。对象写入可以只提交需要修改的
成员；原子对象建议一次提交相关成员。

| 对象 | 字段 | 类型/范围 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `ImagePara` | `width`, `height` | INTEGER，`0..16384` | `0` | 可见尺寸，单位 pixels |
| `ImagePara` | `hstride`, `vstride` | INTEGER，`0..32768` | `0` | 内存步幅，单位 pixels |
| `ImagePara` | `format` | INTEGER，`0..UINT32_MAX` | `0` | V4L2 pixel format fourcc |
| `ImagePara` | `compression` | INTEGER | `ImageCompression::Linear` | `Linear` 或 `Afbc16x16` |
| `ImageCrop` | `x`, `y`, `width`, `height` | INTEGER，`0..16384` | `0` | 左上偏移和区域尺寸；宽高 0 通常表示全区域 |
| `SampleInfo` | `format` | INTEGER，`SAMPLE_FMT_NONE..SAMPLE_FMT_NB-1` | `SAMPLE_FMT_NONE` | 音频采样格式 |
| `SampleInfo` | `channels` | INTEGER，`0..8` | `0` | 声道数 |
| `SampleInfo` | `sample-rate` | INTEGER，`0..192000` | `0` | 采样率，单位 Hz |
| `SampleInfo` | `samples` | INTEGER，`0..8192` | `0` | 每帧采样数 |
| `Rect`/`Region` | `x`, `y` | INTEGER | `0` | 屏幕或窗口左上位置；Renderer 可为负值 |
| `Rect`/`Region` | `width`, `height` | INTEGER，`0..16384` | `0` | 区域尺寸，单位 pixels |

`format`、codec、布局和 buffer 类型等 INTEGER 参数的数值来自公共头文件中的 enum 或
fourcc 定义。若模块 schema 提供 `enum_values`，可按名称使用；否则应传入对应 C++ 常量，
并通过 `queryParameter()` 检查当前版本支持的范围。

常用枚举常量：

| 参数类别 | 常量 |
| --- | --- |
| RTSP 传输 | `RTSP_STREAM_TYPE_UDP`、`RTSP_STREAM_TYPE_TCP`、`RTSP_STREAM_TYPE_MULTICAST` |
| DRM plane 类型 | `PLANE_TYPE_OVERLAY`、`PLANE_TYPE_PRIMARY`、`PLANE_TYPE_CURSOR`、`PLANE_TYPE_OVERLAY_OR_PRIMARY` |
| DRM 窗口模式 | `DrmDisplayPlane::MULTI_WINDOW_DISPLAY`、`DrmDisplayPlane::SINGLE_WINDOW_DISPLAY` |
| DRM 布局模式 | `DrmDisplayPlane::RELATIVE_LAYOUT`、`DrmDisplayPlane::ABSOLUTE_LAYOUT` |
| RGA 变换 | `RGA_ROTATE_NONE`、`RGA_ROTATE_90`、`RGA_ROTATE_180`、`RGA_ROTATE_270`、`RGA_ROTATE_VFLIP`、`RGA_ROTATE_HFLIP` |
| NPU 核心掩码 | `NPU_CORE_AUTO`、`NPU_CORE_0`、`NPU_CORE_1`、`NPU_CORE_2`、`NPU_CORE_0_1`、`NPU_CORE_0_1_2` |
| VideoBuffer 类型 | `VideoBuffer::DRM_BUFFER_NONCACHEABLE`、`DRM_BUFFER_CACHEABLE`、`DRM_BUFFER_NONCACHEABLE_DMA32`、`DRM_BUFFER_CACHEABLE_DMA32` |

RGA 的 `rotation` 取值依次为 `RGA_ROTATE_NONE`、`RGA_ROTATE_90`、`RGA_ROTATE_180`、
`RGA_ROTATE_270`、`RGA_ROTATE_VFLIP`、`RGA_ROTATE_HFLIP`。RGA `pattern/blend-mode`
当前接受 `BLEND_DISABLE`、`BLEND_SRC`、`BLEND_DST`、`BLEND_SRC_OVER`、`BLEND_DST_OVER`。

## ModuleMedia 通用参数

所有模块都提供以下参数：

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `status` | INTEGER enum | `created`；created、started、eos、stopped、abnormal | RUNTIME（只读） | 查询模块状态 |
| `buffer-count` | INTEGER | `0`，`0..65535` buffers | IMMEDIATE | `init()` 前设置输出池大小 |
| `buffer-size` | INTEGER | `0`，`0..1 GiB` bytes | IMMEDIATE | `init()` 前设置单个输出 Buffer 大小 |
| `input-queue-size` | INTEGER | `1024`，`1..1024` buffers | IMMEDIATE | 调整输入队列容量 |
| `metrics/enabled` | BOOLEAN | `true` | RUNTIME | 开关运行时指标采集 |

## 模块参数参考

以下各节按源码中的参数 schema 列出完整公开参数。`IMMEDIATE` 未特别标注；`RUNTIME` 表示
参数 flags 含 `PARAMETER_FLAG_RUNTIME`，其它生命周期取 `RECONFIGURE`、`NEXT_START` 或
`CONSTRUCT_ONLY`。对象参数为原子提交时，建议一次写入完整对象。

表格中的“默认值”是 schema 的默认值。模块构造函数的参数、输入通道协商结果或驱动返回值
可能覆盖实例当前值；读取当前值应使用 `getParameter()`，读取可用范围和枚举应使用
`queryParameter()`。

参数 schema 的实现位置如下；升级模块时应优先检查对应源码中的 `ParameterInfo` 定义和
binding 行为：

| 模块 | 源码 |
| --- | --- |
| `ModuleCam`、`ModuleAlsaCapture`、`ModuleFFmpegDemux`、`ModuleFileReader`、`ModuleMemReader`、`ModuleRtmpClient`、`ModuleRtspClient` | `module/vi/*.cpp` |
| `ModuleAacDec`、`ModuleAacEnc`、`ModuleMppDec`、`ModuleMppEnc`、`ModuleRga`、`ModuleImageProcessor`、`ModuleVideoStack`、`ModuleInference` | `module/vp/*.cpp` |
| `ModuleAlsaPlayBack`、`ModuleDrmDisplay`、`ModuleFFmpegMux`、`ModuleFileWriter`、`ModuleRtmpServer`、`ModuleRtspServer`、`ModuleGB28181Client`、`ModuleRendererVideo` | `module/vo/*.cpp` |
| `ModuleMedia` | `module/module_media.cpp` |
| `ModuleAppSource`、`ModuleAppProcessor` | `module/module_app.cpp`（当前仅通用参数） |

### ModuleCam

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `device` | STRING | 空；V4L2 设备路径 | RECONFIGURE | 初始化前选择摄像头 |
| `capture/*` | OBJECT | `width/height/hstride/vstride/format/compression` 均为 0 或 `linear`；尺寸 `0..16384`，步幅 `0..32768` | RECONFIGURE | 请求采集格式；驱动可能返回实际值 |
| `frame-rate` | DOUBLE | `0.0`，`0..1000` fps | RECONFIGURE | 设为非零请求摄像头帧率 |
| `timeout-ms` | INTEGER | `2000`，`0..3600000` ms | RUNTIME | 调整采集等待超时 |

### ModuleAlsaCapture

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `source/device` | STRING | `default` | RECONFIGURE | ALSA 采集设备名 |
| `source/sample/*` | OBJECT | `format=none`、`channels=0`、`sample-rate=0`、`samples=0`；见采样对象字段 | RECONFIGURE | 指定 PCM 格式；0 表示自动 |
| `source/layout` | INTEGER | `AI_LAYOUT_NORMAL`，`AI_LAYOUT_NORMAL..AI_LAYOUT_BUTT-1` | RECONFIGURE | 设置通道布局 |

### ModuleFFmpegDemux

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `source/uri` | STRING | 空 | RECONFIGURE | 输入文件、设备或网络 URI |
| `source/loop` | INTEGER | `1`，`0..INT_MAX`；0 禁止循环 | RECONFIGURE | 配置循环播放次数 |
| `input-format` | STRING | 空 | RECONFIGURE | FFmpeg demuxer 短名称；空为自动探测 |
| `ffmpeg/options` | STRING | 空 | RECONFIGURE | `key=value,key2=value2` 形式的 AVDictionary |
| `video-input/*` | OBJECT | 图像字段默认 0，尺寸 `0..16384`，步幅 `0..32768` | RECONFIGURE | 使用 FFmpeg 视频设备输入时指定格式；压缩必须为 `Linear` |
| `timeout-usec` | INTEGER | `5000000`，`0..3600000000` us | RUNTIME | I/O 超时 |
| `seek` | INTEGER | `0`，`0..INT64_MAX` us | RUNTIME（只写） | 跳转到输入时间戳 |
| `seek-flags` | INTEGER | `0`；AVSEEK_FLAG 位掩码 | RUNTIME | 控制 seek 行为 |
| `seek-max` | INTEGER | `0`；只读，单位 us | RUNTIME（只读） | 查询媒体时长 |
| `seekable` | BOOLEAN | `false`；只读 | RUNTIME（只读） | 查询输入是否可跳转 |

### ModuleFileReader

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `source/path` | STRING | 空 | RECONFIGURE | 原始或容器文件路径 |
| `source/loop` | BOOLEAN | `false` | RECONFIGURE | 是否循环读取 |
| `seek` | INTEGER | `0`，`0..INT64_MAX` | RUNTIME（只写） | 容器文件按毫秒、裸流按字节偏移跳转 |
| `seek-max` | INTEGER | `0`，只读 | IMMEDIATE（只读） | 查询最大跳转位置 |

### ModuleMemReader

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `input/*` | OBJECT | 图像字段默认 0；尺寸 `0..16384`，步幅 `0..32768` | RECONFIGURE | 描述外部内存输入的图像格式 |

### ModuleRtmpClient

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `source/url` | STRING | 空 | RECONFIGURE | RTMP URL |
| `source/publish` | BOOLEAN | `true` | RECONFIGURE | true 为发布，false 为拉流 |
| `video-input/*` | OBJECT | 图像字段默认 0 | RECONFIGURE | 发布视频时指定格式；输入 fourcc 仅支持 `V4L2_PIX_FMT_H264` 或 `V4L2_PIX_FMT_HEVC` |
| `timeout/seconds` | INTEGER | `0`，`0..3600` s | RUNTIME | 超时整数部分 |
| `timeout/microseconds` | INTEGER | `0`，`0..999999` us | RUNTIME | 超时小数部分 |

### ModuleRtspClient

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `source/url` | STRING | 空 | RECONFIGURE | RTSP URL |
| `source/stream-type` | INTEGER | `RTSP_STREAM_TYPE_UDP`，`UDP..MULTICAST` | RECONFIGURE | 使用 `RTSP_STREAM_TYPE_UDP/TCP/MULTICAST` 选择传输 |
| `receive/video` | BOOLEAN | `true` | RECONFIGURE | 是否接收视频轨 |
| `receive/audio` | BOOLEAN | `false` | RECONFIGURE | 是否接收音频轨 |
| `timeout-ms` | INTEGER | `5000`，`0..3600000` ms | RUNTIME | 接收超时 |

### ModuleAacDec

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `sample/*` | OBJECT | `format=none`、`channels=0`、`sample-rate=0`、`samples=0`；通道 `0..8`、采样率 `0..192000`、samples `0..8192` | RECONFIGURE | 指定 AAC 解码输出采样格式 |

解码器会优先从输入音频通道的采样信息和 extradata 推导未设置的字段；显式设置仍受
`changeSampleInfo()` 的状态检查约束，写入失败时应处理返回的负 errno。

### ModuleAacEnc

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `aot` | INTEGER enum | `aac-lc` (2)，`0..255`；支持 `aac-lc`、`he-aac`、`aac-ld`、`he-aac-v2`、`aac-eld` | RECONFIGURE | 选择 AAC 编码规格 |
| `bitrate` | INTEGER | `64000`，`0..1536000` bit/s | RECONFIGURE | 目标码率；0 为自动 |
| `afterburner` | BOOLEAN | `true` | RECONFIGURE | 启用 FDK afterburner |
| `eld-sbr` | BOOLEAN | `false` | RECONFIGURE | AAC-ELD 启用 SBR |
| `vbr` | INTEGER | `0`，`0..8` | RECONFIGURE | FDK VBR 模式；0 为 CBR |
| `sample/format` | INTEGER enum | `none`；当前常用 `s16` | RECONFIGURE | 输入 PCM 样本格式 |
| `sample/channels` | INTEGER | `0..ENCODER_MAX_CHANNELS`，默认 0 | RECONFIGURE | 输入声道数 |
| `sample/sample-rate` | INTEGER enum | `0`，`0..96000` Hz | RECONFIGURE | 输入采样率 |
| `sample/samples-per-frame` | INTEGER | `0..8192`，默认 0 | RECONFIGURE | 每帧样本数 |

输入通道要求为 PCM 音频，当前通道协商的 `sample_formats` 仅包含 `SAMPLE_FMT_S16`；
其它 PCM codec 由通道 codec 列表描述，但实际样本格式仍需满足该限制。

### ModuleMppDec

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `input/*` | OBJECT | 图像字段默认 0 | RECONFIGURE | 压缩码流输入描述 |
| `output/format` | INTEGER | `V4L2_PIX_FMT_NV12`，`0..UINT32_MAX` | RECONFIGURE | 输出像素格式 fourcc |
| `output/compression` | INTEGER | `ImageCompression::Linear`；范围 `Linear..Afbc16x16` | RECONFIGURE | 使用 `ImageCompression` 常量选择输出布局 |
| `decode-type` | INTEGER enum | `auto`；h264、h265、mjpeg、vp8、vp9、mpeg1、mpeg2、mpeg4、auto | RECONFIGURE | 指定或自动检测解码器 |
| `split` | BOOLEAN | `false` | RECONFIGURE | 启用 MPP split parser |
| `fast` | BOOLEAN | `true` | RECONFIGURE | 启用快速 parser |
| `deinterlace` | BOOLEAN | `true` | RECONFIGURE | 启用去隔行 |
| `output-timeout-ms` | INTEGER | `0`，`-1..3600000` ms；-1 无限等待 | RUNTIME | 输出队列等待策略 |
| `buffer-type` | INTEGER | `VideoBuffer::DRM_BUFFER_NONCACHEABLE`；范围到 `DRM_BUFFER_CACHEABLE_DMA32` | RECONFIGURE | 使用 `VideoBuffer::BUFFER_TYPE` 常量选择输出 Buffer |

输入通道支持 MPEG-1、MPEG-2、MPEG-4、H.264、H.265、VP8、VP9 和 MJPEG；`decode-type`
设置为 `auto` 时根据输入 codec 选择解码器。

### ModuleMppEnc

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `input/*` | OBJECT | 图像字段默认 0 | RECONFIGURE | 原始视频输入描述 |
| `encode/codec` | INTEGER enum | `h265`；h264、h265、mjpeg | RECONFIGURE | 编码格式 |
| `encode/fps` | INTEGER | `30`，`1..1000` fps | RUNTIME | 编码帧率 |
| `encode/gop` | INTEGER | `60`，`0..100000` 帧 | RUNTIME | I 帧间隔 |
| `encode/bitrate` | INTEGER | `2048`，`0..200000`（schema unit 为 bps） | RUNTIME | 目标码率；MPP backend 当前将该值乘以 1024 传给码控，历史配置通常按 Ki-bit/s 使用 |
| `encode/rc-mode` | INTEGER enum | `cbr`；cbr、vbr、fixqp、avbr | RUNTIME | 码率控制模式 |
| `encode/quality` | DOUBLE | `0.8`，`0..1` | RUNTIME | 相对质量 |
| `encode/profile` | INTEGER enum | `high`；baseline、main、high | RUNTIME | 编码 profile |
| `duration-usec` | INTEGER | `0`，`0..86400000000` us | RUNTIME | 非零时按固定间隔生成输出时间戳 |
| `intra-refresh/enabled` | BOOLEAN | `false` | RUNTIME | 启用渐进式 intra refresh |
| `intra-refresh/mode` | INTEGER enum | `row` (0) 或 `column` (1) | RUNTIME | 刷新方向 |
| `intra-refresh/count` | INTEGER | `0..8192`，默认 0 | RUNTIME | 每帧刷新的宏块数 |
| `output-timeout-ms` | INTEGER | `0`，`-1..3600000` ms | RUNTIME | 编码输出等待超时 |
| `input-cache-pool-size` | INTEGER | `1`，`0..64` 帧 | RUNTIME | 输入缓存池大小 |
| `roi/index` | INTEGER | `0`，`0..7` | RUNTIME | 选择当前读取/写入的 ROI 槽位；最多 8 个区域 |
| `roi/enabled` | BOOLEAN | `false` | RUNTIME | 启用所选 ROI 区域；未启用的槽位不会提交给 MPP |
| `roi/x`, `roi/y` | INTEGER | `0`，`0..65535` 像素 | RUNTIME | ROI 左上角坐标 |
| `roi/width`, `roi/height` | INTEGER | `1`，`1..65535` 像素 | RUNTIME | ROI 宽度和高度 |
| `roi/qp` | INTEGER | `0`；绝对 `0..51`，相对 `-51..51` | RUNTIME | 绝对 QP 或相对帧 QP 的增量 |
| `roi/relative`, `roi/intra` | BOOLEAN | `false` | RUNTIME | `relative=true` 时 QP 为帧 QP 增量；`intra=true` 时区域内强制帧内编码 |
| `split/mode` | INTEGER enum | `none` (0)；none、by-byte、by-ctu | RUNTIME | H.264/H.265 分片模式 |
| `split/arg` | INTEGER | `0`，`0..4294967295` | RUNTIME | 按字节分片时的最大字节数，或按 CTU 分片时的 CTU 数；启用分片时必须大于 0 |
| `split/out` | INTEGER enum/mask | `none` (0)；none、lowdelay、segment、lowdelay-segment（`0..3`） | RUNTIME | 分片输出行为；`lowdelay` 和 `segment` 可通过位掩码组合 |

自定义 SEI 通过 `setSeiUserData(payload, uuid)` 设置。`payload` 为不含 SEI 头的字节数组，`uuid` 可省略（使用 MPP 默认 UUID）或传入 16 字节 UUID；每次调用替换后续帧配置，空 payload 清除自定义数据。

### ModuleRga

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `input/*`, `output/*` | OBJECT | 图像字段默认 0 | RECONFIGURE | 输入/输出图像格式 |
| `input-crop/*`, `output-crop/*` | OBJECT | x/y/w/h 默认 0，`0..16384` | RUNTIME | 裁剪源和目标区域 |
| `zoom` | DOUBLE | `1.0`，`1..20` | RUNTIME | 输入缩放倍数 |
| `zoom-center/x`, `zoom-center/y` | INTEGER | `-1`，`-1..16384` | RUNTIME | 缩放中心；-1 为图像中心 |
| `rotation` | INTEGER | `RGA_ROTATE_NONE..RGA_ROTATE_HFLIP` | RUNTIME | 使用 RGA 旋转/镜像常量 |
| `buffer-type` | INTEGER | `VideoBuffer::DRM_BUFFER_NONCACHEABLE`；范围到 `DRM_BUFFER_CACHEABLE_DMA32` | RECONFIGURE | 使用 `VideoBuffer::BUFFER_TYPE` 常量选择输出 DMA-BUF |
| `scheduler-core` | INTEGER | `SCHEDULER_RGA3_DEFAULT`，`0..15` | RUNTIME | RGA 核心掩码；使用 `SCHEDULER_DEFAULT`、`SCHEDULER_RGA3_CORE0/CORE1`、`SCHEDULER_RGA2_CORE0/CORE1` 或组合值 |
| `pattern/image/*`, `pattern/crop/*` | OBJECT | 图像/裁剪默认值 | RUNTIME | 叠加图案及其区域 |
| `pattern/blend-mode` | INTEGER | `BLEND_DISABLE..BLEND_DST_OVER` | RUNTIME | 使用 RGA blend 常量选择图案混合模式 |

### ModuleImageProcessor

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `input/*` | OBJECT | 图像字段默认 0 | RECONFIGURE | 主输入图像 |
| `transform/crop/*` | OBJECT | x/y/w/h 默认 0，`0..16384` | RUNTIME | 主图像裁剪；宽高 0 为全图 |
| `transform/rotation` | INTEGER | `0`，`0..359` 度（当前支持 90 度倍数） | RUNTIME | 主图像旋转 |
| `transform/mirror`, `transform/flip` | BOOLEAN | `false` | RUNTIME | 主图像镜像/翻转 |
| `cover/index` | INTEGER | `0`，`0..15`（共 16 个槽位） | RUNTIME | 选择遮罩槽位 |
| `cover/crop/*` | OBJECT | x/y/w/h 默认 0 | RUNTIME | 遮罩输出区域；宽高 0 禁用 |
| `cover/color` | INTEGER | `0xff000000`，`0..UINT32_MAX` | RUNTIME | `0xAARRGGBB` 遮罩颜色 |
| `blend/input-id` | INTEGER | 默认输入 ID，`1..MEDIA_CHANNEL_ID_ANY-1` | RUNTIME | 选择辅助输入；主输入 ID 不能作为辅助层 |
| `blend/enabled` | BOOLEAN | `false` | RUNTIME | 启用辅助图层 |
| `blend/z-order` | INTEGER | `0`，`INT32_MIN..INT32_MAX` | RUNTIME | 图层顺序，值大者在上 |
| `blend/source-crop/*`, `blend/destination-rect/*` | OBJECT | x/y/w/h 默认 0 | RUNTIME | 辅助源和目标区域 |
| `blend/opacity` | DOUBLE | `1.0`，`0..1` | RUNTIME | 图层透明度 |
| `blend/rotation`, `blend/mirror`, `blend/flip`, `blend/alpha-mode` | INTEGER/BOOLEAN | 旋转 0；镜像/翻转 false；alpha 为 `straight` | RUNTIME | 辅助图层变换及 alpha 表示；alpha 支持 `opaque/straight/premultiplied` |
| `output/*` | OBJECT | 图像字段默认 0 | RECONFIGURE | 输出图像格式 |
| `buffer-type` | INTEGER | `VideoBuffer::DRM_BUFFER_NONCACHEABLE`；范围到 `DRM_BUFFER_CACHEABLE_DMA32` | RECONFIGURE | 使用 `VideoBuffer::BUFFER_TYPE` 常量选择输出 DMA-BUF |

### ModuleVideoStack

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `output/*` | OBJECT | 构造时输出图像配置 | RECONFIGURE | 合成输出格式 |
| `frame-rate` | DOUBLE | `30.0`，`0.1..1000.0` fps | RUNTIME | 合成输出帧率 |
| `thread-count` | INTEGER | `2`，`1..16` | RUNTIME | 处理线程上限 |
| `input-layout/input-id` | INTEGER | `0`，`0..MEDIA_CHANNEL_ID_ANY-1` | RUNTIME | 选择输入通道 |
| `input-layout/crop/*` | OBJECT | x/y/w/h 默认 0 | RUNTIME | 输入在输出画布中的区域 |
| `background-color/r`, `background-color/g`, `background-color/b` | INTEGER | `20`，`0..255` | RUNTIME | RGB 背景色 |

### ModuleAlsaPlayBack

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `device/name` | STRING | `default` | RECONFIGURE | ALSA 播放设备名 |
| `device/sample/*` | OBJECT | `format=none`、`channels=0`、`sample-rate=0`、`samples=0` | RECONFIGURE | 播放采样格式 |
| `device/layout` | INTEGER | `AI_LAYOUT_NORMAL`，`AI_LAYOUT_NORMAL..AI_LAYOUT_BUTT-1` | RECONFIGURE | 声道布局 |

### ModuleDrmDisplay

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `input/*` | OBJECT | 图像字段默认 0 | RECONFIGURE | 输入图像格式 |
| `plane/format` | INTEGER | `V4L2_PIX_FMT_NV12`，`0..UINT32_MAX` | RECONFIGURE | DRM plane fourcc |
| `plane/id`, `plane/connector` | INTEGER | `0`，`0..UINT32_MAX` | RECONFIGURE | 指定 plane/connector |
| `plane/type` | INTEGER | `PLANE_TYPE_OVERLAY_OR_PRIMARY`；`PLANE_TYPE_OVERLAY..PLANE_TYPE_OVERLAY_OR_PRIMARY` | RECONFIGURE | 使用 `PLANE_TYPE_*` 常量选择 plane 类型 |
| `plane/zpos` | INTEGER | `255`，`0..255` | RECONFIGURE | 图层顺序 |
| `plane/linear` | INTEGER | `1`，`0..1` | RECONFIGURE | 1 优先线性 buffer，0 允许 AFBC |
| `plane-rect/*`, `window-rect/*`, `image-rect/*`, `relative-window-rect/*` | OBJECT | x/y/w/h 默认 0，`0..16384` | RUNTIME | 屏幕、窗口、源图像和相对布局区域 |
| `display-mode` | INTEGER | `MULTI_WINDOW_DISPLAY..SINGLE_WINDOW_DISPLAY` | RECONFIGURE | 使用 `DrmDisplayPlane::MULTI_WINDOW_DISPLAY` 或 `SINGLE_WINDOW_DISPLAY` |
| `layout-mode` | INTEGER | `ABSOLUTE_LAYOUT`；`RELATIVE_LAYOUT..ABSOLUTE_LAYOUT` | RECONFIGURE | 使用 `DrmDisplayPlane::RELATIVE_LAYOUT/ABSOLUTE_LAYOUT` |
| `visible` | BOOLEAN | `true` | RUNTIME | 显示/隐藏窗口 |
| `drm-sync` | BOOLEAN | `true` | RECONFIGURE | 仅单窗口提交后等待 DRM vblank |

### ModuleRendererVideo

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `input/*` | OBJECT | 图像字段默认 0 | RECONFIGURE | 输入图像格式 |
| `output/width`, `output/height` | INTEGER | `0`，`0..16384` | RECONFIGURE | OpenGL 输出分辨率 |
| `window/*`, `image/*` | OBJECT | x/y `-16384..16384`，宽高 `0..16384` | RUNTIME | 窗口和图像显示区域 |
| `visible` | BOOLEAN | `true` | RUNTIME | 显示/隐藏窗口 |
| `title` | STRING | 空 | CONSTRUCT_ONLY | 创建窗口时设置标题 |

### ModuleFFmpegMux

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `destination/uri` | STRING | 空 | RECONFIGURE | 输出 URI 或文件路径 |
| `destination/format` | STRING | 空 | RECONFIGURE | muxer 短名称；空为自动 |
| `ffmpeg/options` | STRING | 空 | RECONFIGURE | 输出 muxer/protocol AVDictionary |
| `video/width`, `video/height` | INTEGER | `0..16384`，默认 0 | NEXT_START | 无输入通道时指定视频尺寸 |
| `video/codec` | INTEGER | `MEDIA_CODEC_UNKNOWN..MEDIA_CODEC_VIDEO_RAW` | NEXT_START | 指定视频 codec |
| `video/pixel-format` | INTEGER | `0..UINT32_MAX`，默认 0 | NEXT_START | 指定视频 fourcc |
| `audio/channels` | INTEGER | `0..8`，默认 0 | NEXT_START | 音频声道数 |
| `audio/bits-per-sample` | INTEGER | `0..32`，默认 0 | NEXT_START | 位深 |
| `audio/sample-rate` | INTEGER | `0..192000` Hz，默认 0 | NEXT_START | 采样率 |
| `audio/codec` | INTEGER | `MEDIA_CODEC_UNKNOWN..MEDIA_CODEC_AUDIO_OPUS` | NEXT_START | 音频 codec |
| `audio/sample-format` | INTEGER | `SAMPLE_FMT_NONE..SAMPLE_FMT_NB-1` | NEXT_START | 音频样本格式 |

封装模块优先从输入通道取得 codec、图像/采样参数和 extradata。只有手动注入 Buffer 或没有
建立通道时才需要设置 `video/*`、`audio/*`。

### ModuleRecorder

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `destination/path-pattern` | STRING | 构造值；写入不能为空 | RECONFIGURE | 分片输出路径模式，可含一个整数占位符 |
| `destination/format` | STRING | 构造值 | RECONFIGURE | FFmpeg muxer 短名称；空为按扩展名推断 |
| `destination/index-file` | STRING | `<path_pattern>.ffrec` | RECONFIGURE | 分片索引文件 |
| `recording/mode` | INTEGER enum | `continuous` (0)；`continuous..event` | IMMEDIATE | 连续录像或事件触发录像 |
| `recording/segment-duration-ms` | INTEGER | `0`，`0..INT64_MAX` | RUNTIME / IMMEDIATE | 自动按时间分片；0 禁用 |
| `recording/segment-size-bytes` | INTEGER | `0`，`0..INT64_MAX` | RUNTIME / IMMEDIATE | 按包 active size 估算并触发分片；0 禁用 |
| `recording/retained-segments` | INTEGER | `0`，`0..UINT32_MAX` | NEXT_START | 保留的已关闭分片数；0 不按数量删除 |
| `recording/storage-quota-bytes` | INTEGER | `0`，`0..INT64_MAX` | NEXT_START | 已关闭分片总大小配额；0 不限制 |
| `recording/event-pre-ms` | INTEGER | `0`，`0..INT64_MAX` | RUNTIME / IMMEDIATE | EVENT 模式触发前缓存时长 |
| `recording/event-post-ms` | INTEGER | `0`，`0..INT64_MAX` | RUNTIME / IMMEDIATE | EVENT 模式触发后录制时长 |
| `recorder-status` | INTEGER | `idle` (0)，`0..3`；只读 | RUNTIME（只读） | 查询 `idle/armed/recording/faulted` |
| `ffmpeg/options` | STRING | 空 | RECONFIGURE | 每个分片 muxer/protocol 的 `key=value,...` AVDictionary |

`destination` 和 `recording` 是事务 OBJECT。通过 OBJECT 写入时，自定义绑定要求本次值中
包含全部成员；通过叶子路径修改时，未修改成员沿用当前值并随整个 OBJECT 一起提交。修改
`destination/*` 或 `ffmpeg/options` 后必须重新执行 `init()`。

运行中的 `segment-duration-ms`、`segment-size-bytes`、`event-pre-ms` 和 `event-post-ms`
可以修改；已在等待执行的事件使用触发时捕获的事件窗口。运行中修改 `mode`、
`retained-segments` 或 `storage-quota-bytes` 返回 `-EBUSY`。保留策略只作用于已关闭分片，
并在数量或配额超限时从最旧分片开始删除。

### ModuleFileWriter

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `path` | STRING | 空 | RECONFIGURE | 输出文件路径 |
| `video/width`, `video/height` | INTEGER | `0..16384`，默认 0 | NEXT_START | 视频尺寸 |
| `video/codec` | INTEGER | `MEDIA_CODEC_UNKNOWN..MEDIA_CODEC_VIDEO_RAW` | NEXT_START | 视频 codec |
| `audio/channels` | INTEGER | `0..8`，默认 0 | NEXT_START | 声道数 |
| `audio/bits-per-sample` | INTEGER | `0..32`，默认 0 | NEXT_START | 位深 |
| `audio/sample-rate` | INTEGER | `0..192000` Hz，默认 0 | NEXT_START | 采样率 |
| `audio/codec` | INTEGER | `MEDIA_CODEC_UNKNOWN..MEDIA_CODEC_AUDIO_OPUS` | NEXT_START | 音频 codec |

### ModuleRtmpServer

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `endpoint/path` | STRING | `/live` | RECONFIGURE | 发布路径 |
| `endpoint/port` | INTEGER | `1935`，`1..65535` | RECONFIGURE | 监听端口 |
| `video-input/*` | OBJECT | 图像字段默认 0 | RECONFIGURE | 输入视频格式；输入 fourcc 仅支持 `V4L2_PIX_FMT_H264` 或 `V4L2_PIX_FMT_HEVC` |
| `media/video-codec`, `media/audio-codec` | INTEGER | UNKNOWN 到对应 codec 最大值 | RECONFIGURE | 声视频 codec；RTMP Server 实际支持视频 H.264/H.265、音频 AAC/MP3 |
| `max-clients` | INTEGER | `32`，`1..128` | RUNTIME | 最大客户端数 |
| `max-timeout-count` | INTEGER | `3`，`0..1000` | RUNTIME | 连续超时次数上限 |
| `timeout/seconds` | INTEGER | `3`，`0..3600` s | RUNTIME | 超时整数部分 |
| `timeout/microseconds` | INTEGER | `0`，`0..999999` us | RUNTIME | 超时小数部分 |

### ModuleRtspServer

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `endpoint/path` | STRING | 空 | CONSTRUCT_ONLY | RTSP 路径 |
| `endpoint/port` | INTEGER | `554`，`1..65535` | CONSTRUCT_ONLY | 监听端口 |
| `media/video-codec`, `media/audio-codec` | INTEGER | UNKNOWN 到对应 codec 最大值 | RECONFIGURE | 声视频 codec；RTSP Server 实际支持视频 H.264/H.265、音频 AAC/G.711A/G.711U |
| `authentication/realm` | STRING | 空 | RUNTIME（只写） | 认证域 |
| `authentication/username` | STRING | 空 | RUNTIME（只写） | 用户名 |
| `authentication/password` | STRING | 空 | RUNTIME（只写） | 密码 |

### ModuleGB28181Client

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `local/user-id`, `local/user-agent` | STRING | 空 | CONSTRUCT_ONLY | 本地 SIP 身份 |
| `local/transport` | INTEGER | `TRANSPORT_TYPE_TCP`；接受 TCP、UDP、TLS（不接受 DTLS） | CONSTRUCT_ONLY | 使用 `TRANSPORT_TYPE_TCP/UDP/TLS` 选择 SIP 传输协议 |
| `local/port` | INTEGER | `0..65535`，默认 0 | CONSTRUCT_ONLY | 本地 SIP 端口 |
| `video-input/*` | OBJECT | 图像字段默认 0 | RECONFIGURE | 输入视频格式；输入 fourcc 仅支持 `V4L2_PIX_FMT_H264` 或 `V4L2_PIX_FMT_HEVC` |
| `media/video-codec`, `media/audio-codec` | INTEGER | UNKNOWN 到对应 codec 最大值 | RECONFIGURE | 媒体 codec；GB28181 实际支持视频 H.264/H.265、音频 AAC/G.711A/G.711U |
| `server/id`, `server/realm`, `server/ip` | STRING | 空 | RUNTIME | SIP 服务器信息 |
| `server/port` | INTEGER | `5060`，`1..65535` | RUNTIME | SIP 端口 |
| `server/expiry` | INTEGER | `3600`，`0..86400` s | RUNTIME | 注册有效期 |
| `authentication/username`, `authentication/password` | STRING | 空；只写 | RUNTIME | SIP 认证 |
| `device/name`, `device/manufacturer`, `device/channel` | STRING | 空 | RUNTIME | 设备描述 |
| `firewall-ip` | STRING | 空 | RUNTIME | 公网防火墙地址 |
| `auto-masquerade` | BOOLEAN | `false` | RUNTIME | 自动 SIP Contact 映射 |
| `keepalive-seconds` | INTEGER | `0`，`0..86400` s | RUNTIME | 心跳周期；0 禁用 |

### ModuleInference

| 路径 | 类型 | 默认值/范围 | 生命周期 | 使用时机 |
| --- | --- | --- | --- | --- |
| `input/image/*` | OBJECT | 图像字段默认 0 | RECONFIGURE | 推理输入图像 |
| `input/crop/*` | OBJECT | x/y/w/h 默认 0，`0..16384` | RECONFIGURE | 推理区域；宽高 0 为全图 |
| `model/path` | STRING | 空 | RECONFIGURE | RKNN 模型文件路径 |
| `model/core-mask` | INTEGER | `NPU_CORE_AUTO`，到 `NPU_CORE_0_1_2` | RECONFIGURE | NPU 核心掩码 |
| `interval` | INTEGER | `0`，`0..1000000` 帧 | RUNTIME | 两次推理之间跳过的帧数 |

### ModuleAppSource 与 ModuleAppProcessor

这两个应用扩展模块当前没有额外的参数 schema，只提供 `ModuleMedia` 通用参数。输入输出
通道、队列策略、回调和应用内存模式通过构造函数中的 `AppSourceOptions`、
`AppProcessorOptions` 及 `AppFrame` 配置；详见 API 文档中的应用扩展章节。

## 输入通道约束

参数配置不能替代通道类型协商。模块在 `init()` 时仍会检查输入通道的 media type、codec 和
必要的样本/图像格式；常用约束如下：

| 模块 | 输入 ID/名称 | 接受的输入 |
| --- | --- | --- |
| `ModuleAacDec` | `0 / audio-aac` | 音频，`MEDIA_CODEC_AUDIO_AAC` |
| `ModuleAacEnc` | `0 / audio-pcm` | PCM 音频；通道样本格式要求 `SAMPLE_FMT_S16` |
| `ModuleMppDec` | `0 / compressed-video` | MPEG-1/2/4、H.264、H.265、VP8、VP9、MJPEG |
| `ModuleMppEnc`、`ModuleRga`、`ModuleDrmDisplay`、`ModuleRendererVideo` | `0` | 原始视频 |
| `ModuleImageProcessor` | `main-video`（主输入）及动态 `blend-*` | 原始视频 |
| `ModuleFFmpegMux`、`ModuleRecorder`、`ModuleFileWriter`、`ModuleRtmpServer`、`ModuleRtspServer`、`ModuleGB28181Client` | `0 / video`、`1 / audio` | 视频和音频按模块支持的 codec 列表 |
| `ModuleVideoStack` | 动态输入 ID | 原始视频；`input-layout/input-id` 选择要布局的通道 |

`ModuleRtmpClient`、`ModuleRtspClient`、各 Reader 和采集模块是源模块，不要求输入通道。
当参数与通道元数据冲突时，应先修正连接或输入 Buffer 的 media 信息，而不是只修改参数。

## C++ 使用建议

`ParameterObject` 支持从初始化列表隐式构造，因此可以直接使用
`setParameter("path", {{"member", value}})`；需要保存对象、复用对象或避免重载歧义时，
再显式写成 `ParameterObject({...})`。

```cpp
// 静态参数：在 init() 前写入，并检查负 errno。
camera->setParameter("device", std::string("/dev/video0"));
camera->setParameter("capture", {
    {"width", 1920}, {"height", 1080}, {"format", V4L2_PIX_FMT_NV12},
});

encoder->setParameter("encode", {
    {"codec", FFMedia::MEDIA_CODEC_VIDEO_H264},
    {"fps", 30}, {"gop", 60}, {"bitrate", 4096},
});

// 运行期参数：仅写入标记为 RUNTIME 的路径。
display->setParameter("visible", false);
encoder->setParameter("duration-usec", 40000);

// 读取完整 metadata，避免依赖构建版本中的枚举数值。
FFMedia::ParameterInfo info;
if (encoder->queryParameter("encode/codec", info) == 0) {
    for (const auto& item : info.enum_values)
        std::cout << item.name << "=" << item.value << "\n";
}
```

嵌套对象同样可以隐式构造；以下写法等价于显式创建两个
`FFMedia::ParameterObject`：

```cpp
processor->setParameter("transform", {
    {"crop", {{"x", 100}, {"y", 50}, {"width", 1280}, {"height", 720}}},
    {"rotation", 90},
    {"mirror", true},
});
```

`RECONFIGURE`、`NEXT_START` 和 `CONSTRUCT_ONLY` 参数必须在 `CREATED` 或 `STOPPED` 状态配置。
修改 `RECONFIGURE` 参数后，通常还需要重新调用 `init()`；具体模块行为以该模块 API 章节为准。

模块章节中的旧式 setter 仍属于兼容 API；新代码优先使用参数路径，并根据返回的负 errno
处理配置失败。未知或版本不支持的路径不应被静默忽略。对于图像对象统一使用
`width`、`height`、`hstride`、`vstride`、`format`、`compression`；对于裁剪对象使用
`x`、`y`、`width`、`height`；对于采样对象使用 `format`、`channels`、`sample-rate`、
`samples`。

## Python 使用建议

Python 绑定模块名为 `ff_pymedia`。`MediaParameter.setParameter()` 接受 Python 的
`bool`、`int`、`float`、`str`、`dict`、`ParameterValue` 或 `ParameterObject`；`dict` 的
键必须是字符串，并会递归转换为参数对象。查询和读取失败会抛出 `KeyError`（未知路径）
或 `ValueError`（类型、范围、状态等错误），写入接口返回负 errno，调用方应检查返回值。

```python
import ff_pymedia as ff

encoder = ff.ModuleMppEnc(ff.MEDIA_CODEC_VIDEO_H264)

# 对象参数可以直接使用 dict，也可以显式构造 ParameterObject。
ret = encoder.setParameter("encode", {
    "codec": ff.MEDIA_CODEC_VIDEO_H264,
    "fps": 30,
    "gop": 60,
    "bitrate": 4096,
})
if ret < 0:
    raise RuntimeError(f"set encode failed: {ret}")

# 读取参数值和当前实例的 metadata。
fps = encoder.getParameter("encode/fps")
info = encoder.queryParameter("encode/codec")
print("fps:", fps)
print("default codec:", info.default_value.toPython())
for item in info.enum_values:
    print(item.name, item.value, item.description)

# 运行期参数仍须遵守 schema 的 RUNTIME 和 writable_states 限制。
ret = encoder.setParameter("duration-usec", 40000)
if ret < 0:
    raise RuntimeError(f"set duration failed: {ret}")

# 也可以使用字符串形式；枚举参数支持名称或整数文本。
ret = encoder.setParameterFromString("encode/rc-mode", "cbr")
if ret < 0:
    raise RuntimeError(f"set rc mode failed: {ret}")
```

`queryParameters()` 返回 `ParameterInfo` 列表；传入对象路径可以只枚举该对象的直接成员。
`ParameterInfo.default_value`、`minimum` 和 `maximum` 是 `ParameterValue`，使用
`.toPython()` 转换为 `bool`、`int`、`float`、`str` 或 `dict`。`ParameterObject.toDict()`
可将显式对象转换回 Python 字典：

```python
processor = ff.ModuleImageProcessor()
crop = ff.ParameterObject({"x": 100, "y": 50, "width": 1280, "height": 720})
processor.setParameter("transform/crop", crop)
print(processor.getParameter("transform/crop"))

for child in processor.queryParameters("transform"):
    print(child.name, child.type, child.apply_mode)
```

模块构造函数和媒体 Buffer 的 Python API 仍按 [FFMedia API](ffmedia_api.md) 中的绑定说明
使用；参数路径、默认值和状态限制以本文各模块章节及运行时 `ParameterInfo` 为准。
