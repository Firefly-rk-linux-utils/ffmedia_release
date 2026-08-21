# FFMedia 发布包测试

发布包中的本目录位于 `examples/tests/`，用于验证 FFMedia SDK 的接口、输入源、硬件编解码、
图像处理和显示能力。以下命令均假定当前目录是发布包根目录。

## 1. 编译测试程序

发布包自带顶层 `CMakeLists.txt`，直接使用安装好的 `FFMedia::FFMedia` 目标：

```bash
cmake -S . -B build -DENABLE_TESTS=ON
cmake --build build -j
```

编译后的测试程序位于 `build/examples/tests/`。运行前可设置运行时库路径：

```bash
test_dir=./build/examples/tests
export LD_LIBRARY_PATH=./lib
```

如果 FFMedia 库已经安装到系统库目录，可以不设置 `LD_LIBRARY_PATH`。

## 2. 测试分类

| 类型 | 测试/程序 | 主要用途 | 运行条件 |
| --- | --- | --- | --- |
| CTest | `media_parameter` | 参数定义、查询和更新 | CPU 环境 |
| CTest | `media_channel` | 通道连接、选择和生命周期 | CPU 环境 |
| CTest | `module_app_source` | 应用输入源模块 | CPU 环境 |
| CTest | `module_app_processor` | 应用处理模块 | CPU 环境 |
| VI | `test_file_reader` | 文件读取、循环和 seek | 媒体文件 |
| VI | `test_ffmpeg_demux` | 文件、网络流或设备解封装 | 输入文件或 URL |
| VI | `test_camera` | V4L2 摄像头采集 | 摄像头设备 |
| VI | `test_rtsp_client` | RTSP 拉流 | RTSP 服务 |
| VP | `test_mpp_decoder` | MPP 硬件解码 | Rockchip MPP |
| VP | `test_mpp_encoder` | MPP 硬件编码 | Rockchip MPP |
| VP | `test_module_imageProcessor` | 格式转换、缩放、裁剪和旋转 | RGA/GPU 能力（视配置而定） |
| VO | `test_module_drmDisplay` | 解码后 DRM/KMS 显示 | 显示设备和 DRM 资源 |

所有 C++ 手动测试都支持 `--help`。首次运行建议先查看帮助，再使用有限的帧数或时长。

## 3. 运行 CPU 自动测试

自动测试不需要摄像头、显示器或媒体硬件：

```bash
(cd build && ctest --output-on-failure)
```

进程退出码为 `0` 表示成功，非 `0` 表示参数、初始化或运行过程失败。

## 4. C++ 手动测试

### 4.1 输入源（VI）

文件读取模块输出文件中的媒体 buffer，不负责显示：

```bash
$test_dir/test_file_reader sample.mp4 --frames 300
$test_dir/test_file_reader sample.mp4 \
  --seek 1000 --runtime-seek 5000 --frames 300
```

FFmpeg demux 用于检查输入流探测和解封装：

```bash
$test_dir/test_ffmpeg_demux sample.mp4 --frames 300 --dump-pipe
$test_dir/test_ffmpeg_demux 'rtsp://user:password@host/stream' \
  --format-options rtsp_transport=tcp --duration 10
```

摄像头测试的第一个参数是 V4L2 设备节点。可以先查询设备能力：

```bash
$test_dir/test_camera /dev/video0 --query-capabilities
$test_dir/test_camera /dev/video0 \
  --format nv12 --width 1920 --height 1080 \
  --frame-rate 30 --frames 300
```

RTSP 测试的第一个参数是 URL，传输方式可选 `udp`、`tcp` 或 `multicast`：

```bash
$test_dir/test_rtsp_client 'rtsp://user:password@host/stream' \
  --transport tcp --duration 10
```

### 4.2 编解码和图像处理（VP）

MPP 解码从压缩视频文件读取数据；使用 `--output` 可保存解码后的裸数据：

```bash
$test_dir/test_mpp_decoder input.h265 \
  --frames 300 --output decoded.yuv
```

MPP 编码使用内部生成的图像 buffer，不需要输入图像文件；第一个参数是输出文件：

```bash
$test_dir/test_mpp_encoder encoded.h265 \
  --codec h265 --width 1920 --height 1080 \
  --format nv12 --frames 300
```

图像处理测试默认生成输入内容，可设置格式、尺寸、裁剪、旋转和测量帧数：

```bash
$test_dir/test_module_imageProcessor \
  --input-format RGB24 --input-size 1920x1080 \
  --output-format NV12 --output-size 1280x720 \
  --rotation 90 --iterations 300
```

### 4.3 显示（VO）

DRM 显示测试执行“文件读取 + MPP 解码 + DRM/KMS 显示”链路。先使用默认单窗口场景：

```bash
$test_dir/test_module_drmDisplay \
  --scenario single-window --duration 10 sample.mp4
```

确认基本链路后，可测试多窗口或其他场景：

```bash
$test_dir/test_module_drmDisplay \
  --scenario multi-window --frames 300 sample.mp4
```

仅校验并打印显示配置，不打开显示设备：

```bash
$test_dir/test_module_drmDisplay --dry-run sample.mp4
```

## 5. 常用运行选项

不同程序支持的选项略有差异，请以各程序的 `--help` 为准。常见选项包括：

- `--frames N`：处理 N 个帧或 buffer 后退出；多数程序中 `0` 表示不限制。
- `--duration SEC`：运行指定秒数后退出；`0` 表示不限制。
- `--verbose`：输出更详细的状态和处理信息。
- `--report-every N`：每处理 N 个 buffer 输出一次统计。
- `--dump-pipe`：输出模块管线及停止后的汇总。
- `--external-consumer`：增加外部消费者，检查 buffer 分发。
- `--buffer-type noncache|cache|malloc|dma32|cache-dma32`：选择视频 buffer 类型（MPP 测试）。

长时间测试应设置 `--frames` 或 `--duration`；需要提前停止时可使用 `Ctrl-C`。

## 6. Python 测试

`vi/` 和 `vp/` 下提供以下 Python 绑定测试：

- `vi/test_ffmpeg_demux.py`
- `vp/test_mpp_decoder.py`
- `vp/test_mpp_encoder.py`

先安装发布包提供的 `ff_pymedia` wheel，或将扩展目录加入 `PYTHONPATH`，再从发布包根目录运行：

```bash
LD_LIBRARY_PATH=./lib \
  python3 examples/tests/vi/test_ffmpeg_demux.py input.mp4 --frames 300

LD_LIBRARY_PATH=./lib \
  python3 examples/tests/vp/test_mpp_decoder.py input.h265 \
  --frames 300 --output decoded.yuv

LD_LIBRARY_PATH=./lib \
  python3 examples/tests/vp/test_mpp_encoder.py encoded.h265 --frames 300
```

Python 测试与对应的 C++ 测试使用相同的输入和硬件前提。

## 7. 运行环境限制

CPU CTest 可在普通 Linux 构建环境中运行。DRM、GPU、RGA、MPP、摄像头、显示和实时媒体
测试必须在具备相应设备的目标机上运行。
