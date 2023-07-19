# ffmedia介绍

ffmedia是一套基于Rockchip Mpp/RGA开发的视频编解码框架。支持音频aac编解码。
ffmedia一共包含以下单元

- 输入源单元 VI：
  - Camera:  支持UVC， Mipi CSI
  - RTSP Client: 支持tcp、udp和多播协议
  - RTMP Client: 支持播放和发布流
  - File Reader：支持mkv、mp4、flv文件读入及裸流等文件读入
- 处理单元 VP:
  - MppDec: 视频解码，支持H264,H265,MJpeg
  - MppEnc: 视频编码，支持H264,H265
  - RGA：图像缩放，裁剪，格式转换
  - AacDec: aac音频解码和播放
  - AacEnc: aac音频编码
- 输出单元 VO
  - DRM Display: 基于libdrm的显示模块
  - RTSP Server:
  - RTMP Server: 支持推流
  - File Writer: 支持mkv、mp4、flv及裸流等文件保存
- pybind11 pymodule.cpp
  - pymodule: 创建vi、vo、vp等的c++代码的Python绑定，以提供python调用vi、vo、vp等c++模块的python接口

各个模块成员函数及参数说明请参看documentation/ffmedia.docx 。

## 软件框架：

整个框架采用Productor/Consumer模型，将各个单元都抽象为ModuleMedia类。
一个Productor可以有多个Consumer，一个Consumer只有一个Productor. 输入源单元没有Productor.

## 安装编译环境
```
apt update
apt install -y gcc g++ make cmake
apt install -y libasound2-dev libopencv-dev libdrm-dev libfdk-aac-dev
```
如果需要编译python接口库，并系统默认python是3.8，则安装3.8的软件包
```
apt install -y python3.8-dev python3.8-venv
```
如果不需要
```
sed -i 's/.*ff_pymedia*/\#&/' CMakeLists.txt
sed -i 's/add_subdirectory(pybind11)/\#&/' CMakeLists.txt
```

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
