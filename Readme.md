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
  - Video Stack: 支持多路视频拼接输出
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
  - FFmpeg Mux: 支持文件、网络流等封装输出
- pybind11:
  - pymodule: 创建vi、vo、vp等的c++代码的Python绑定，以提供python调用vi、vo、vp等c++模块的python接口

各个模块成员函数及参数说明请参看 [docs/ffmedia_api.md](docs/ffmedia_api.md)。

## 软件框架：

整个框架采用Productor/Consumer模型，将各个单元都抽象为ModuleMedia类。
一个Productor可以有多个Consumer，一个Consumer也可以有多个Productor. 输入源单元没有Productor.
框架详细介绍文档：[docs/module_media.md](docs/module_media.md)
![](./docs/img/p1.png)

## 示例的编译和运行

示例位于demo下，编译及使用介绍说明：[demo/Readme.md](demo/Readme.md)

## ffmedia api 文档
ffmedia的api详细文档：[docs/ffmedia_api.md](docs/ffmedia_api.md)

## 低延迟显示测试

以下场景均使用ffmedia实现的低负载、高实时性的低延迟显示测试。

- 场景1：HDMI输入转发显示：HDMI画面经过RK3588-A采集、编码、推流后画面延迟为17.5ms（画面帧率越高延迟越低）。画面经过RK3588-B取流、解码、送显后画面延迟为1.5ms。画面只要在vsync来临前1ms设置显示，就可以保证在屏幕的vsync来临后将画面内容显示出来，则估算屏幕显示画面延迟为1ms~17.6ms，取平均10ms。画面从HDMI输入到转发显示的延迟平均为29ms。
- 场景2：Camera采集转发显示：RK3588-A控制LED，再通过Camera采集该画面，经过编码、推流后画面延迟为7.5ms。画面经过RK3588-B取流、解码、送显后画面延迟为1.5ms。屏幕显示画面延迟为1ms~17.6ms，取平均10ms。画面从Camera采集到转发显示的延迟平均为19ms。

![](./docs/img/low_delay_demo.png)

### 显示延迟优化

通过上面测试结果可观察到系统整体延迟受显示延迟影响比较大，并存在较大波动。可通过下面方法降低画面显示延迟并将波动范围控制在可接受范围。

1. 由于采集数据的频率固定，每次开启时数据送显时间点与屏幕的vsync时间点差值随机，可能就会遇到较大的差值导致延迟偏高，这时就可以通过reset VideoPort 控制vsync时间点与数据送显时间点接近。
2. 采集数据的帧率与刷新屏幕的帧率不可能完全一致，对于要求低延迟显示来说，可能就会有延迟时间的波动，为了解决这个问题，就需要动态调整屏幕的刷新率，调整hdmitx的 phy clk，dp、edp的v0pll，dsi的aupll 时钟频率，尽可能与数据采集频率趋于一致，让延迟波动控制在可接受范围。

通过以上两步可以让显示延迟从1ms~17.6ms的波动范围降低至1ms~7ms。

## 常见问题

### 依赖库路径问题
程序运行环境区别可能导致寻找不到依赖的动态库，可通过LD_LIBRARY_PATH将库路径添加进当前环境。
```
export LD_LIBRARY_PATH=/path/to/your/libs:$LD_LIBRARY_PATH
```

或者通过patchelf直接修改程序或者动态库的库路径。
```
patchelf --set-rpath /path/to/your/libs <your-binary>
```
### 多路编解码失败问题

在多路编解码时，如果出现无法申请buf或者无法初始化等，可能是进程使用文件描述符数量限制，一般为1024。
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

## 其他

如果遇到问题或者有其他功能需求的，可以提issue，我们将在下个版本修复或添加支持。
