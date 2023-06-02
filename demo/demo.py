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

def cv2_extcall_back(obj, VideoBuffer):
    vb = m.VideoBuffer.from_base(VideoBuffer)
    if obj.sync is not None:
        delay = obj.sync.updateVideo(vb.getPUstimestamp(), 0)
        if delay > 0:
            time.sleep(delay/1000000)
    data = vb.getActiveData()
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

def call_back(obj, VideoBuffer):
    a = VideoBuffer.getActiveData()
    obj.write(a)

def get_parameters():
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--input_source", dest='input_source', type=str, help="input source")
    parser.add_argument("-f", "--save_file", dest='save_file', type=str, help="Enable save dec output data to file, set filename, default disabled")
    parser.add_argument("-o", "--output", dest='output', type=str, help="Output image size, default same as input")
    parser.add_argument("-b", "--outputfmt", dest='outputfmt', type=str, default="NV12", help="Output image format, default NV12")
    parser.add_argument("-e", "--encodetype", dest='encodetype', type=int, default=-1, help="Encode encode, set encode type, default disabled")
    parser.add_argument("-m", "--enmux", dest='enmux', type=str, help="Enable save encode data to file, Enable package as mp4, mkv, or raw stream files depending on the file name suffix. default disabled")
    parser.add_argument("-M", "--filemaxframe", dest='filemaxframe', type=int, default= 0, help="Set the maximum number of frames that can be saved. The default number is unlimited")
    parser.add_argument("-p", "--port", dest='port', type=int, default=0, help="Enable rtsp stream, set push port, depend on encode enabled, default disabled\n")
    parser.add_argument("--rtsp_transport", dest='rtsp_transport', type=int, default=0, help="Set the rtsp transport type，default 0(udp)")
    parser.add_argument("-s", "--sync", dest="sync", type=int, default=-1, help="Enable synchronization module, default disabled. Enable the default 0(audio)")
    parser.add_argument("-a", "--aplay", dest='aplay', type=str, help="Enable play audio, default disabled. e.g. -a plughw:3,0")
    parser.add_argument("-r", "--rotate", dest='rotate',type=int, default=0, help="Image rotation degree, default 0" )
    parser.add_argument("-d", "--drmdisplay", dest='drmdisplay', type=int, default=-1, help="Drm display, set display plane, set 0 to auto find plane")
    parser.add_argument("-c", "--cvdisplay", dest='cvdisplay', type=int, default=0, help="OpenCv display, set window number, default 0")
    return parser.parse_args()

def main():

    args = get_parameters()

    if args.input_source is None:
        return 1
    elif args.input_source.startswith("rtsp://"):
        print("input source is a rtsp url")
        input_source = m.ModuleRtspClient(args.input_source, args.rtsp_transport)
    else:
        is_stat = os.stat(args.input_source)
        if stat.S_ISCHR(is_stat.st_mode):
            print("input source is a camera device.")
            input_source = m.ModuleCam(args.input_source)
        elif stat.S_ISREG(is_stat.st_mode):
            print("input source is a regular file.")
            input_source = m.ModuleFileReader(args.input_source, 1);
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
        input_source.setSynchronize(sync)

    if args.aplay is not None:
        extra_data = last_module.audioExtraData()
        aplay = m.ModuleAacDec(extra_data.tobytes(),last_module.audioExtraDataSize(), -1, -1)
        aplay.setProductor(last_module)
        aplay.setBufferCount(1)
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
        dec = m.ModuleMppDec(input_para)
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

        rga = m.ModuleRga(input_para, output_para, rotate)
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
        drm_display = m.ModuleDrmDisplay(input_para)
        drm_display.setPlanePara(m.v4l2GetFmtByName("NV12"), args.drmdisplay, m.PLANE_TYPE.PLANE_TYPE_OVERLAY_OR_PRIMARY, 1)
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
    elif args.cvdisplay > 0:
        if not cv2_enable:
            print("Run 'pip3 install opencv-python' to install opencv")
            return 1
        if output_para.v4l2Fmt != m.v4l2GetFmtByName("RGB24"):
            print("Output image format is not 'RGB24', Use the '-b RGB24' option to specify image format.")
            return 1
        cv_display = Cv2Display("Cv2Display", None, sync, args.cvdisplay)
        cv_display.module = last_module.addExternalConsumer("Cv2Display", cv_display, cv2_extcall_back)

    if args.encodetype != -1:
        input_para = last_module.getOutputImagePara()
        enc = m.ModuleMppEnc(m.EncodeType(args.encodetype), input_para)
        enc.setProductor(last_module)
        enc.setBufferCount(8)
        ret = enc.init()
        if ret < 0:
            print("ModuleMppEnc init failed")
            return 1
        last_module = enc

        if args.port != 0:
            input_para = last_module.getOutputImagePara()
            rtsp_out = m.ModuleRtspServer(input_para, "/live/0", args.port)
            rtsp_out.setProductor(last_module)
            rtsp_out.setBufferCount(0)
            rtsp_out.setSynchronize(sync)
            ret = rtsp_out.init()
            if ret < 0:
                print("rtsp_out init failed")
                return 1

    if args.enmux is not None:
        input_para = last_module.getOutputImagePara()
        enm = m.ModuleFileWriter(input_para, args.enmux)
        enm.setProductor(last_module)
        enm.setBufferCount(1)

        if args.filemaxframe > 0:
            enm.setMaxFrameCount(args.filemaxframe)
        enm.init()
        if ret < 0:
            print("ModuleFileWriter init failed")
            return 1

    if args.save_file is not None:
        file = open(args.save_file, "wb")
        input_source.setOutputDataCallback(file, call_back)

    input_source.start()
    text = input("wait...")
    input_source.stop()

    if args.save_file is not None:
        file.close()

if __name__ == "__main__":
    main()
