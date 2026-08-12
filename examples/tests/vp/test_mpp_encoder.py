#!/usr/bin/env python3

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import ff_pymedia as ff
from module_test_utils import RunMonitor, dump_output_channels


CODECS = {
    "h264": ff.MEDIA_CODEC_VIDEO_H264,
    "h265": ff.MEDIA_CODEC_VIDEO_H265,
    "mjpeg": ff.MEDIA_CODEC_VIDEO_MJPEG,
}
RC_MODES = {
    "cbr": ff.ENCODE_RC_MODE_CBR,
    "vbr": ff.ENCODE_RC_MODE_VBR,
    "fixqp": ff.ENCODE_RC_MODE_FIXQP,
    "avbr": ff.ENCODE_RC_MODE_AVBR,
}
PROFILES = {
    "baseline": ff.ENCODE_PROFILE_BASELINE,
    "main": ff.ENCODE_PROFILE_MAIN,
    "high": ff.ENCODE_PROFILE_HIGH,
}
BUFFER_TYPES = {
    "noncache": ff.DRM_BUFFER_NONCACHEABLE,
    "cache": ff.DRM_BUFFER_CACHEABLE,
    "malloc": ff.MALLOC_BUFFER,
    "dma32": ff.DRM_BUFFER_NONCACHEABLE_DMA32,
    "cache-dma32": ff.DRM_BUFFER_CACHEABLE_DMA32,
}


def parse_args():
    parser = argparse.ArgumentParser(description="Full ModuleMppEnc usage test")
    parser.add_argument("output")
    parser.add_argument("--codec", choices=CODECS, default="h265")
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    parser.add_argument("--hstride", type=int)
    parser.add_argument("--vstride", type=int)
    parser.add_argument("--format", default="nv12")
    parser.add_argument("--buffer-type", choices=BUFFER_TYPES, default="noncache")
    parser.add_argument("--fps", type=int, default=30)
    parser.add_argument("--gop", type=int, default=60)
    parser.add_argument("--bps", type=int, default=2048)
    parser.add_argument("--rc", choices=RC_MODES, default="cbr")
    parser.add_argument("--quality", type=float, default=0.8)
    parser.add_argument("--profile", choices=PROFILES, default="high")
    parser.add_argument("--duration-us", type=int)
    parser.add_argument("--output-timeout-ms", type=int, default=-1)
    parser.add_argument("--cache-frames", type=int, default=0)
    parser.add_argument("--intra-refresh", action="store_true")
    parser.add_argument("--refresh-mode", type=int, choices=(0, 1), default=0)
    parser.add_argument("--refresh-num", type=int, default=10)
    parser.add_argument("--parallel-buffers", type=int, default=0)
    parser.add_argument("--buffers", type=int, default=8)
    parser.add_argument("--frames", type=int, default=300)
    parser.add_argument("--duration", type=float, default=0.0)
    parser.add_argument("--wait-timeout-ms", type=int, default=2000)
    parser.add_argument("--drain-timeout-ms", type=int, default=2000)
    parser.add_argument("--animate", action="store_true")
    parser.add_argument("--report-every", type=int, default=100)
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--external-consumer", action="store_true")
    parser.add_argument("--dump-pipe", action="store_true")
    return parser.parse_args()


def main():
    args = parse_args()
    if args.cache_frames > 1 and args.parallel_buffers <= args.cache_frames:
        print("cache-frames must be smaller than parallel-buffers when greater than 1")
        return 2
    pixel_format = ff.v4l2GetFmtByName(args.format)
    if not pixel_format or args.width <= 0 or args.height <= 0:
        print("Invalid dimensions or pixel format")
        return 2
    image_para = ff.ImagePara(
        args.width, args.height, args.hstride or args.width,
        args.vstride or args.height, pixel_format
    )
    input_buffer = ff.VideoBuffer(BUFFER_TYPES[args.buffer_type])
    input_buffer.allocBuffer(image_para)
    if input_buffer.getSize() == 0:
        print("Failed to allocate input buffer")
        return 1
    input_buffer.fillWithColor(32, 128, 224)
    if args.buffer_type in ("cache", "cache-dma32"):
        input_buffer.flushDrmBuf()

    reader = ff.ModuleMemReader(image_para)
    ret = reader.init()
    if ret < 0:
        print(f"Failed to init memory reader: {ret}")
        return 1
    dump_output_channels(reader)

    producer = reader
    rga = None
    if args.parallel_buffers > 0:
        rga = ff.ModuleRga(image_para, ff.RGA_ROTATE_NONE)
        ret = rga.connectProducer(reader)
        if ret < 0:
            print(f"Failed to connect RGA: {ret}")
            return 1
        rga.setBufferCount(args.parallel_buffers)
        ret = rga.init()
        if ret < 0:
            print(f"Failed to init RGA: {ret}")
            return 1
        dump_output_channels(rga)
        producer = rga

    encoder = ff.ModuleMppEnc(
        CODECS[args.codec], args.fps, args.gop, args.bps,
        RC_MODES[args.rc], args.quality, PROFILES[args.profile]
    )
    ret = encoder.connectProducer(producer)
    if ret < 0:
        print(f"Failed to connect encoder: {ret}")
        return 1
    encoder.setBufferCount(args.buffers)
    encoder.setOutputTimeOut(args.output_timeout_ms)
    encoder.setInputCachePoolSize(args.cache_frames)
    if args.duration_us is not None:
        encoder.setDuration(args.duration_us)
    if args.intra_refresh:
        encoder.setIntraRefresh(True, args.refresh_mode, args.refresh_num)

    monitor = RunMonitor(args.frames, args.duration, args.report_every, args.verbose)
    monitor.install_signal_handlers()
    reader.setMediaStatusChangeHooker(monitor.status_callback)
    encoder.setMediaStatusChangeHooker(monitor.status_callback)
    output_file = open(args.output, "wb")

    def output_callback(name, queue_size, buffer):
        output_file.write(buffer.getActiveData())
        monitor.output_callback(name, queue_size, buffer)

    encoder.setMediaBufferProduceHooker(output_callback)
    ret = encoder.init()
    if ret < 0:
        output_file.close()
        print(f"Failed to init encoder: {ret}")
        return 1
    dump_output_channels(encoder)
    extra = encoder.getExtraBuffer()
    print(f"Encoder extra data: {extra.getActiveSize() if extra else 0} bytes")

    external = None
    external_frames = [0]
    if args.external_consumer:
        def external_callback(_name, _queue_size, _buffer):
            external_frames[0] += 1
        external = encoder.addExternalConsumer("encoder-external", external_callback)
    if args.dump_pipe:
        reader.dumpPipe()

    monitor.reset()
    reader.start()
    input_frames = 0
    start_time = time.monotonic()
    while not monitor.stop_event.is_set() and (args.frames == 0 or input_frames < args.frames):
        if args.duration > 0 and time.monotonic() - start_time >= args.duration:
            break
        if args.animate:
            input_buffer.fillWithColor(
                input_frames & 0xFF, (input_frames * 3) & 0xFF,
                (input_frames * 7) & 0xFF
            )
            if args.buffer_type in ("cache", "cache-dma32"):
                input_buffer.flushDrmBuf()
        input_buffer.setPUstimestamp(input_frames * 1_000_000 // args.fps if args.fps else 0)
        ret = reader.setInputBuffer(input_buffer)
        if ret != 0:
            print(f"setInputBuffer failed: {ret}")
            break
        ret = reader.waitProcess(args.wait_timeout_ms)
        if ret != 0:
            print(f"waitProcess failed or timed out: {ret}")
            break
        input_frames += 1

    drain_deadline = time.monotonic() + args.drain_timeout_ms / 1000.0
    while ret == 0 and monitor.frames < input_frames and time.monotonic() < drain_deadline:
        time.sleep(0.01)
    incomplete_output = monitor.frames != input_frames
    if incomplete_output:
        print(f"Encoder drain incomplete: input {input_frames}, output {monitor.frames}")

    reader.setProcessStatus(ff.PROCESS_STATUS_EXIT)
    encoder.stop()
    if rga is not None:
        rga.stop()
    reader.stop()
    output_file.close()
    if args.dump_pipe:
        reader.dumpPipeSummary()
    monitor.print_summary("MPP encoder")
    print(f"Input frames={input_frames}, output frames={monitor.frames}")
    if external is not None:
        print(f"External consumer received {external_frames[0]} buffers")
    return 1 if ret != 0 or monitor.abnormal or incomplete_output else 0


if __name__ == "__main__":
    raise SystemExit(main())
