#!/usr/bin/env python3
import ff_pymedia as m
import argparse
import re
import os
import stat
import time
cv2_enable = True
try:
    import cv2
except ImportError:
    cv2_enable = False

class Cv2Display():
    def __init__(self, name, module, sync, count):
        self.name = name
        self.module = module
        self.sync = sync
        self.count = count

def align(x, a):
    return (x + a - 1) & ~(a - 1)

def find_two_numbers(n, x, y):
    a = 1
    b = n
    min_diff = 8192
    result = (0, 0)
    while a <= b:
        if n % a == 0:
            b = n // a
            diff1 = abs(a - x) + abs(b - y)
            diff2 = abs(a - y) + abs(b - x)
            if diff1 < min_diff or diff2 < min_diff:
                if diff1 < diff2:
                    result = (a, b)
                else:
                    result = (b, a)
                min_diff = min(diff1, diff2)
        a += 1
    return result

def cv2_extcall_back(obj, MediaBuffer):
    vb = m.VideoBuffer.from_base(MediaBuffer)
    if obj.sync is not None:
        delay = obj.sync.updateVideo(vb.getPUstimestamp(), 0)
        if delay > 0:
            time.sleep(delay/1000000)
    data = vb.getActiveData()
    #flush dma buf to cpu
    vb.invalidateDrmBuf();

    try:
        img = data.reshape((vb.getImagePara().vstride, vb.getImagePara().hstride, 3))
    except ValueError:
        print("Invalid image resolution!")
        resolution = find_two_numbers(data.size//3, vb.getImagePara().hstride, vb.getImagePara().vstride)
        print("Try the recommended resolution: -o {}x{}".format(resolution[0], resolution[1]))
        exit(-1)
    for i in range(obj.count):
        cv2.imshow(obj.name + str(i), img)
    cv2.waitKey(1)

def call_back(obj, MediaBuffer):
    a = MediaBuffer.getActiveData()
    obj.write(a)

def get_parameters():
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--input_source", dest='input_source', type=str, help="input source")
    parser.add_argument("-f", "--save_file", dest='save_file', type=str, help="Enable save source output data to file, set filename, default disabled")
    parser.add_argument("-o", "--output", dest='output', type=str, help="Output image size, default same as input")
    parser.add_argument("-b", "--outputfmt", dest='outputfmt', type=str, default="NV12", help="Output image format, default NV12")
    parser.add_argument("-e", "--encodetype", dest='encodetype', type=int, default=-1, help="Encode encode, set encode type, default disabled")
    parser.add_argument("-m", "--enmux", dest='enmux', type=str, help="Enable save encode data to file. Enable package as mp4, mkv, flv, ts, ps or raw stream files, muxer type depends on the filename suffix. default disabled")
    parser.add_argument("-p", "--port", dest='port', type=int, default=0, help="Enable push stream, default rtsp stream, set push port, depend on encode enabled, default disabled\n")
    parser.add_argument("--push_type", dest='push_type', type=int, default=0, help="Set push stream type, default rtsp. e.g. --push_type 1\n")
    parser.add_argument("--rtmp_url", dest='rtmp_url', type=str, help="Set the rtmp client push address. e.g. --rtmp_url rtmp://xxx\n")
    parser.add_argument("--rtsp_transport", dest='rtsp_transport', type=int, default=0, help="Set the rtsp transport type, default 0(udp)")
    parser.add_argument("-s", "--sync", dest="sync", type=int, default=-1, help="Enable synchronization module, default disabled. e.g. -s 1")
    parser.add_argument("-a", "--aplay", dest='aplay', type=str, help="Enable play audio, default disabled. e.g. -a plughw:3,0")
    parser.add_argument("-r", "--rotate", dest='rotate',type=int, default=0, help="Image rotation degree, default 0" )
    parser.add_argument("-d", "--drmdisplay", dest='drmdisplay', type=int, default=-1, help="Drm display, set display plane, set 0 to auto find plane")
    parser.add_argument("--connector", dest='connector', type=int, default=0, help="Set drm display connector, default 0 to auto find connector")
    parser.add_argument("-z","--zpos", dest='zpos', type=int, default=0xff, help="Drm display plane zpos, default auto select")
    parser.add_argument("-c", "--cvdisplay", dest='cvdisplay', type=int, default=0, help="OpenCv display, set window number, default 0")
    parser.add_argument("-x", "--x11display", dest='x11display', type=int, default=0, help="X11 window displays, render the video using gles. default disabled")
    parser.add_argument("-l", "--loop", dest='loop', action='store_true', help="Loop reads the media file.")

    return parser.parse_args()

def main():

    audio_enable = False
    args = get_parameters()
    if args.aplay is not None:
        audio_enable = True

    if args.input_source is None:
        return 1
    elif args.input_source.startswith("rtsp://"):
        print("input source is a rtsp url")
        input_source = m.ModuleRtspClient(args.input_source, m.RTSP_STREAM_TYPE(args.rtsp_transport), True, audio_enable)
    elif args.input_source.startswith("rtmp://"):
        print("input source is a rtmp url")
        input_source = m.ModuleRtmpClient(args.input_source)
    else:
        is_stat = os.stat(args.input_source)
        if stat.S_ISCHR(is_stat.st_mode):
            print("input source is a camera device.")
            input_source = m.ModuleCam(args.input_source)
        elif stat.S_ISREG(is_stat.st_mode):
            print("input source is a regular file.")
            input_source = m.ModuleFileReader(args.input_source, args.loop);
        else:
            print("{} is not support.".format(args.input_source))
            return 1

    ret = input_source.init()
    last_module = input_source
    if ret < 0:
        print("input_source init failed")
        return 1

    if args.sync == -1:
        sync = None
    else:
        sync = m.Synchronize(m.SynchronizeType(args.sync))

    if args.aplay is not None:
        aplay = m.ModuleAacDec()
        aplay.setProductor(last_module)
        aplay.setAlsaDevice(args.aplay)
        aplay.setSynchronize(sync)
        ret = aplay.init()
        if ret <0:
            print("aac_dec init failed")
            return 1

    input_para = last_module.getOutputImagePara()
    if input_para.v4l2Fmt == m.v4l2GetFmtByName("H264") or \
        input_para.v4l2Fmt == m.v4l2GetFmtByName("MJPEG")or \
        input_para.v4l2Fmt == m.v4l2GetFmtByName("H265"):
        dec = m.ModuleMppDec()
        dec.setProductor(last_module)
        ret = dec.init()
        if ret < 0:
            print("dec init failed")
            return 1
        last_module = dec

    input_para = last_module.getOutputImagePara()
    output_para=m.ImagePara(input_para.width, input_para.height, input_para.hstride, input_para.vstride, m.v4l2GetFmtByName(args.outputfmt))
    if args.output is not None:
        match = re.match(r"(\d+)x(\d+)", args.output)
        if match:
            width, height = map(int, match.groups())
            output_para.width = align(width, 8)
            output_para.height = align(height, 8)
            output_para.hstride = width
            output_para.vstride = height

    if args.rotate !=0 or input_para.height != output_para.height or \
        input_para.height != output_para.height or \
        input_para.width != output_para.width or \
        input_para.v4l2Fmt != output_para.v4l2Fmt:
        rotate = m.RgaRotate(args.rotate)

        if rotate == m.RgaRotate.RGA_ROTATE_90 or rotate == m.RgaRotate.RGA_ROTATE_270:
            t = output_para.width
            output_para.width = output_para.height
            output_para.height = t
            t = output_para.hstride
            output_para.hstride = output_para.vstride
            output_para.vstride = t

        rga = m.ModuleRga(output_para, rotate)
        rga.setProductor(last_module)
        rga.setBufferCount(2)
        ret = rga.init()
        if ret < 0:
            print("rga init failed")
            return 1
        last_module = rga

    cv_display = None
    if args.drmdisplay != -1:
        input_para = last_module.getOutputImagePara()
        drm_display = m.ModuleDrmDisplay()
        drm_display.setPlanePara(m.v4l2GetFmtByName("NV12"), args.drmdisplay,
                                 m.PLANE_TYPE.PLANE_TYPE_OVERLAY_OR_PRIMARY, args.zpos, 1, args.connector)
        drm_display.setProductor(last_module)
        drm_display.setSynchronize(sync)
        ret = drm_display.init()
        if ret < 0:
            print("drm display init failed")
            return 1
        else:
            t_h = drm_display.getDisplayPlaneH()
            t_w = drm_display.getDisplayPlaneW()
            w = min(t_w, input_para.width)
            h = min(t_h, input_para.height)
            x = (t_w - w) // 2
            y = (t_h - h) // 2
            drm_display.setWindowSize(x, y, w, h)
    else:
        if args.x11display != 0:
            x11_display = m.ModuleRendererVideo(args.input_source)
            x11_display.setProductor(last_module)
            x11_display.setSynchronize(sync)
            ret = x11_display.init()
            if ret < 0:
                print("ModuleRendererVideo init failed")
                return 1

        if args.cvdisplay > 0:
            if not cv2_enable:
                print("Run 'pip3 install opencv-python' to install opencv")
                return 1
            if output_para.v4l2Fmt != m.v4l2GetFmtByName("BGR24"):
                print("Output image format is not 'BGR24', Use the '-b BGR24' option to specify image format.")
                return 1
            cv_display = Cv2Display("Cv2Display", None, sync, args.cvdisplay)
            cv_display.module = last_module.addExternalConsumer("Cv2Display", cv_display, cv2_extcall_back)

    if args.encodetype != -1:
        enc = m.ModuleMppEnc(m.EncodeType(args.encodetype))
        enc.setProductor(last_module)
        enc.setBufferCount(8)
        enc.setDuration(0) #Use the input source timestamp
        ret = enc.init()
        if ret < 0:
            print("ModuleMppEnc init failed")
            return 1
        last_module = enc

        if args.port != 0:
            push_s = None
            if args.push_type == 0:
                push_s = m.ModuleRtspServer("/live/0", args.port)
            else:
                push_s = m.ModuleRtmpServer("/live/0", args.port)
            push_s.setProductor(last_module)
            push_s.setBufferCount(0)
            if args.sync != -1:
                push_s.setSynchronize(m.Synchronize(m.SynchronizeType(args.sync)))

            ret = push_s.init()
            if ret < 0:
                print("push server init failed")
                return 1

        if args.rtmp_url is not None:
            push_c = m.ModuleRtmpClient(args.rtmp_url, m.ImagePara(), 0)
            push_c.setProductor(last_module)
            if args.sync != -1:
                push_c.setSynchronize(m.Synchronize(m.SynchronizeType(args.sync)))
            ret = push_c.init()
            if ret < 0:
                print("Fail to init rtmp client push")
                return 1

    if args.enmux is not None:
        enm = m.ModuleFileWriter(args.enmux)
        enm.setProductor(last_module)
        enm.init()
        if ret < 0:
            print("ModuleFileWriter init failed")
            return 1

    if args.save_file is not None:
        file = open(args.save_file, "wb")
        input_source.setOutputDataCallback(file, call_back)

    input_source.start()
    input_source.dumpPipe()
    text = input("wait...")
    input_source.dumpPipeSummary()
    input_source.stop()

    if args.save_file is not None:
        file.close()

if __name__ == "__main__":
    main()
