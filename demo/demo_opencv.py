#!/usr/bin/env python3
import ff_pymedia as m
import argparse
import re
import os
import stat
import cv2
import threading

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


class Cv2Display(threading.Thread):
    def __init__(self, name, module, sync):
        super().__init__(name=name)
        self.module = module
        self.sync = sync
        self.is_stop = False
        self.lock = threading.Lock()
        self.condition = threading.Condition(self.lock)
        self.frame = None
        self.frame_complete = False

    def run(self):
        input_para = self.module.getOutputImagePara()

        cv2.namedWindow(self.name, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(self.name, input_para.width, input_para.height)
        cv2.startWindowThread()
        delay = 1
        while not self.is_stop and cv2.getWindowProperty(self.name, cv2.WND_PROP_VISIBLE) > 0:
            with self.lock:
                while not self.frame_complete:
                    self.condition.wait()

                data = self.frame.getActiveData()
                #flush dma buf to cpu
                self.frame.invalidateDrmBuf();

                try:
                    img = data.reshape((input_para.vstride, input_para.hstride, 3))
                except ValueError:
                    print("Invalid image resolution!")
                    resolution = find_two_numbers(data.size//3, input_para.hstride, input_para.vstride)
                    print("Try the recommended resolution: -o {}x{}".format(resolution[0], resolution[1]))
                    break

                if self.sync is not None:
                    delay = self.sync.updateVideo(self.frame.getPUstimestamp(), 0)//1000
                    if delay == 0:
                        delay = 1

                cv2.imshow(self.name, img)
                self.frame_complete = False
                self.condition.notify()

            cv2.waitKey(delay)

        cv2.destroyAllWindows()

    def stop(self):
        self.is_stop = True

def align(x, a):
    return (x + a - 1) & ~(a - 1)

def call_back(obj, MediaBuffer):
    a = MediaBuffer.getActiveData()
    obj.write(a)
#    print('VideoBuffer data type: ', a.dtype)
#    print('VideoBuffer data size: ', a.size)

def cv2_call_back(obj, MediaBuffer):
    with obj.lock:
        while obj.frame_complete:
            if not obj.condition.wait(timeout=1):
                return
        vb = obj.module.exportUseMediaBuffer(MediaBuffer, obj.frame, 0)
        if vb is not None:
            obj.frame = vb
            obj.frame_complete = True
        obj.condition.notify()


def get_parameters():
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--input_source", dest='input_source', type=str, help="input source")
    parser.add_argument("-f", "--save_file", dest='save_file', type=str, help="Enable save source output data to file, set filename, default disabled")
    parser.add_argument("-o", "--output", dest='output', type=str, help="Output image size, default same as input")
    parser.add_argument("-b", "--outputfmt", dest='outputfmt', type=str, default="NV12", help="Output image format, default NV12")
    parser.add_argument("-p", "--rtsp_transport", dest='rtsp_transport', type=int, default=0, help="Set the rtsp transport type, default 0(udp)")
    parser.add_argument("-s", "--sync", dest="sync", type=int, default=-1, help="Enable synchronization module, default disabled. e.g. -s 1")
    parser.add_argument("-a", "--aplay", dest='aplay', type=str, help="Enable play audio, default disabled. e.g. -a plughw:3,0")
    parser.add_argument("-r", "--rotate", dest='rotate',type=int, default=0, help="Image rotation degree, default 0" )
    parser.add_argument("-d", "--drmdisplay", dest='drmdisplay', type=int, default=-1, help="Drm display, set display plane, set 0 to auto find plane")
    parser.add_argument("-c", "--cvdisplay", dest='cvdisplay', type=int, default=0, help="opencv display, default disabled")
    return parser.parse_args()

def main():

    args = get_parameters()

    if args.input_source is None:
        return 1
    elif args.input_source.startswith("rtsp://"):
        print("input source is a rtsp url")
        input_source = m.ModuleRtspClient(args.input_source, m.RTSP_STREAM_TYPE(args.rtsp_transport))
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
        rga.setBufferCount(1)
        ret = rga.init()
        if ret < 0:
            print("rga init failed")
            return 1
        last_module = rga

    cv_display = None
    if args.drmdisplay != -1:
        input_para = last_module.getOutputImagePara()
        drm_display = m.ModuleDrmDisplay(input_para)
        drm_display.setPlanePara(m.v4l2GetFmtByName("NV12"), args.drmdisplay, m.PLANE_TYPE.PLANE_TYPE_OVERLAY_OR_PRIMARY, 0xff)
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
    elif args.cvdisplay != 0:
        input_para = last_module.getOutputImagePara()
        if input_para.v4l2Fmt != m.v4l2GetFmtByName("BGR24"):
            print("Output image format is not 'BGR24', Use the '-b BGR24' option to specify image format.")
            return

        cv_display = Cv2Display("Cv2Display", last_module, sync)
        last_module.setOutputDataCallback(cv_display, cv2_call_back)
        cv_display.frame = last_module.newModuleMediaBuffer(m.BUFFER_TYPE.DRM_BUFFER_CACHEABLE)
        cv_display.start()

    if args.save_file is not None:
        file = open(args.save_file, "wb")
        input_source.setOutputDataCallback(file, call_back)

    input_source.start()
    text = input("wait...")

    if cv_display is not None:
        cv_display.stop()
        cv_display.join()
    input_source.stop()

    if args.save_file is not None:
        file.close()

if __name__ == "__main__":
    main()
