# ffmedia介绍

ffmedia是一套基于Rockchip Mpp/RGA开发的视频编解码框架。支持音频aac编解码。
ffmedia一共包含以下单元

- 输入源单元 VI：
  - Camera:  支持UVC， Mipi CSI
  - RTSP Client: 支持tcp、udp和多播协议
  - RTMP Client: 支持拉流和推流
  - File Reader：支持mkv、mp4、flv、ts、ps文件及裸流等文件读入
  - Memory Reader:支持内存数据读入
  - Alsa Capture: 音频采集
  - FFmpeg Demux: 支持文件、网络流及UVC等读取
- 处理单元 VP:
  - MppDec: 视频解码，支持H264,H265,MJpeg,VP8,VP9,MPEG1,MPEG2,MPEG4
  - MppEnc: 视频编码，支持H264,H265,MJpeg
  - RGA：图像合成，缩放，裁剪，格式转换
  - AacDec: aac音频解码
  - AacEnc: aac音频编码
  - Inference: rknn模型推理
- 输出单元 VO：
  - DRM Display: 基于libdrm的显示模块
  - Renderer Video: 使用gles渲染视频，基于libx11窗口显示
  - RTSP Server: 支持tcp和udp推流
  - RTMP Server: 支持推流
  - File Writer: 支持mkv、mp4、flv、ts、ps文件封装及裸流等文件保存
  - Alsa PlayBack: 音频播放
  - GB28181 Client: 支持点播
- pybind11:
  - pymodule: 创建vi、vo、vp等的c++代码的Python绑定，以提供python调用vi、vo、vp等c++模块的python接口

各个模块成员函数及参数说明请参看documentation/ffmedia.docx 。

## 软件框架：

整个框架采用Productor/Consumer模型，将各个单元都抽象为ModuleMedia类。
一个Productor可以有多个Consumer，一个Consumer只有一个Productor. 输入源单元没有Productor.

## Cpp示例的编译和运行

### 安装相关环境

```
apt update
apt install -y gcc g++ make cmake libdrm-dev libjpeg9-dev libasound2-dev libfdk-aac-dev libgles-dev libx11-dev

# 如需要支持opencv相关demo，安装下列软件包

apt install libopencv-dev

```

### 编译与运行

1. 首先在源码根路径创建编译文件夹并进入

```
$ ls
CMakeLists.txt  demo  dist  documentation  include  lib  Readme.md  inference_examples

$ mkdir build
$ cd build
```

2. 使用cmake 选择要编译的demo, 默认不编译opencv、inference的demo

```
# 如果需要编译opencv、inference的demo，则cmake ../ -DDEMO_OPENCV=ON -DDEMO_INFERENCE=ON
$ cmake ../

# 编译
$ make -j8

# 把rknn的库路径添加到当前环境；如果是rk356x板子，则把RK3588更改为RK356X。
# 也可以忽略这步使用系统默认的rknn库或自行指定rknn库。
$ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${PWD}/../inference_examples/lib/RK3588/

```

3. 运行在build编译出demo

```
cd build
./demo

```

执行可能会报错 “error while loading shared libraries: librknnrt.so: cannot open shared object file: No such file or directory”

由于ffmedia使用了rknn，需要将rknn库路径添加到当前环境

```
#根据对应的芯片选择对应的rknn库目录，如果找不到芯片对应的库路径则默认选择RK3588
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${PWD}/../inference_examples/lib/RK3588/

#或者直接将库安装到进系统中
cp ../inference_examples/lib/RK3588/librknnrt.so /usr/lib/

```

4. 更多示例使用介绍说明：[demo/Readme.md](demo/Readme.md)

## ffmedia api 文档
ffmedia的api详细文档：[ffmedia_api.pdf](documentation/ffmedia_api.pdf)

## 多路编解码问题

在多路编解码时，如果出现无法申请buf或者无法初始化等，可能是系统限制了进程使用fd的数量
更改进程使用的fd数量，临时更改：

```
ulimit -n #查看当前进程可用fd最大数量
ulimit -n 102400 #更改进程可用fd最大数量到102400
```
永久更改：

```
sudo vim /etc/security/limits.conf
#在尾部添加
*	soft	nofile	102400
*	hard	nofile	102400
*	soft	nproc	102400
*	hard	nproc	102400

```

## 依赖库路径问题
程序运行环境区别可能导致寻找不到依赖的动态库，可通过LD_LIBRARY_PATH将库路径添加进当前环境。
```
export LD_LIBRARY_PATH=/path/to/your/libs:$LD_LIBRARY_PATH
```

或者通过patchelf直接修改程序或者动态库的库路径。
```
patchelf --set-rpath /path/to/your/libs <your-binary>
```
