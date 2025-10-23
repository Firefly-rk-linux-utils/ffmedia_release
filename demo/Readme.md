# demo 用法
该目录下的demo是展现vi、vp及vo 模块的使用示例

## CPP demo

### demo.cpp
该demo展现了大部分模块的基本使用示例。

#### file

读取或写入文件。

- 读取

```
# 解封装解码播放; opengl渲染(-x), drm渲染(-d 0), 同步(-s)
./demo input.mp4 -x -s
./demo input.mp4 -d 0 -s
# 使用ffmpeg解封装
./demo input.mp4 --use_ffmpeg_demux -x -s
```

- 写入

```
# mp4转封装mkv; 关闭解码（--dec_disabled）
./demo input.mp4 --dec_disabled -m output.mpeg
# 使用ffmpeg转封装
./demo input.mp4 --use_ffmpeg_demux --dec_disabled --use_ffmpeg_mux -m output.mpeg
# 管道输出
./demo input.mp4 --dec_disabled --use_ffmpeg_mux=h264 -m pipe:1
```

#### camera

```
# 尝试以分辨率为1920x1080，格式为nv12图像参数实时预览摄像头；指定输入图像分辨率(-i 1920x1080), 指定输入图像格式(-a nv12)
./demo /dev/videoX -i 1920x1080 -a nv12 -d 0
```

#### rtsp

- 拉流
```
# 拉流解码播放; drm渲染(-d 0), 同步(-s), 传输协议（--rtsp_transport udp）
./demo rtsp://hostname[:port]/path -d 0 -s
./demo rtsp://hostname[:port]/path --use_ffmpeg_demux=rtsp --rtsp_transport udp -d 0 -s

# 拉取16路rtsp流解码播放
./demo rtsp://hostname[:port]/path -d 0 -s -c 16
```

- 推流

```
# 媒体视频文件实时发送流到RTSP服务器，供其他人观看
./demo input.mp4 --use_ffmpeg_mux=rtsp --dec_disabled -m rtsp://hostname[:port]/path -s
```

- RTSP服务器

```
# 自身做RTSP服务器，供其他人来拉流观看：rtsp://localhost:8554/live/test
./demo input.mp4 --dec_disabled --port 8554 --push_path "/live/test" -s
# 拉流转码转播
./demo rtsp://hostname[:port]/path -e h265 --port 8554 --push_path "/live/test" -s
```


#### rtmp

- 拉流

```
# 拉流解码播放; drm渲染(-d 0), 同步(-s)
./demo rtmp://server[:port][/app][/playpath] -d 0 -s
./demo rtmp://server[:port][/app][/playpath] --use_ffmpeg_demux=rtmp -d 0 -s
```


- 推流

```
# 媒体视频文件实时发送流到RTMP服务器，供其他人观看
./demo input.mp4 --dec_disabled --rtmp_url rtmp://server[:port][/app][/playpath] -s
./demo input.mp4 --dec_disabled --use_ffmpeg_mux=rtmp -m rtmp://server[:port][/app][/playpath] -s
# 转码h264再发送流到RTMP服务器
./demo input.mp4 -e h264 --use_ffmpeg_mux=rtmp -m rtmp://server[:port][/app][/playpath] -s
```


- RTMP服务器

```
# 自身做RTMP服务器，供其他人来拉流观看：rtsp://localhost:1935/live/test
./demo input.mp4 --dec_disabled --push_type rtmp --port 1935 --push_path "/live/test" -s
```


#### webrtc

- whep

```
# 拉流播放，如需设置token可通过FFmpegDemux的setFormatOption接口设置：setFormatOption("bearer_token"，"xxxx", 0);
./demo "http://server:port/path" --use_ffmpeg_demux=whep -d 0 -s
```

- whip

```
# 推流到webrtc服务器，如需设置token可通过FFmpegMux的setFormatOption接口设置。
./demo input.mp4 --dec_disabled --use_ffmpeg_mux=whip -m 'http://server:port/path'
# 转码h264再发送流到webrtc服务器
./demo input.mp4 -e h264 --use_ffmpeg_mux=whip -m 'http://server:port/path'
```

#### 视频处理

- 解码

解码默认开启，如果不需要解码可通过--dec_disabled关闭解码

```
# 读取视频文件并解码输出到 output.nv12文件上
./demo input.mp4 -m output.nv12
# 拉取rtsp流解码并通过管道输出到标准输出(stdout)
./demo rtsp://hostname[:port]/path --use_ffmpeg_mux=rawvideo -m pipe:1
```

- 图像处理

```
# 读取媒体文件就，解码并将图像分辨率调整至1280x720，图像格式转换成bgr24，并且旋转180度显示
./demo input.mp4 -o 1280x720 -b bgr24 -r 180 -x -s

# 拉取rtsp流解码并调整图像格式为YUV420，通过管道输出到标准输出(stdout)
./demo rtsp://hostname[:port]/path -b YUV420 --use_ffmpeg_mux=rawvideo -m pipe:1
```

- 编码

```
# 读取分辨率为1920x1080,格式为nv12裸流进行h264编码并输出文件
./demo input.yuv -i 1920x1080 -a nv12 -e h264 -m output.h264
# rtmp拉流、转码成h265、并推流到RTSP服务器
./demo rtmp://server[:port][/app][/playpath] -e h265 --use_ffmpeg_mux=rtsp -m "rtsp://hostname[:port]/path"
```



#### 其他示范

```
## 示范：输入是分辨率为 1080p 的tcp流 rtsp 摄像头，把解码图像缩放为 720p 并且旋转 90 度，使用drm显示, 使用同步播放。
./demo rtsp://admin:firefly123@168.168.2.143 --rtsp_transport tcp -o 1280x720 -d 0 -r 90 -s 

## 使用rtmp拉流，把解码图像缩放为 720p 并且旋转 180 度，使用x11窗口显示。
./demo rtmp://192.168.1.220:1935/live/0 -o 1280x720 -x -r 180

## 输入是本地视频文件，把解码图像缩放为 720p， 使用x11窗口显示，使用plughw:3,0音频设备进行播放，并使用同步播放。
./demo /home/firefly/test.mp4 -o 1280x720 -x 0 --aplay plughw:3,0 -s

## 输入是本地视频文件，把解码图像缩放为 720p， 使用drm显示，并编码成h264向1935端口进行rtsp推流。
./demo /home/firefly/test.mkv -o 1280x720 -d 0 -e h264 -p 1935

## 输入是摄像头设备，编码成h265，同时采集plughw:2,0音频设备音频，编码成aac，并封装成mp4文件保存。
./demo /dev/video0 -e h265 --arecord plughw:2,0 -m out.mp4

## 循环读取本地视频文件, 转码成h264，然后推到gb28181服务器上。
./demo /home/firefly/test.mp4 -e h264 -s -l --gb28181_user_id 000001 --gb28181_server_id 000002 --gb28181_server_ip 172.16.10.204 --gb28181_server_port 5060

## 播放本地视频
./demo /home/firefly/test.mp4 -d 0
./demo /home/firefly/test.mp4 --use_ffmpeg_demux -d 0

## 录屏并显示
./demo /dev/dri/card0 --use_ffmpeg_demux=kmsgrab -x -s

## 读取本地文件，转码成h264,然后使用ffmpeg进行rtsp封装推流
./demo test.mp4 -e h264 --use_ffmpeg_mux=rtsp -m rtsp://192.168.0.11:8554/live -s

```

### demo_simple.cpp demo_opencv.cpp demo_opencv_multi.cpp
- demo_simple.cpp示例展现了使用rtsp模块拉流解码，进行drm显示
- demo_opencv.cpp示例展现了在模块回调函数使用opencv显示
- demo_opencv_multi.cpp 示例展现了通过申请外部模块，达到多个实例使用rga模块输出数据

**需要自行更改示例的rtsp模块的输入地址**

```
./demo_simple
./demo_opencv
./demo_opencv_multi
```

### demo_rgablend.cpp

该示例展现了在rga模块的回调上使用opencv将时间戳生成图片，并将该图片使用rga合成接口与源图像混合输出给drm模块显示。

**需要自行更改示例的rtsp模块的输入地址**

```
./demo_rgablend
```

### demo_memory_read.cpp
该示例展现了使用内存读取模块读取h264文件进行解码播放。

```
## 读取本地h264文件并指定了视频的宽度及高度
./demo_memory_read test.h264 1920 1080
```

### demo_multi_drmplane.cpp demo_multi_window.cpp
这两个示例展现了drm显示模块的特别用法。
**需要自行更改示例的rtsp模块的输入地址。**

```
## 使用四个drm模块并且移动显示其中一个模块
./demo_multi_drmplane
```

### demo_multi_splice.cpp
多路拼接显示和推流示例。拉多路rtsp流解码拼接在一个画面上显示同时将该画面编码推流。
需自行在代码里的rtspUrl变量设置rtsp地址

```
./demo_multi_splice
```

### 推理示例
该源码在inference_examples/yolov5/src/
#### 安装依赖

```
apt install libeigen3-dev
```
#### 编译

```
cd build 								                        #进入编译目录
cmake ../ -DDEMO_INFERENCE=ON                                   #打开编译inference demo
make -j8 										                #编译
cp -r ../inference_examples/yolov5/model inference_examples/    #将yolov5的model目录拷贝到运行目录

```

#### 运行测试

```
# 进入推理示例的编译目录
cd inference_examples/
# 物体识别示例，可输入媒体文件、网络流等
./demo_yolov5 input.mp4 ./model/RK3588/yolov5s-640-640.rknn
# taskset -c 4 ./demo_yolov5 input.mp4 ./model/RK3588/yolov5s-640-640.rknn

# 目标跟踪示例，可输入媒体文件、网络流等
./demo_yolov5_track rtsp://xxx ./model/RK3588/yolov5s-640-640.rknn

# 推理池使用示例，多线程并发推理
./demo_multi_detector input.mp4 ./model/RK3588/yolov5s-640-640.rknn model/coco_80_labels_list.txt
```


## python demo
c++所展示使用模块接口和python的一一对应。

**py示例使用之前需要安装python版本的ffmedia库运行,使用pip安装dist/目录下的库即可**

如需要更新python版本的ffmedia库需要先卸载旧库再安装新的。
### demo.py
demo.py和demo.cpp的参数类似。
简单使用说明:

```
## 示范：输入是分辨率为 1080p 的tcp流 rtsp 摄像头，把解码图像缩放为 720p 并且旋转 90 度，使用drm显示, 使用同步播放
./demo.py -i rtsp://admin:firefly123@168.168.2.143 --rtsp_transport 1 -o 1280x720 -d 0 -r 1 -s 1

## 使用rmtp拉流，把解码图像缩放为 720p 并且旋转 180 度，使用x11窗口显示。
./demo.py -i rtmp://192.168.1.220:1935/live/0 -o 1280x720 -x 1 -r 2

## 输入是本地视频文件，把解码图像缩放为720p，使用x11窗口显示，使用plughw:3,0音频设备进行播放，使用同步播放；
./demo.py -i /home/firefly/test.mp4 -o 1280x720 -x 1 --aplay plughw:3,0 -s 1

## 输入是本地mp4视频文件，把解码图像缩放为 720p，使用drm显示，并编码成h264向1935端口进行rtmp推流。
./demo.py -i /home/firefly/test.mp4 -o 1280x720 -d 0 -e 0 -p 1935 --push_type 1

## 输入是本地mkv视频文件，把解码图像缩放为 720p，转码成BGR24格式使用opengcv显示, 并使用同步播放。
./demo.py -i /home/firefly/test.mkv -o 1280x720 -b BGR24 -c 1 -s 1

## 输入是摄像头设备，编码成h265并封装成mkv文件保存。
./demo.py -i /dev/video0 -e 1 -m out.mkv

## 播放本地视频
./demo.py /home/firefly/test.mp4 -d 0
./demo.py /home/firefly/test.mp4 --use_ffmpeg_demux mp4 -d 0

## 录屏并显示
./demo.py -i /dev/dri/card0 --use_ffmpeg_demux kmsgrab -x 1

## 读取本地文件，转码成h264,然后使用ffmpeg进行rtsp封装推流
./demo.py test.mp4 -e 0 --use_ffmpeg_mux rtsp -m rtsp://192.168.0.11:8554/live -s 0

```

### 推理示例

yolov5推理示例位于inference_examples/yolov5/python目录下

对该目录下的sample_720p.mp4视频解封装、解码、推理、将推理结果绘制到画面并显示和编码封装成result.mp4

```
cd inference_examples/yolov5/python
python3 main.py
```

