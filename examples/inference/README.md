# FFMedia 推理扩展

本仓库是FFMedia 推理扩展，包含推理、目标跟踪和 OSD 等相关实现，并包含媒体文件推理与摄像头人脸跟踪示例。

## 目录内容

| 目录 | 内容 |
| --- | --- |
| `infer/` | 推理相关实现，主要适配了RKNN2的推理实现 |
| `buffer/` | `InferBuffer`、检测目标、人脸目标和姿态目标数据结构 |
| `track/` | ByteTrack 跟踪实现 |
| `osd/` | 检测框、人脸关键点和姿态骨架绘制模块 |
| `demo/examples/` | YOLOv5、YOLOv8、YOLOv8 Pose、RetinaFace 文件推理示例 |
| `demo/face_track_demo/` | 摄像头人脸跟踪、云台控制和 RTSP 输出示例 |

核心数据流如下：

```text
原始视频帧 → ModuleInferRKNN2* → InferBuffer → ModuleByteTrack（可选）→ ModuleOsd* / 业务回调
```

检测结果位于 `InferBuffer::targets`，人脸结果位于 `face_targets`，姿态结果位于 `pose_targets`。

## 依赖

- CMake 3.10+ 和支持 C++17 的编译器；
- FFMedia CMake package（提供 `FFMedia::ff_media`）；
- Eigen3 3.3；
- OpenCV 4；
- RKNN Runtime：`rknn_api.h` 和 `librknnrt.so`；
- 目标板对应的 RGA、MPP 运行库。

RKNN Runtime、模型和 NPU 驱动必须匹配目标 SoC 及版本。

## 编译

在本目录中执行：

```bash
cmake -S . -B build
cmake --build build -j
```

如果本目录不位于 FFMedia 安装目录的 `examples/inference/` 中，可指定 FFMedia CMake 配置目录：

```bash
cmake -S . -B build \
    -DFFMedia_DIR=/path/to/lib/cmake/FFMedia
cmake --build build -j
```

构建产物：

```text
build/
├── libff_media_infer.so
└── demo/
    ├── examples/
    │   ├── yolov5_demo
    │   ├── yolov8_demo
    │   ├── yolov8_pose_demo
    │   └── retina_face_demo
    └── face_track_demo/face_track_demo
```

运行前确保扩展库、FFMedia 和 RKNN Runtime 可被动态链接器找到：

```bash
export LD_LIBRARY_PATH="$PWD/build:/path/to/ffmedia/lib:/path/to/rknn/lib:${LD_LIBRARY_PATH:-}"
```

## 示例

文件推理示例的参数如下：

```bash
./build/demo/examples/yolov5_demo <video> <model.rknn> <labels.txt>
./build/demo/examples/yolov8_demo <video> <model.rknn> <labels.txt>
./build/demo/examples/yolov8_pose_demo <video> <model.rknn> <labels.txt>
./build/demo/examples/retina_face_demo <video> <model.rknn>
```

以上示例使用 OpenCV HighGUI 显示结果，需要图形显示环境。无头设备可参考示例中的 `setMediaBufferProduceHooker()`，将 `InferBuffer` 接入业务处理、编码或推流模块。

摄像头人脸跟踪示例：

```bash
./build/demo/face_track_demo/face_track_demo \
    /dev/video11 \
    ./demo/face_track_demo/model/RetinaFace_mobile320_RV1126B.rknn
```

该示例默认使用 `/dev/video11`，启动 RTSP 服务 `rtsp://<设备地址>:8554/live/0`，并尝试控制 PWM 云台。使用前请根据板卡调整 `demo/face_track_demo/src/cam_face_track_demo.cpp` 中的摄像头节点、PWM 通道、角度和脉宽参数。

## 模型约束

- YOLOv5、YOLOv8 和 YOLOv8 Pose 的标签文件每行一个类别，顺序必须与模型类别 ID 一致；
- RetinaFace 当前支持输入高度为 320 或 640 的模型；
- YOLOv8 Pose 当前按 17 个关键点解析；
- 推理输入为 3 通道图像，内部以 RGB24 进行等比例缩放和 letterbox，并将结果坐标映射回原图。
