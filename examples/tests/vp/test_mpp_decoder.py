#!/usr/bin/env python3

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import ff_pymedia as ff
from module_test_utils import RunMonitor, dump_output_channels


DECODE_TYPES = {
    ff.MEDIA_CODEC_VIDEO_H264: ff.DECODE_TYPE_H264,
    ff.MEDIA_CODEC_VIDEO_H265: ff.DECODE_TYPE_H265,
    ff.MEDIA_CODEC_VIDEO_MJPEG: ff.DECODE_TYPE_MJPEG,
    ff.MEDIA_CODEC_VIDEO_VP8: ff.DECODE_TYPE_VP8,
    ff.MEDIA_CODEC_VIDEO_VP9: ff.DECODE_TYPE_VP9,
    ff.MEDIA_CODEC_VIDEO_MPEG1: ff.DECODE_TYPE_MPEG1,
    ff.MEDIA_CODEC_VIDEO_MPEG2: ff.DECODE_TYPE_MPEG2,
    ff.MEDIA_CODEC_VIDEO_MPEG4: ff.DECODE_TYPE_MPEG4,
}

BUFFER_TYPES = {
    "noncache": ff.DRM_BUFFER_NONCACHEABLE,
    "cache": ff.DRM_BUFFER_CACHEABLE,
    "malloc": ff.MALLOC_BUFFER,
    "dma32": ff.DRM_BUFFER_NONCACHEABLE_DMA32,
    "cache-dma32": ff.DRM_BUFFER_CACHEABLE_DMA32,
}


def parse_args():
    parser = argparse.ArgumentParser(description="Full FFmpeg-demux + MPP-decoder usage test")
    parser.add_argument("input")
    parser.add_argument("--loop", type=int, default=1)
    parser.add_argument("--probesize", default="200K")
    parser.add_argument("--timeout-us", type=int, default=5_000_000)
    parser.add_argument("--seek", type=int)
    parser.add_argument("--seek-flags", type=int, default=0)
    parser.add_argument("--split", type=int, choices=(0, 1), default=0)
    parser.add_argument("--fast", type=int, choices=(0, 1), default=1)
    parser.add_argument("--deinterlace", type=int, choices=(0, 1), default=1)
    parser.add_argument("--output-timeout-ms", type=int, default=0)
    parser.add_argument("--buffers", type=int, default=20)
    parser.add_argument("--buffer-type", choices=BUFFER_TYPES, default="noncache")
    parser.add_argument("--output-format")
    parser.add_argument("--output")
    parser.add_argument("--frames", type=int, default=0)
    parser.add_argument("--duration", type=float, default=0.0)
    parser.add_argument("--report-every", type=int, default=100)
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--change-source")
    parser.add_argument("--external-consumer", action="store_true")
    parser.add_argument("--dump-pipe", action="store_true")
    return parser.parse_args()


def main():
    args = parse_args()
    monitor = RunMonitor(args.frames, args.duration, args.report_every, args.verbose)
    monitor.install_signal_handlers()

    demuxer = ff.ModuleFFmpegDemux(args.input, args.loop)
    if args.change_source:
        ret = demuxer.changeSource(args.change_source, args.loop)
        if ret < 0:
            print(f"changeSource failed: {ret}")
            return 1
    demuxer.setFormatOption("probesize", args.probesize, 0)
    demuxer.setTimeOut(args.timeout_us)
    demuxer.setMediaStatusChangeHooker(monitor.status_callback)
    ret = demuxer.init()
    if ret < 0:
        print(f"Failed to init demuxer: {ret}")
        return 1
    if args.seek is not None:
        ret = demuxer.setFileSeek(args.seek, args.seek_flags)
        if ret < 0:
            print(f"setFileSeek failed: {ret}")
            return 1
    dump_output_channels(demuxer)

    codec = demuxer.getVideoCodec()
    if codec not in DECODE_TYPES:
        print(f"Unsupported video codec: {codec}")
        return 2
    decoder = ff.ModuleMppDec(demuxer.getOutputImagePara(), DECODE_TYPES[codec])
    ret = decoder.connectProducer(demuxer)
    if ret < 0:
        print(f"Failed to connect decoder: {ret}")
        return 1
    decoder.setNeedSplit(args.split)
    decoder.setFastMode(args.fast)
    decoder.setDeinterlace(args.deinterlace)
    decoder.setOutputTimeOut(args.output_timeout_ms)
    decoder.setBufferCount(args.buffers)
    decoder.setBufferType(BUFFER_TYPES[args.buffer_type])
    if args.output_format:
        output_para = demuxer.getOutputImagePara()
        output_para.v4l2Fmt = ff.v4l2GetFmtByName(args.output_format)
        if output_para.v4l2Fmt == 0:
            print(f"Unknown output format: {args.output_format}")
            return 2
        decoder.setOutputImagePara(output_para)

    output_file = open(args.output, "wb") if args.output else None

    def output_callback(name, queue_size, buffer):
        if output_file:
            output_file.write(buffer.getActiveData())
        monitor.output_callback(name, queue_size, buffer)

    decoder.setMediaBufferProduceHooker(output_callback)
    decoder.setMediaStatusChangeHooker(monitor.status_callback)
    ret = decoder.init()
    if ret < 0:
        if output_file:
            output_file.close()
        print(f"Failed to init decoder: {ret}")
        return 1
    dump_output_channels(decoder)

    external = None
    external_frames = [0]
    if args.external_consumer:
        def external_callback(_name, _queue_size, _buffer):
            external_frames[0] += 1
        external = decoder.addExternalConsumer("decoder-external", external_callback)
    if args.dump_pipe:
        demuxer.dumpPipe()

    monitor.reset()
    demuxer.start()
    monitor.wait()
    demuxer.stop()
    if output_file:
        output_file.close()
    if args.dump_pipe:
        demuxer.dumpPipeSummary()
    monitor.print_summary("MPP decoder")
    if external is not None:
        print(f"External consumer received {external_frames[0]} buffers")
    return 1 if monitor.abnormal else 0


if __name__ == "__main__":
    raise SystemExit(main())
