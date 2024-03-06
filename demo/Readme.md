# demo 用法
该目录下的demo是展现vi、vp及vo 模块的使用示例

## 安装依赖库
1. 安装编译环境

```
apt update
apt install -y gcc g++ make cmake
apt install libdrm-dev libjpeg9-dev
```
2. 安装音频相关模块依赖库

```
apt install libasound2-dev libfdk-aac-dev
```
3. 安装opengl相关模块依赖库。在ubuntu18上没有libgles-dev软件包，可更改成libgles2-mesa-dev软件包

```
apt install libgles-dev libx11-dev
```
4. 如需要支持opencv相关demo，安装下列软件包

```
apt install libopencv-dev
```

## c++ demo

### 编译demo

1. 首先在源码根路径创建编译文件夹并进入

```
$ ls
build  CMakeLists.txt  demo  dist  documentation  include  lib  Readme.md  rknn

$ mkdir build
$ cd build
```

2. 使用cmake 选择要编译的demo, 默认不编译opencv、rknn的demo

```
# 如果需要编译opencv、rknn的demo，则cmake ../ -DDEMO_OPENCV=ON -DDEMO_RKNN=ON
$ cmake ../

# 编译
$ make -j8


# 把rknn的库路径添加到当前环境；如果是rk356x板子，则把RK3588更改为RK356X。
# 也可以忽略这步使用系统默认的rknn库或自行指定rknn库。
$ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../rknn/lib/RK3588/

```

**ffmedia默认使用了rknn,如果是rk3399等不支持rknn机型，也是需要指定rknn库的，使其编译时可以找到函数定义**
，但不能使用推理模块

```
#可以指定任意一个rknn库位置
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../rknn/lib/RK3588/

#或者直接将任意一个rknn库拷贝进系统中
cp ../rknn/lib/RK3588/librknnrt.so /usr/lib/

```


### demo.cpp
该demo展现了大部分模块的基本使用示例。
简单使用示例说明:

```
## 示范：输入是分辨率为 1080p 的tcp流 rtsp 摄像头，把解码图像缩放为 720p 并且旋转 90 度，使用drm显示, 使用同步播放。
./demo rtsp://admin:firefly123@168.168.2.143 --rtsp_transport tcp -o 1280x720 -d 0 -r 90 -s 

## 使用rtmp拉流，把解码图像缩放为 720p 并且旋转 180 度，使用x11窗口显示。
./demo rtmp://192.168.1.220:1935/live/0 -o 1280x720 -x -r 180

## 输入是本地视频文件，把解码图像缩放为 720p， 使用x11窗口显示，使用plughw:3,0音频设备进行播放，并使用同步播放。
./demo /home/firefly/test.mp4 -o 1280x720 -x 0 --aplay plughw:3,0 -s

## 输入是本地视频文件，把解码图像缩放为 720p， 使用drm显示，并编码成h264向1935端口进行rtsp推流。
./demo /home/firefly/test.mkv -o 1280x720 -d 0 -e h264 -p 1935

## 输入是摄像头设备，编码成h265并封装成mp4文件保存。根据文件名后缀封装成mp4、mkv、flv媒体文件或h264、yuv、rgb等裸流文件。
./demo /dev/video0 -e h265 -m out.mp4
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

### demo_rknn.cpp
该源码在../rknn/src/demo_rknn.cpp 。
该示例展现了使用推理模块进行推理，计算推理结果使用opencv将目标框住并显示。

```
cd build 													#进入编译目录
cmake ../ -DDEMO_OPENCV=ON -DDEMO_RKNN=ON 					#打开编译opencv及rknn demo
make -j8 													#编译
cp -r ../rknn/model ./ 										#将rknn下的model目录拷贝到当前目录
taskset -c 3 ./demo_rknn rtsp://xxx ./model/RK3588/yolov5s-640-640.rknn #指定rtsp地址及模型文件路径运行

```


## python demo
c++所展示使用模块接口和python的一一对应。

**py示例使用之前需要安装python版本的ffmedia库运行,使用pip安装dist/目录下的库即可**

如需要更新python版本的ffmedia库需要先卸载旧库再安装新的。
### demo.py
该demo展现了大部分模块的基本使用示例。
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
```
