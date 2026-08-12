# FFMedia Demo 使用指南

`demo/` 目录提供 FFMedia 输入（VI）、处理（VP）和输出（VO）模块的组合示例，覆盖
文件、Camera、RTSP/RTMP、硬件编解码、RGA、DRM、X11、OpenCV、音频、推流、录制和
多路拼接等场景。

源码仓库中的路径为 `demo/`，发布 SDK 中对应路径为 `examples/demo/`。下文命令出现
`demo/...` 时，在发布 SDK 中替换为 `examples/demo/...`。

## 1. 示例一览

| 示例 | 类型 | 主要用途 | 是否需要修改源码 |
| --- | --- | --- | --- |
| `ffmedia.cpp` | C++ | 基于统一参数系统的通用模块图 CLI，构建为 `build/ffmedia` | 否 |
| `ffmedia.py` | Python | 与 C++ 版使用相同参数和模块图语法，依赖 `ff_pymedia` | 否 |
| `demo.cpp` | C++ | 通用命令行示例，可动态组合大部分模块 | 否 |
| `demo_simple.cpp` | C++ | RTSP 多通道匹配、MPP 解码、DRM 显示的最小示例 | 否 |
| `demo_memory_read.cpp` | C++ | 从应用内存逐帧送入 H.264 数据并解码显示 | 否 |
| `demo_multi_drmplane.cpp` | C++ | 一个解码源同时显示到多个 DRM plane/window | 是，修改 RTSP 地址 |
| `demo_multi_window.cpp` | C++ | 同一 DRM plane 上动态切换 1/4/6/9/16 窗口布局 | 是，修改 RTSP 地址 |
| `demo_video_stack.cpp` | C++ | 多路 RTSP 解码、拼接、显示并重新编码为 RTSP | 是，修改地址和常量 |
| `demo_opencv.cpp` | C++/OpenCV | 在 RGA 生产钩子中使用 OpenCV 显示 | 是，修改 RTSP 地址 |
| `demo_opencv_multi.cpp` | C++/OpenCV | 使用多个外部消费者读取同一路 RGA 输出 | 是，修改 RTSP 地址 |
| `demo_rgablend.cpp` | C++/OpenCV | OpenCV 生成动态图层，RGA 混合后 DRM 显示 | 是，修改 RTSP 地址 |
| `demo.py` | Python | Python 版通用管线组合示例 | 否，部分回调路径见兼容说明 |
| `demo_opencv.py` | Python/OpenCV | Python/OpenCV 独立显示线程和缓冲池交换示例 | 否，当前绑定下需适配旧接口 |

`utils.cpp/.hpp` 是 `demo.cpp` 使用的原始图像落盘辅助代码，不单独生成程序。

`ffmedia.cpp` 和 `ffmedia.py` 分别提供 C++ 与 Python 版本的统一参数 CLI，两者均支持
`modules`、`params`、`run` 子命令，并使用相同的模块声明、参数配置和连接语法：

```bash
./build/ffmedia --help
LD_LIBRARY_PATH="$PWD/lib" ./bin/ffmedia --help  # 发布 SDK 的预编译程序
python3 demo/ffmedia.py --help                    # 安装匹配 wheel 后
```

Python 解释器版本必须与 `ff_pymedia` 扩展的 ABI 匹配。完整的构建方式、命令语法、
参数配置和运行示例请参见 [`ffmedia` 命令行使用介绍](ffmedia.md)。

## 2. 按需加载的运行库

以下模块只在实际使用时才加载运行库；未使用这些模块时不要求安装。
运行库需位于系统动态库搜索路径或 `LD_LIBRARY_PATH` 中。

| 模块或场景 | 按需加载的库 | 常见运行时包或来源 |
| --- | --- | --- |
| `mpp-dec`、`mpp-enc` | `librockchip_mpp.so.1`、`.so.0` 或 `.so` | Firefly/Rockchip 的 `librockchip-mpp1` |
| `rga`、`video-stack`、`inference` | `librga.so.2`、`.so.1` 或 `.so` | Firefly/Rockchip 的 `librga2` |
| `alsa-capture`、`alsa-playback` | `libasound.so.2` 或 `.so` | `libasound2` |
| `aac-dec`、`aac-enc` | `libfdk-aac.so.2`、`.so.1`、`.so.0`、`.so` 或 `.so.1.0.1` | 发行版或板卡 SDK 提供的 FDK-AAC 运行库 |
| `inference` | `librknnrt.so` 或 `.so.1` | 与 RK356x、RK3588 及 RKNN Toolkit 版本匹配的板卡 SDK |
| `renderer-video` 使用 X11 后端 | `libX11.so.6` 或 `.so` | `libx11-6` |
| `renderer-video` 使用 Wayland 后端 | `libwayland-client.so.0`、`libwayland-egl.so.1`，或对应 `.so` | `libwayland-client0 libwayland-egl1` |

MPP、RGA 和 FDK-AAC 可分别通过 `FFMEDIA_MPP_LIBRARY`、`FFMEDIA_RGA_LIBRARY`、
`FFMEDIA_FDK_AAC_LIBRARY` 指定自定义运行库路径。`renderer-video` 的显示后端由
`FFMEDIA_DISPLAY_BACKEND=x11` 或 `wayland` 选择。

## 3. 编译

### 3.1 仅编译基础 C++ Demo

如果只需要通用、DRM、内存输入和 VideoStack 示例：

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

生成：

```text
build/ffmedia
build/demo
build/demo_simple
build/demo_memory_read
build/demo_multi_drmplane
build/demo_multi_window
build/demo_video_stack
```

### 3.2 编译全部 `demo/` C++ 示例

```bash
cmake -S . -B build \
    -DDEMO_OPENCV=ON \
cmake --build build -j$(nproc)
```

额外生成：

```text
build/demo_opencv
build/demo_opencv_multi
build/demo_rgablend
```

## 4. 通用 C++ 示例：`demo`

### 4.1 基本语法

```bash
./build/demo <输入源> [选项]
```

输入源可以是：

- 普通媒体文件：MP4、MKV、FLV、TS、PS、JPEG、H.264/H.265 裸流等。
- 原始图像文件：需要通过 `-i` 和 `-a` 指定尺寸、格式。
- V4L2 Camera：如 `/dev/video0`。
- RTSP 地址：`rtsp://...`。
- RTMP 地址：`rtmp://...`。
- FFmpeg 可识别的其他 URL 或设备。

程序根据输入类型自动选择 `ModuleFileReader`、`ModuleCam`、`ModuleRtspClient`、
`ModuleRtmpClient` 或 `ModuleFFmpegDemux`，之后按选项依次组合：

```text
输入源
  -> 可选 MPP 解码
  -> 可选 RGA 缩放/格式转换/旋转
  +-> 可选 DRM 或 X11 显示
  -> 可选 MPP 编码
  +-> 文件/Mux 输出
  +-> RTSP/RTMP Server
  +-> RTMP Client 推流
  +-> GB28181
```

本地文件到达 EOS 后会自动退出。网络流通常使用 `Ctrl+C` 或 `SIGTERM` 停止。

### 4.2 输入和图像处理参数

| 参数 | 说明 | 示例 |
| --- | --- | --- |
| `-i, --input WxH` | 指定 Camera 或裸流输入尺寸。 | `-i 1920x1080` |
| `-a, --inputfmt FORMAT` | 指定输入像素或编码格式。 | `-a NV12`、`-a H264` |
| `-o, --output WxH` | 指定 RGA 输出尺寸；未指定时沿用输入尺寸。 | `-o 1280x720` |
| `-b, --outputfmt FORMAT` | 指定 RGA 输出格式。 | `-b NV12`、`-b BGR24` |
| `-r, --rotate VALUE` | 旋转或镜像。 | `-r 90` |
| `--dec_disabled` | 禁用自动视频解码，常用于转封装或直接转发编码流。 | 见转封装示例 |
| `-l, --loop` | 循环读取本地媒体文件。 | `-l` |
| `-c, --count N` | 创建 N 份实例，DRM 显示时自动分格布局。 | `-c 4` |

`-r` 支持：

| 值 | 行为 |
| --- | --- |
| `0` | 不旋转。 |
| `1` | 垂直镜像。 |
| `2` | 水平镜像。 |
| `90` | 旋转 90°。 |
| `180` | 旋转 180°。 |
| `270` | 旋转 270°。 |

格式名称不区分大小写。当前像素格式表包含 `NV12`、`NV21`、`NV16`、`YUV420`、
`RGB24`、`BGR24`、`BGRA32`、`MJPEG`、`H264`、`H265`、`VP8` 和 `VP9` 等。

### 4.3 显示参数

| 参数 | 说明 |
| --- | --- |
| `-d, --drmdisplay PLANE_ID` | 启用 DRM 显示；`0` 表示自动选择 plane。 |
| `--connector CONNECTOR_ID` | 指定 DRM connector；`0` 表示自动选择。 |
| `-z, --zpos ZPOS` | 指定 DRM plane 层级，默认自动选择。 |
| `-x, --x11` | 使用窗口显示后端和 EGL/GLES；默认后端为 X11。 |

DRM 显示通常要求进程可访问 `/dev/dri/card*`。X11 显示要求有效的 `DISPLAY`：

```bash
export DISPLAY=:0
```

使用 Wayland 后端时设置：

```bash
export FFMEDIA_DISPLAY_BACKEND=wayland
export XDG_RUNTIME_DIR=/run/user/$(id -u)
export WAYLAND_DISPLAY=wayland-0
```

Wayland 窗口的位置由 compositor 管理，`setWindowRect()` 的 `x/y` 参数不会强制改变窗口位置。

### 4.4 编码、保存和推流参数

| 参数 | 说明 |
| --- | --- |
| `-e, --encodetype h264|h265|mjpeg` | 启用 MPP 编码。参数字符串包含 `264`、`265` 或 `jpeg` 即可。 |
| `-f, --file PATH` | 将输入源模块的原始输出直接 dump 到文件，位于解码和处理之前。 |
| `-m, --enmux URI` | 将当前管线最后一个模块写入文件、管道或网络 URI。 |
| `--port PORT` | 启动内置 RTSP/RTMP Server。当前代码没有 `-p` 短选项。 |
| `--push_type rtsp|rtmp` | 配合 `--port` 选择内置服务类型，默认 RTSP。 |
| `--push_path PATH` | 内置服务路径，默认按实例生成 `/live/N`。 |
| `--rtmp_url URL` | 使用 `ModuleRtmpClient` 向已有 RTMP 服务推流。 |
| `--rtsp_transport udp|tcp|multicast` | 选择 RTSP 拉流协议，默认 UDP。 |

`-f` 与 `-m` 的区别：

- `-f` 通过源模块生产钩子直接 dump 输入数据，不经过解码、RGA 或编码。
- `-m` 在当前管线末端添加 Writer/Muxer，保存处理后的数据。

### 4.5 FFmpeg 参数

| 参数 | 说明 |
| --- | --- |
| `--use_ffmpeg_demux` | 强制使用 `ModuleFFmpegDemux`，输入格式自动探测。 |
| `--use_ffmpeg_demux=FORMAT` | 强制 FFmpeg demux 并指定输入格式，如 `rtsp`、`whep`、`kmsgrab`。 |
| `--use_ffmpeg_mux` | 使用 `ModuleFFmpegMux`，输出格式从 URI 或扩展名推断。 |
| `--use_ffmpeg_mux=FORMAT` | 使用 FFmpeg mux 并指定 `mp4`、`rtsp`、`rawvideo`、`whip` 等格式。 |

上述参数在 C++ 中是“可选参数”长选项。需要指定格式时应使用等号，例如
`--use_ffmpeg_mux=rtsp`，不要写成 `--use_ffmpeg_mux rtsp`。

### 4.6 音频和同步参数

| 参数 | 说明 |
| --- | --- |
| `--audio` | 请求输入、封装或 RTSP 输出中的音频通道。 |
| `--aplay DEVICE` | 解码 AAC 并通过指定 ALSA 设备播放。 |
| `--arecord DEVICE` | 从指定 ALSA 设备采集 PCM，并编码为 AAC。 |
| `-s` | 启用默认的音频时钟同步。 |
| `--sync=video` | 使用视频时钟同步。 |
| `--sync=abs` | 使用绝对时钟同步。 |

`--sync` 也是可选参数长选项，指定模式时必须使用等号。查看 ALSA 设备：

```bash
aplay -l
arecord -l
```

### 4.7 GB28181 参数

```text
--gb28181_user_id ID
--gb28181_server_id ID
--gb28181_server_ip IP
--gb28181_server_port PORT
```

只要设置 `--gb28181_user_id` 等任意 GB28181 配置项就会启用 GB28181 模块。实际运行时
应完整提供本地用户 ID、服务器 ID、服务器 IP 和端口。

## 5. `demo` 常用场景

以下命令中的地址、设备节点和 ALSA 设备名均需替换为实际值。

### 5.1 播放本地文件

DRM 显示：

```bash
./build/demo input.mp4 -d 0
```

X11 显示并按视频时钟同步：

```bash
./build/demo input.mp4 -x --sync=video
```

强制使用 FFmpeg 解封装：

```bash
./build/demo input.mp4 --use_ffmpeg_demux -d 0
```

循环播放，并播放音频：

```bash
./build/demo input.mp4 -d 0 -l --audio --aplay plughw:3,0 -s
```

### 5.2 Camera 预览

```bash
./build/demo /dev/video0 \
    -i 1920x1080 \
    -a NV12 \
    -d 0
```

Camera 实际支持的分辨率和格式应先用 `v4l2-ctl` 查询：

```bash
v4l2-ctl -d /dev/video0 --list-formats-ext
```

### 5.3 RTSP 拉流

TCP 拉流、解码并 DRM 显示：

```bash
./build/demo 'rtsp://user:password@host/path' \
    --rtsp_transport tcp \
    -d 0 \
    --sync=video
```

使用 FFmpeg 拉取 RTSP：

```bash
./build/demo 'rtsp://host/path' \
    --use_ffmpeg_demux=rtsp \
    --rtsp_transport udp \
    -d 0
```

同一地址创建 4 个独立拉流实例并分格显示：

```bash
./build/demo 'rtsp://host/path' -d 0 -c 4
```

`-c` 会独立创建多套拉流和解码管线，资源占用随实例数增加。

### 5.4 RTMP 拉流

```bash
./build/demo 'rtmp://server/app/stream' -d 0 --sync=video
```

强制使用 FFmpeg：

```bash
./build/demo 'rtmp://server/app/stream' \
    --use_ffmpeg_demux=rtmp \
    -d 0
```

### 5.5 缩放、格式转换和旋转

将视频缩放到 1280×720、转换成 BGR24、旋转 180°，然后用 X11 显示：

```bash
./build/demo input.mp4 \
    -o 1280x720 \
    -b BGR24 \
    -r 180 \
    -x
```

RGA 只会在尺寸、格式或旋转参数发生变化时自动插入管线。

### 5.6 原始图像编码

将 1920×1080 NV12 裸流编码为 H.264：

```bash
./build/demo input.nv12 \
    -i 1920x1080 \
    -a NV12 \
    -e h264 \
    -m output.h264
```

输入文件必须是连续、无文件头的原始帧，尺寸和格式必须与参数一致。

### 5.7 转封装

不解码，将 MP4 中的编码数据转封装为 MKV：

```bash
./build/demo input.mp4 --dec_disabled -m output.mkv
```

使用 FFmpeg demux/mux：

```bash
./build/demo input.mp4 \
    --use_ffmpeg_demux \
    --dec_disabled \
    --use_ffmpeg_mux \
    -m output.mkv
```

只有输入编码与目标容器兼容时才能直接转封装；否则应先解码并重新编码。

### 5.8 转码并保存

```bash
./build/demo input.mp4 \
    -o 1280x720 \
    -e h265 \
    -m output.mp4
```

### 5.9 内置 RTSP Server

直接转发文件中的压缩视频：

```bash
./build/demo input.mp4 \
    --dec_disabled \
    --port 8554 \
    --push_path /live/test \
    --sync=video
```

客户端播放：

```text
rtsp://<设备IP>:8554/live/test
```

拉流后重新编码再转发：

```bash
./build/demo 'rtsp://upstream/path' \
    -e h265 \
    --port 8554 \
    --push_path /live/test
```

### 5.10 内置 RTMP Server

```bash
./build/demo input.mp4 \
    --dec_disabled \
    --push_type rtmp \
    --port 1935 \
    --push_path /live/test
```

输出地址：

```text
rtmp://<设备IP>:1935/live/test
```

### 5.11 推送到已有 RTMP 服务

```bash
./build/demo input.mp4 \
    -e h264 \
    --rtmp_url 'rtmp://server/app/stream' \
    --sync=video
```

也可使用 FFmpeg mux：

```bash
./build/demo input.mp4 \
    -e h264 \
    --use_ffmpeg_mux=rtmp \
    -m 'rtmp://server/app/stream'
```

### 5.12 推送到已有 RTSP 服务

```bash
./build/demo input.mp4 \
    -e h264 \
    --use_ffmpeg_mux=rtsp \
    -m 'rtsp://server:8554/live/test' \
    --sync=video
```

### 5.13 管道输入输出

将解码后的原始视频输出到标准输出：

```bash
./build/demo 'rtsp://host/path' \
    --use_ffmpeg_mux=rawvideo \
    -m pipe:1
```

输出是二进制数据，日志也可能写入终端。用于脚本管道时应根据项目日志配置处理标准输出
和标准错误，避免污染媒体数据。

### 5.14 WHEP/WHIP

WHEP 拉流：

```bash
./build/demo 'http://server/path' \
    --use_ffmpeg_demux=whep \
    -d 0 \
    --sync=video
```

WHIP 推流：

```bash
./build/demo input.mp4 \
    -e h264 \
    --use_ffmpeg_mux=whip \
    -m 'http://server/path'
```

鉴权 token 目前需要在代码中通过 `ModuleFFmpegDemux::setFormatOption()` 或
`ModuleFFmpegMux::setFormatOption()` 设置。

### 5.15 KMSGrab 录屏

```bash
./build/demo /dev/dri/card0 \
    --use_ffmpeg_demux=kmsgrab \
    -x \
    --sync=video
```

该功能依赖 FFmpeg 的 `kmsgrab` 支持和 DRM 访问权限。

### 5.16 音视频采集并录制

Camera 编码为 H.265，同时采集 ALSA 音频并编码 AAC，最后封装 MP4：

```bash
./build/demo /dev/video0 \
    -i 1920x1080 \
    -a NV12 \
    -e h265 \
    --arecord plughw:2,0 \
    --audio \
    -m output.mp4
```

## 6. 专用 C++ 示例

### 6.1 `demo_simple`

最小的通道连接示例：

```text
ModuleRtspClient
  -> connectProducer()
ModuleMppDec
  -> connectProducer()
ModuleDrmDisplay
```

它会打印 RTSP 源发布的所有媒体通道，并显示解码器最终匹配到的生产者通道。解码器只声明
压缩视频输入，因此会自动忽略 RTSP 音频通道。

```bash
./build/demo_simple 'rtsp://user:password@host/path'
```

该示例固定使用 TCP 拉流。按 Enter 停止。

### 6.2 `demo_memory_read`

该示例演示业务程序如何把内存中的 H.264 NAL 数据送给 `ModuleMemReader`：

```text
H.264 Annex-B 文件
  -> H264ReadFrame()
  -> ModuleMemReader::setInputBuffer()
  -> ModuleMppDec
  -> ModuleDrmDisplay
```

运行：

```bash
./build/demo_memory_read input.h264 1920 1080
```

说明：

- 输入必须是带 `00 00 00 01` 起始码的 H.264 Annex-B 裸流。
- 宽高用于构造媒体参数，不会从文件中自动探测。
- 示例每次送入一段 NAL 数据，并通过 `waitProcess(2000)` 等待处理完成。
- 示例使用约 30ms 的固定送帧间隔，仅用于演示，不代表源文件真实帧率。

### 6.3 `demo_multi_drmplane`

将同一解码视频连接到四个 `ModuleDrmDisplay`，演示：

- 多个 zpos plane。
- 同一 plane 上的多个 window。
- `setPlaneRect()` 与 `setWindowRect()`。
- 运行时调用 `move()` 移动窗口。

运行前修改 `demo/demo_multi_drmplane.cpp` 中硬编码的 RTSP 地址：

```bash
cmake --build build --target demo_multi_drmplane -j$(nproc)
./build/demo_multi_drmplane
```

程序会先移动第四个显示窗口一段时间，然后等待 Enter 退出。实际 plane 数量、格式和 zpos
能力取决于具体 DRM 驱动。

### 6.4 `demo_multi_window`

该示例在同一个 `DrmDisplayPlane` 上创建 16 个窗口，所有窗口显示同一解码流。运行前修改
`demo/demo_multi_window.cpp` 中的 RTSP 地址。

```bash
./build/demo_multi_window
```

运行时按键：

| 按键 | 操作 |
| --- | --- |
| `1` | 让窗口 0 占满当前 plane。 |
| `t` | 从“窗口占满 plane”恢复。 |
| `4` | 切换为 2×2、4 窗口布局。 |
| `6` | 切换为 6 窗口布局。 |
| `9` | 切换为 3×3、9 窗口布局。 |
| `m` | 切换为 4×4、16 窗口布局。 |
| `f` | 让 plane 全屏。 |
| `r` | 恢复 plane 原始区域。 |
| `F` | 让窗口 0 全屏。 |
| `R` | 恢复窗口 0。 |
| `q` | 退出。 |

窗口数是显示分支数，不是 RTSP 拉流数；整个示例只有一套 RTSP 拉流和解码管线。

### 6.5 `demo_video_stack`

多路拼接管线：

```text
RTSP 0 -> Decoder 0 --+
RTSP 1 -> Decoder 1 --+
RTSP 2 -> Decoder 2 --+-> ModuleVideoStack -> DRM Display
RTSP 3 -> Decoder 3 --+                   +-> MPP Encoder -> RTSP Server
```

运行前修改 `demo/demo_video_stack.cpp` 中：

- `rtspUrl[]`：输入流地址。
- `STACK_WIDTH`、`STACK_HEIGHT`、`STACK_FPS`：拼接输出参数。
- `rtspTransport`：输入 RTSP 传输方式。
- `encodeType`：输出编码格式。
- `rtspPushPort`、`rtspPushPath`：输出 RTSP 地址。
- `TEST_DISPLAY`、`TEST_PUSH_STREAM`：是否编译显示和推流分支。

```bash
cmake --build build --target demo_video_stack -j$(nproc)
./build/demo_video_stack
```

默认输出 RTSP 地址为：

```text
rtsp://<设备IP>:8554/live/1
```

程序根据输入数量自动计算接近方形的网格，并把每路视频缩放到对应区域。按 Enter 退出。

### 6.6 `demo_opencv`

运行前修改 `demo/demo_opencv.cpp` 中的 RTSP 地址，并确保构建时启用
`DEMO_OPENCV=ON`。

```text
RTSP -> MPP Decoder -> RGA(BGR24、1/2 尺寸)
                              |
                              +-> Produce Hook -> cv::imshow()
```

```bash
./build/demo_opencv
```

OpenCV 回调运行在 RGA 工作线程内。`cv::imshow()` 或其他回调处理过慢会直接降低管线吞吐。
按 Enter 停止。

### 6.7 `demo_opencv_multi`

该示例不使用单一生产钩子，而是通过 `addExternalConsumer()` 创建两个外部消费模块：

```text
RGA output +-> external_test1 -> OpenCV window 1
           +-> external_test2 -> OpenCV window 2
```

每个外部消费者拥有独立的输入队列和工作线程，更适合演示一份 Buffer 向多个业务分支扇出。
运行前修改 `demo/demo_opencv_multi.cpp` 中的 RTSP 地址：

```bash
./build/demo_opencv_multi
```

按 Enter 停止。

### 6.8 `demo_rgablend`

该示例使用 OpenCV 在 BGRA Buffer 上绘制当前时间戳，再把该 Buffer 设置为 RGA pattern，
与解码视频混合后送到 DRM：

```text
RTSP -> Decoder -> RGA blend -> DRM Display
                    ^
                    |
          OpenCV 动态 BGRA 时间戳图层
```

运行前修改 `demo/demo_rgablend.cpp` 中的 RTSP 地址：

```bash
./build/demo_rgablend
```

按 Enter 停止。该示例同时要求 OpenCV、RGA 和 DRM Buffer 支持。

## 7. Python 示例

### 7.1 构建和加载 `ff_pymedia`

源码仓库可直接构建扩展：

```bash
cmake -S . -B build \
    -DENABLE_PYTHON=ON
cmake --build build -j$(nproc)
```

将扩展模块目录加入 `PYTHONPATH`：

```bash
export PYTHONPATH="$PWD/build:$PYTHONPATH"
python3 -c 'import ff_pymedia; print(ff_pymedia)'
```

发布 SDK 不包含 Python 扩展源码，应安装 `python/` 目录中与解释器 ABI 匹配的 wheel：

```bash
python3 --version
python3 -m pip install python/ff_pymedia-2.6.0-cp310-cp310-linux_aarch64.whl
# Python 3.11 使用 ff_pymedia-2.6.0-cp311-cp311-linux_aarch64.whl
python3 -c 'import ff_pymedia; print(ff_pymedia.__file__)'
```

扩展文件名包含 Python ABI，例如 `ff_pymedia.cpython-38-aarch64-linux-gnu.so`。运行 Python
版本必须与构建时使用的版本兼容。

Python 示例还需要：

```bash
python3 -m pip install numpy
python3 -m pip install opencv-python  # 使用 OpenCV 显示时
```

在没有桌面环境的嵌入式系统上，可使用发行版提供的 OpenCV Python 包，或跳过 OpenCV
显示路径。

### 7.2 Python 数值枚举

Python CLI 将部分枚举直接定义为整数：

| 参数 | 值 |
| --- | --- |
| `--rtsp_transport` | `0=UDP`，`1=TCP`，`2=Multicast` |
| `-e, --encodetype` | `-1=禁用`，`0=H.264`，`1=H.265`，`2=MJPEG` |
| `-r, --rotate` | `0=无`，`1=90°`，`2=180°`，`3=270°`，`4=垂直镜像`，`5=水平镜像` |
| `-s, --sync` | `-1=禁用`，`0=视频时钟`，`1=音频时钟`，`2=绝对时钟` |
| `--push_type` | `0=RTSP`，非 `0=RTMP` |

注意：Python 的旋转值是 `RgaRotate` 枚举序号，与 C++ `demo` 使用的角度参数不同。

### 7.3 `demo.py`

基本语法：

```bash
python3 demo/demo.py -i <输入源> [选项]
```

输入源必须使用 `-i/--input_source`，不能像 C++ `demo` 一样直接写成位置参数。

常用示例：

以下 OpenCV 命令展示参数用法，但需要先按 7.4 节适配当前回调接口；其余命令不依赖旧
回调接口。

```bash
# 本地文件 DRM 显示
python3 demo/demo.py -i input.mp4 -d 0

# RTSP TCP 拉流、缩放、旋转 90°并 DRM 显示
python3 demo/demo.py \
    -i 'rtsp://host/path' \
    --rtsp_transport 1 \
    -o 1280x720 \
    -r 1 \
    -d 0 \
    -s 0

# 转换为 BGR24，打开两个 OpenCV 窗口
python3 demo/demo.py \
    -i input.mp4 \
    -b BGR24 \
    -c 2

# 编码 H.264 并保存 MP4
python3 demo/demo.py \
    -i input.mp4 \
    -e 0 \
    -m output.mp4

# 使用 FFmpeg Mux 推送 RTSP
python3 demo/demo.py \
    -i input.mp4 \
    -e 0 \
    --use_ffmpeg_mux rtsp \
    -m 'rtsp://server:8554/live/test'
```

`demo.py` 启动后会停在 `input("wait...")`，按 Enter 才会执行停止和资源回收。

Python `argparse` 中 `--audio` 和 `-x/--x11display` 当前使用 `type=bool`，启用时需要显式
提供非空值，例如：

```bash
python3 demo/demo.py -i input.mp4 -x True
python3 demo/demo.py -i input.mp4 --audio True --aplay plughw:3,0 -d 0
```

不要传 `False` 试图关闭，因为 Python 的 `bool("False")` 仍为 `True`；不写该参数即可关闭。

### 7.4 Python 回调兼容说明

当前 `module/pymodule.cpp` 已提供 `addExternalConsumer(name, callback)`，但仓库中的 Python
示例仍有少量旧绑定调用：

- `demo.py` 的 `--save_file` 路径调用旧的 `setOutputDataCallback()`。
- `demo.py` 的 OpenCV 路径按旧的三参数形式调用 `addExternalConsumer()`。
- `demo_opencv.py` 使用旧的 `setOutputDataCallback()` 和 `setAlsaDevice()`。

这些方法或调用形式不在当前绑定中。使用当前源码重新构建 `ff_pymedia` 时，相关路径需要
迁移到当前接口；DRM、X11、编解码、Mux 等不经过这些旧回调的路径不受该问题影响。

建议的新外部消费者形态为：

```python
def on_buffer(module_name, queue_size, buffer):
    # 同步处理 buffer；异步持有时需遵守 FFMedia Buffer 生命周期规则
    pass

external = last_module.addExternalConsumer("python-consumer", on_buffer)
```

### 7.5 `demo_opencv.py`

该文件展示的设计思路是：

1. 在模块回调中导出缓冲池 Buffer。
2. 把 Buffer 交给独立 `Cv2Display` 线程显示。
3. 下一帧到达前把上一帧重新导入缓冲池。

它用于说明跨线程处理时的缓冲池交换方式，但按当前 Python 绑定直接运行前，需要根据上一节
替换旧回调和音频接口。修正后可按其参数入口运行：

```bash
python3 demo/demo_opencv.py \
    -i 'rtsp://host/path' \
    --rtsp_transport 1 \
    -b BGR24 \
    -c 1
```

## 8. 常见问题

### 8.1 DRM 初始化失败

检查：

```bash
ls -l /dev/dri/
modetest -c
modetest -p
```

确认进程有权限访问 DRM 节点，connector/plane ID 存在，并且所选 plane 支持 NV12。

### 8.2 Camera 初始化失败

```bash
v4l2-ctl -d /dev/video0 --all
v4l2-ctl -d /dev/video0 --list-formats-ext
```

`-i` 和 `-a` 必须与设备实际支持的模式一致。

### 8.3 RTSP/RTMP 无法连接

- 检查 URL、用户名和密码。
- 检查设备到服务器的网络连通性和防火墙。
- RTSP UDP 不稳定时改用 `--rtsp_transport tcp`。
- 确认输入编码是当前 MPP 解码器支持的格式。

### 8.4 多路实例初始化失败

多路解码、DRM 和网络连接会消耗大量文件描述符：

```bash
ulimit -n
ulimit -n 102400
```

同时检查 MPP Buffer、DRM plane、内存带宽和解码实例数是否超过芯片能力。

### 8.5 程序卡在输出 Buffer

运行结束前 Demo 会打印 `dumpPipeSummary()`。如果 `Out Full` 持续增加，通常表示下游处理
过慢或 Buffer 被外部代码长期持有。OpenCV 回调、磁盘写入和网络阻塞是常见原因。

### 8.6 X11、Wayland 或 OpenCV 没有窗口

- X11 检查 `DISPLAY` 是否正确，并确认存在 X Server。
- Wayland 检查 `FFMEDIA_DISPLAY_BACKEND`、`XDG_RUNTIME_DIR` 和 `WAYLAND_DISPLAY`。
- 确认当前会话有可访问的图形 compositor。
- 远程运行时检查 X11 转发权限。
- OpenCV 输入必须为 BGR24；Python 示例使用 `-b BGR24`。

### 8.7 Python 无法导入 `ff_pymedia`

源码仓库构建方式：

```bash
find build -maxdepth 1 -name 'ff_pymedia*.so'
export PYTHONPATH="$PWD/build:$PYTHONPATH"
python3 -c 'import ff_pymedia'
```

发布 SDK 使用 wheel：

```bash
ls python/ff_pymedia-*.whl
python3 -m pip install --force-reinstall python/ff_pymedia-2.6.0-cp310-cp310-linux_aarch64.whl
```

若仍失败，检查 Python ABI、CPU 架构以及扩展所依赖的动态库。

## 9. 阅读源码建议

建议按以下顺序理解 Demo：

1. `demo_simple.cpp`：理解 `init()`、`connectProducer()`、`start()` 和 `stop()`。
2. `demo_memory_read.cpp`：理解业务内存如何进入 FFMedia 管线。
3. `demo.cpp`：理解模块如何根据参数动态组合。
4. `demo_opencv_multi.cpp`：理解一个生产者向多个外部消费者扇出。
5. `demo_multi_window.cpp`：理解 DRM plane 与 window 的关系。
6. `demo_video_stack.cpp`：理解多路输入聚合和一份输出连接多个下游。

更多接口和核心机制说明：

- [`../docs/ffmedia_api.md`](../docs/ffmedia_api.md)
- [`../docs/module_media.md`](../docs/module_media.md)
