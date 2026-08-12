# `ffmedia` 命令行使用介绍

`ffmedia` 是由 [`ffmedia.cpp`](ffmedia.cpp) 构建的通用媒体管线命令行工具。源码仓库
路径为 `demo/ffmedia.cpp`，发布 SDK 路径为 `examples/demo/ffmedia.cpp`。它通过命令行
声明模块、设置模块参数并连接模块，不需要为每种媒体链路单独编写 C++ Demo。

工具遵循三个原则：

- 使用 `ID=TYPE` 声明每个模块实例。
- 所有模块配置都通过统一参数系统设置，不调用模块专用 setter。
- 使用显式有向图描述输入、处理和输出关系，支持串联、分支和多源汇聚。

本文只介绍 `ffmedia` CLI。其他固定场景 Demo 请参见 [Demo 总览](Readme.md)。

## 1. 构建和运行

可以使用已提供预编译程序，可直接运行：

```bash
LD_LIBRARY_PATH="$PWD/lib" ./bin/ffmedia --help
```

也可使用发布 SDK 根目录的 `CMakeLists.txt` 重新编译 CLI：

```bash
cmake -S . -B build -DENABLE_TESTS=OFF
cmake --build build --target ffmedia -j
LD_LIBRARY_PATH="$PWD/lib" ./build/ffmedia --help
```

下文的 `./build/ffmedia` 均可替换为发布 SDK 的 `./bin/ffmedia`。

如果已将 `ffmedia` 和 `libff_media.so` 一起复制到目标目录，则可使用：

```bash
LD_LIBRARY_PATH=. ./ffmedia modules
```

## 2. 命令概览

```text
ffmedia modules
ffmedia params TYPE [PATH]
ffmedia run [options]
```

| 命令 | 用途 |
| --- | --- |
| `modules` | 列出当前 SDK 可用的模块类型。 |
| `params TYPE [PATH]` | 查询某类模块的完整参数 schema，或指定参数子树。 |
| `run` | 声明模块、配置参数、连接并运行媒体管线。 |

查看帮助：

```bash
./build/ffmedia --help
./build/ffmedia run --help
```

推荐的使用顺序是：

1. 用 `modules` 确认模块已编译。
2. 用 `params TYPE` 查询模块支持的参数和合法取值。
3. 用 `run -m ... -p ... -c ...` 描述完整管线。
4. 正式运行前，可用 `--show-params` 检查配置结果。

## 3. 查询可用模块

```bash
./build/ffmedia modules
```

输出包含：

- `TYPE`：传给 `-m ID=TYPE` 的模块类型名。
- `CLASS`：`vi`、`vp` 或 `vo`。
- `GRAPH`：`yes` 表示通用 runner 可运行，`special` 表示需要应用适配。
- `DESCRIPTION`：模块用途。

当前 SDK 的可用模块如下，实际结果以 `ffmedia modules` 为准。

| 分类 | 模块类型 | 用途 | 条件或限制 |
| --- | --- | --- | --- |
| VI | `cam` | V4L2 Camera 输入 | 需要摄像头设备 |
| VI | `file-reader` | 文件或容器输入 | 普通 graph 模块 |
| VI | `rtsp-client` | RTSP 拉流 | 普通 graph 模块 |
| VI | `rtmp-client` | RTMP 拉流或推流 | 角色由 `source/publish` 决定 |
| VI | `ffmpeg-demux` | FFmpeg 输入和解复用 | — |
| VI | `alsa-capture` | ALSA 采集 | 需要 ALSA 采集设备 |
| VI | `mem-reader` | 应用内存喂帧 | `special`，只能查询参数 |
| VP | `mpp-dec` | Rockchip MPP 解码 | 需要 MPP |
| VP | `mpp-enc` | Rockchip MPP 编码 | 需要 MPP |
| VP | `rga` | Rockchip RGA 图像处理 | 需要 RGA |
| VP | `video-stack` | 多路视频拼接 | 每个输入需配置一组 `input-layout` |
| VP | `image-processor` | EGL/OpenGL 图像处理 | 需要 EGL/GLES 运行环境 |
| VP | `aac-dec` | AAC 解码 | — |
| VP | `aac-enc` | AAC 编码 | — |
| VP | `inference` | RKNN 推理 | 需要匹配芯片的 RKNN 运行库 |
| VO | `drm-display` | DRM 显示 | 需要 DRM 设备 |
| VO | `renderer-video` | 窗口视频渲染 | 需要可用的窗口显示后端 |
| VO | `file-writer` | 文件输出 | 普通 graph 模块 |
| VO | `ffmpeg-mux` | FFmpeg 封装或网络输出 | — |
| VO | `rtsp-server` | RTSP Server 输出 | 普通 graph 模块 |
| VO | `rtmp-server` | RTMP Server 输出 | 普通 graph 模块 |
| VO | `gb28181-client` | GB28181 输出 | 普通 graph 模块 |
| VO | `alsa-playback` | ALSA 播放 | 需要 ALSA 播放设备 |

`mem-reader` 需要应用主动送入 `MediaBuffer`，因此通用 `ffmedia run` 会明确返回
`-ENOTSUP`。`video-stack` 可作为普通 graph 模块使用，但每个输入通道必须通过独立的
`-p` 参数块配置一组 `input-layout`。

## 4. 查询参数配置

### 4.1 查询模块类型的 schema

查询完整参数：

```bash
./build/ffmedia params image-processor
```

只查询指定对象或叶子参数：

```bash
./build/ffmedia params image-processor output
./build/ffmedia params image-processor transform/crop
./build/ffmedia params mpp-dec output/compression
```

参数输出会显示以下信息：

- 类型：`boolean`、`integer`、`double`、`string` 或 `object`。
- 访问权限：`r`、`w` 或 `rw`。
- `atomic`：该结构作为一个原子对象提交。
- `runtime`：运行期参数。
- `apply`：`immediate`、`reconfigure`、`next-start` 或 `construct-only`。
- `states`：允许写入该参数的模块状态。
- 默认值、当前值、最小值、最大值、单位和枚举值。

只写参数的默认值和当前值会显示为隐藏，不会输出凭据内容。

每个模块还继承 `ModuleMedia` 的通用参数，例如：

- `status`：当前模块状态，只读。
- `buffer-count`：输出缓冲区数量。
- `buffer-size`：单个输出缓冲区大小。
- `input-queue-size`：输入队列容量。

### 4.2 查询应用配置后的模块实例

`params TYPE` 查询的是新建模块的默认配置。要查看一组 `-p` 应用后的结果，使用：

```bash
./build/ffmedia run \
  -m gpu=image-processor \
  -p 'gpu:output{width=1280;height=720;format=NV12;compression=linear}' \
  -p 'gpu:transform{crop{x=0;y=0;width=1920;height=1080};rotation=90}' \
  --show-params gpu
```

也可只看某个参数子树：

```bash
./build/ffmedia run \
  -m gpu=image-processor \
  -p 'gpu:output{width=1280;height=720;format=NV12;compression=linear}' \
  --show-params gpu:output
```

`--show-params` 可重复指定。只要命令中包含该选项，工具就会在参数设置完成后打印配置并
退出，不执行图连接、图结构验证和模块 `init()`，因此查询单个模块时不需要提供 `-c`。

## 5. 声明和连接管线

### 5.1 声明模块

```text
-m ID=TYPE
--module ID=TYPE
```

示例：

```bash
-m source=ffmpeg-demux
-m decoder=mpp-dec
-m output=file-writer
```

`ID` 是本条命令内使用的实例名，必须唯一；可使用字母、数字、下划线和连字符，并且不能
以数字开头。`TYPE` 必须来自 `ffmedia modules`，并且区分大小写。

### 5.2 连接模块

```text
-c PRODUCER[@CHANNELS]=CONSUMER
--connect PRODUCER[@CHANNELS]=CONSUMER
```

不指定通道时，连接逻辑根据生产者输出和消费者输入要求选择兼容通道：

```bash
-c decoder=display
```

指定单个输出通道：

```bash
-c demux@0=decoder
```

一次选择多个输出通道：

```bash
-c demux@0,1=mux
```

通道 ID 只接受十进制数字，不接受 `video`、`audio` 等通道名称。

同一生产者需要连接同一消费者的多个通道时，应放在同一个 `-c` 的逗号列表中；重复声明
同一 `PRODUCER=CONSUMER` 边会返回 `-EEXIST`。

### 5.3 分支和多源汇聚

一个生产者可连接多个消费者：

```text
             +-> display
source -> dec
             +-> encoder -> writer
```

对应连接：

```bash
-c source@0=dec
-c dec=display
-c dec=encoder
-c encoder=writer
```

一个支持多个输入的消费者也可接收多个生产者，例如分别将视频和音频送入同一个 mux：

```bash
-c video=mux
-c audio=mux
```

工具会拒绝未知节点、重复边、自连接、环路、带输入边的源模块，以及没有生产者的处理或
输出模块。模块按拓扑顺序连接和初始化。

## 6. 使用 `-p` 完整配置模块参数

### 6.1 基本格式

```text
-p 'ID:ENTRY[;ENTRY...]'
--params 'ID:ENTRY[;ENTRY...]'
```

一个 `-p` 可以一次写完一个模块的多条参数：

```bash
-p 'decoder:fast=true;deinterlace=true;buffer-count=10;input-queue-size=64'
```

也可以为同一模块重复使用多个 `-p`，便于按功能拆分：

```bash
-p 'decoder:fast=true;deinterlace=true'
-p 'decoder:buffer-count=10;input-queue-size=64'
```

参数名和取值都以 `params TYPE` 输出的 schema 为准。

### 6.2 扁平路径

结构参数可以使用 `/` 写成完整路径：

```bash
-p 'gpu:output/width=1280;output/height=720;output/format=NV12'
```

### 6.3 OBJECT 块

同一结构的成员可使用 `{...}` 集中描述：

```bash
-p 'gpu:output{width=1280;height=720;format=NV12;compression=linear}'
```

两种写法等价，也可以混用。OBJECT 本身不能通过 `output=...` 赋一个字符串，必须配置其
子路径或使用 OBJECT 块。

### 6.4 嵌套 OBJECT 块

结构可以继续嵌套，适合一次描述完整图像变换：

```bash
-p 'gpu:transform{crop{x=0;y=0;width=1920;height=1080};rotation=90;mirror=false;flip=false};output{width=1280;height=720;format=NV12;compression=linear}'
```

这与以下扁平写法等价：

```bash
-p 'gpu:transform/crop/x=0;transform/crop/y=0;transform/crop/width=1920;transform/crop/height=1080;transform/rotation=90;transform/mirror=false;transform/flip=false;output/width=1280;output/height=720;output/format=NV12;output/compression=linear'
```

### 6.5 atomic OBJECT

schema 中标记为 `atomic` 的 OBJECT 需要以完整对象状态提交。CLI 会把属于同一最外层
atomic OBJECT 的子项自动聚合，只调用一次对象参数设置：

```bash
-p 'gpu:output/width=1280'
-p 'gpu:output{height=720;format=NV12;compression=linear}'
```

上例中的 `output` 会合并后一次提交。未指定成员沿用模块当前值或默认值。

该保证只覆盖单个 atomic OBJECT；不同根参数之间不是一个全局回滚事务。如果后续独立参数
设置失败，CLI 会终止本次临时管线创建，但不要把多组独立参数理解为跨模块事务。

同一完整参数路径不能重复配置，即使一次使用扁平路径、另一次使用 OBJECT 块也会返回
`-EEXIST`：

```bash
# 错误：output/width 被配置了两次
-p 'gpu:output/width=1280'
-p 'gpu:output{width=1920}'
```

`video-stack/input-layout` 是例外：可以通过多个 `-p` 参数块重复提交，每个参数块配置
一个输入布局。布局提交顺序与传入 `video-stack` 的 `-c` 连接声明顺序一一对应。

### 6.6 值类型

| 参数类型 | 常用写法 |
| --- | --- |
| Boolean | `true/false`、`1/0`、`yes/no`、`on/off`，不区分大小写 |
| Integer | 十进制或 `0x` 开头的十六进制 |
| Double | `30`、`29.97`、`0.8` |
| String | 普通文本；需要时使用引号或转义 |
| Integer enum | 优先使用 schema 输出的枚举名，也可使用合法枚举数值 |

参数系统会检查类型、读写权限、模块状态、枚举、最小值和最大值。不要依赖隐式类型转换。
枚举名称区分大小写，应直接使用 schema 中打印的名称。

CLI 会把图像参数中的整数型 `format` 和 `pixel-format` 识别为 V4L2 格式名称，名称不区分
大小写，例如 `NV12`、`NV21`、`YUV420`、`RGB24`、`BGR24`、`H264` 和 `H265`。参数
查询中的默认值和当前值也会优先显示这些名称。

十进制和十六进制 FourCC 继续兼容，例如 `format=NV12` 与
`format=0x3231564e` 等价。codec 和部分压缩字段仍需根据 schema 使用枚举名或数字：

| 含义 | 值 |
| --- | --- |
| NV12 V4L2 格式 | `NV12`，兼容 `0x3231564e` |
| `MEDIA_CODEC_VIDEO_H264` | `5` |
| `MEDIA_CODEC_VIDEO_H265` | `6` |
| Linear compression | `0` |
| AFBC 16x16 compression | `1` |

普通整数枚举是否支持名称仍应以 `params TYPE [PATH]` 是否打印 `enum:` 为准。例如：

- `mpp-dec` 的 `output/format` 可直接写 `NV12`。
- `image-processor` 的 `output/compression` 可写 `linear`。
- `image-processor` 的 `transform/rotation` 直接使用顺时针角度，当前支持 `0`、`90`、`180`、`270`，该参数不是枚举。
- `rga` 的 `rotation` 没有角度枚举名，当前 `0..5` 中旋转 90° 应写 `1`，不能写 `90`。
- `mpp-dec` 的 `output/compression` 没有枚举名，必须写 `0` 或 `1`，不能写 `linear`。
- `mpp-enc` 的 `encode/codec` 当前应写 codec 数值，例如 H.264 写 `5`。

`ffmpeg-mux` 的 `destination/format=mp4` 是容器格式字符串，音频参数中的采样 `format` 也
不是 V4L2 像素格式，不会经过上述转换。

### 6.7 Shell 引号和转义

建议始终使用单引号包住完整 `ID:PARAMETERS`，避免 shell 解释分号、花括号、`&` 等字符：

```bash
-p 'source:source{uri=rtsp://192.168.1.10/live/test;loop=0}'
```

字符串值包含分号时，可以在参数表达式内部使用双引号：

```bash
-p 'view:title="main;preview"'
```

也可以转义分号：

```bash
-p 'view:title=main\;preview'
```

被引号包住的字符串会保留首尾空格：

```bash
-p 'view:title="  Main Preview  "'
```

CLI 可识别对 `;`、`{`、`}`、反斜杠和单双引号的反斜杠转义。

## 7. FFmpeg 参数转发

`ffmpeg-demux` 和 `ffmpeg-mux` 都提供 `ffmpeg/options`，格式为逗号分隔的 FFmpeg
`AVDictionary` 项：

```bash
-p 'source:ffmpeg{options=rtsp_transport=tcp,stimeout=5000000}'
```

```bash
-p 'mux:ffmpeg{options=movflags=+faststart,flush_packets=1}'
```

输入格式可通过 `ffmpeg-demux` 的 `input-format` 指定；输出地址和格式通过
`ffmpeg-mux` 的 `destination{uri=...;format=...}` 指定：

```bash
-p 'source:input-format=rtsp;source{uri=rtsp://192.168.1.10/live/test;loop=0}'
-p 'mux:destination{uri=/data/output.mp4;format=mp4}'
```

具体可转发的 option 由当前 FFmpeg 版本、协议和 demuxer/muxer 决定。

## 8. 运行、退出和状态

```text
-d, --duration SECONDS
```

- `SECONDS > 0`：到达指定时间后停止，可使用小数。
- `SECONDS = 0` 或不指定：等待全部源模块 EOS，或等待 `SIGINT`/`SIGTERM`。
- 本地非循环文件通常会在 EOS 后自动退出。
- Camera 和网络流通常使用 `--duration`、`Ctrl+C` 或 `SIGTERM` 停止。

运行时 CLI 会打印模块状态变化，例如：

```text
[status] source -> started
[status] source -> eos
```

执行流程为：应用参数、生成拓扑顺序、连接并初始化模块、启动所有 root 源模块、等待退出条件、
停止管线并打印统计摘要。初始化或运行期间出现 `abnormal` 状态时，命令返回失败。

### 8.1 运行期参数和 seek

参数 schema 中包含的 `runtime` 表示模块 API 支持在适当运行状态下读写该参数，不表示
`ffmedia run -p` 会在启动后再次设置它。当前 CLI 的所有 `-p` 都在 `init()` 前应用，并且
没有交互式运行期 `set/get` 子命令。

因此以下参数主要供应用通过 `ModuleMedia` 参数 API 在已初始化或运行中的实例上使用：

- `file-reader/seek`：容器文件以毫秒为单位，裸流以字节偏移为单位。
- `file-reader/seek-max`：容器文件最大毫秒位置或裸流最大字节位置。
- `ffmpeg-demux/seek`：微秒时间戳。
- `ffmpeg-demux/seek-flags`：FFmpeg `AVSEEK_FLAG_*` 位掩码。
- `ffmpeg-demux/seek-max`：最大可 seek 时间戳，单位微秒。
- `ffmpeg-demux/seekable`：输入是否支持 seek。
- `status`：活动模块的实时状态。

直接在启动参数中写 `-p 'source:seek=...'` 时，依赖已打开输入的模块可能返回 `-EPIPE`。
`params` 和 `--show-params` 创建的也不是正在播放的实例，所以其中的 `status`、`seek-max`、
`seekable` 不能代替对活动实例的运行期查询。

## 9. 常用管线示例

以下命令均从仓库根目录运行。请按目标设备修改输入路径、输出路径、网络地址、Camera 格式和
DRM plane/connector 参数。

### 9.1 FFmpeg 解复用、MPP 解码并保存 NV12

```bash
LD_LIBRARY_PATH=./build ./build/ffmedia run \
  -m source=ffmpeg-demux \
  -m decoder=mpp-dec \
  -m output=file-writer \
  -p 'source:source{uri=/data/input.mp4;loop=0};buffer-count=20' \
  -p 'decoder:fast=true;buffer-count=10;output{format=NV12;compression=0}' \
  -p 'output:path=/data/output.nv12' \
  -c source@0=decoder \
  -c decoder=output
```

`source@0` 假定输入文件的 0 号输出通道是视频。若输入流顺序不同，应改为实际视频通道，或
省略 `@0` 让连接逻辑自动选择兼容输出。

### 9.2 Camera、RGA 缩放并通过 DRM 显示

```bash
LD_LIBRARY_PATH=./build ./build/ffmedia run \
  -m camera=cam \
  -m scale=rga \
  -m display=drm-display \
  -p 'camera:device=/dev/video0;capture{width=1920;height=1080;format=NV12;compression=linear};frame-rate=30' \
  -p 'scale:output{width=1280;height=720;format=NV12;compression=linear};rotation=0' \
  -p 'display:plane{format=NV12;connector=0};window-rect{x=0;y=0;width=1280;height=720}' \
  -c camera=scale \
  -c scale=display
```

Camera 必须实际支持所配置的分辨率和 FourCC。DRM 自动选择失败时，通过
`ffmedia params drm-display plane` 查询字段，再指定目标设备上的 `plane/id` 和
`plane/connector`。

### 9.3 使用 `image-processor` 配置裁剪、旋转和输出

`image-processor` 的结构参数可在一个 `-p` 中完整描述：

```bash
-m gpu=image-processor \
-p 'gpu:transform{crop{x=0;y=0;width=1920;height=1080};rotation=90;mirror=false;flip=false};output{width=720;height=1280;format=NV12;compression=linear}'
```

将其放入 Camera 或解码链路时，像普通处理节点一样连接：

```bash
-c camera=gpu -c gpu=display
```

### 9.4 解码后分支显示并编码保存

```bash
LD_LIBRARY_PATH=./build ./build/ffmedia run \
  -m source=ffmpeg-demux \
  -m decoder=mpp-dec \
  -m display=drm-display \
  -m encoder=mpp-enc \
  -m record=file-writer \
  -p 'source:source{uri=/data/input.mp4;loop=0}' \
  -p 'decoder:output{format=NV12;compression=0}' \
  -p 'encoder:encode{codec=5;fps=30;gop=60}' \
  -p 'record:path=/data/output.h264' \
  -c source@0=decoder \
  -c decoder=display \
  -c decoder=encoder \
  -c encoder=record
```

这里 `codec=5` 表示当前代码中的 H.264。若要输出 MP4、MKV、FLV 或网络协议，可把
`file-writer` 替换为 `ffmpeg-mux` 并设置 `destination`。

### 9.5 两个输入汇聚到一个 FFmpeg mux

```bash
LD_LIBRARY_PATH=./build ./build/ffmedia run \
  -m video=ffmpeg-demux \
  -m audio=ffmpeg-demux \
  -m mux=ffmpeg-mux \
  -p 'video:source{uri=/data/video.h264;loop=0}' \
  -p 'audio:source{uri=/data/audio.aac;loop=0}' \
  -p 'mux:destination{uri=/data/output.mp4;format=mp4}' \
  -c video=mux \
  -c audio=mux
```

省略通道列表时，mux 根据视频和音频输入要求匹配兼容通道。输入必须包含目标容器支持的编码，
时间戳也需要满足 muxer 要求。

### 9.6 推送 RTMP

当前 `rtmp-client` 的 `source/publish` 语义需要特别注意：

- `publish=true`：作为拉流源，是图的 root。
- `publish=false`：作为推流消费者，必须有输入连接。

下面将本地视频解码、重新编码为 H.264 后推流：

```bash
LD_LIBRARY_PATH=./build ./build/ffmedia run \
  -m source=ffmpeg-demux \
  -m decoder=mpp-dec \
  -m encoder=mpp-enc \
  -m push=rtmp-client \
  -p 'source:source{uri=/data/input.mp4;loop=0}' \
  -p 'decoder:output{format=NV12;compression=0}' \
  -p 'encoder:encode{codec=5;fps=30;gop=60}' \
  -p 'push:source{url=rtmp://192.168.1.10/live/test;publish=false};timeout{seconds=5;microseconds=0}' \
  -c source@0=decoder \
  -c decoder=encoder \
  -c encoder=push
```

### 9.7 两路视频拼接

每个 `input-layout` 必须使用独立的 `-p` 参数块。布局顺序与连接到 `stack` 的 `-c` 顺序
一致，下面将两路视频分别放在输出画面的左右两侧：

```bash
LD_LIBRARY_PATH=./build ./build/ffmedia run \
  -m source0=ffmpeg-demux -m decoder0=mpp-dec \
  -m source1=ffmpeg-demux -m decoder1=mpp-dec \
  -m stack=video-stack -m output=file-writer \
  -p 'source0:source{uri=/data/input0.mp4;loop=1}' \
  -p 'source1:source{uri=/data/input1.mp4;loop=1}' \
  -p 'stack:output{width=1280;height=720;hstride=1280;vstride=720;format=NV12;compression=linear};frame-rate=30;background-color{r=20;g=20;b=20}' \
  -p 'stack:input-layout{input-id=0;crop{x=0;y=0;width=640;height=720}}' \
  -p 'stack:input-layout{input-id=1;crop{x=640;y=0;width=640;height=720}}' \
  -p 'output:path=/data/video-stack.nv12' \
  -c source0@0=decoder0 -c source1@0=decoder1 \
  -c decoder0=stack -c decoder1=stack \
  -c stack=output \
  --duration 10
```

## 10. 错误信息和排查

模块参数设置失败时，CLI 会打印失败模块、参数路径、返回值和该模块的完整参数 schema。
连接失败时，还会打印生产者输出通道和消费者输入要求。

| 返回值或提示 | 常见原因 | 处理方式 |
| --- | --- | --- |
| `Unknown or disabled module type` | 类型名错误或当前 SDK 中无此模块 | 先执行 `ffmedia modules` |
| `-ENOENT` | 模块 ID、参数路径或连接节点不存在 | 用 `params` 核对路径和拼写 |
| `-EEXIST` | 模块 ID、参数路径或连接边重复 | 删除重复声明；多个通道合并到一个 `-c` |
| `-EACCES` | 尝试写只读参数 | 查看 schema 的 `r/w` 标记 |
| `-ERANGE` | 数值超出最小值或最大值 | 查看 schema 的 `min/max` |
| `-EINVAL` | 类型、枚举、图结构或模块配置不合法 | 检查枚举名、数字格式和输入输出兼容性 |
| `-ENOTSUP` | 直接给 OBJECT 赋字符串，或使用 special 模块运行 | 改用子路径/OBJECT 块，或编写专用适配程序 |
| `-EBUSY` | 当前模块状态不允许写参数 | 查看 schema 的 `states` 和 `apply` |
| `-EPIPE` | 参数依赖尚未初始化或打开的媒体资源 | 在活动模块上通过 API 执行运行期操作 |
| `Module graph contains a cycle` | 连接形成环路 | 调整 `-c` 使图保持有向无环 |
| 动态库找不到 | `LD_LIBRARY_PATH` 未包含库目录 | 设置 `LD_LIBRARY_PATH=./build` 或 `.` |

建议在复杂命令中先使用 `--show-params` 分别检查每个模块，再添加连接并正式运行。对于
codec、FourCC、DRM ID、Camera 格式和 FFmpeg option，不要仅凭名称猜测，应以当前 SDK 的
schema、目标设备能力和运行日志为准。
