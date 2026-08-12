#!/usr/bin/env python3

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import ff_pymedia as ff
from module_test_utils import RunMonitor, dump_output_channels


def parse_args():
    parser = argparse.ArgumentParser(description="Full ModuleFFmpegDemux usage test")
    parser.add_argument("input", help="media file, URL, or input device")
    parser.add_argument("--loop", type=int, default=1, help="loop count; -1 loops forever")
    parser.add_argument("--input-format", help="force FFmpeg input format")
    parser.add_argument(
        "--format-option", action="append", default=["probesize=200K"],
        metavar="KEY=VALUE", help="FFmpeg format option; may be repeated"
    )
    parser.add_argument("--timeout-us", type=int, default=5_000_000)
    parser.add_argument("--seek", type=int)
    parser.add_argument("--seek-flags", type=int, default=0)
    parser.add_argument("--buffers", type=int, default=20)
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
    if args.input_format:
        ret = demuxer.setInputFormat(args.input_format)
        if ret < 0:
            print(f"setInputFormat failed: {ret}")
            return 1
    for item in args.format_option:
        key, separator, value = item.partition("=")
        if not separator or not key:
            print(f"Invalid format option: {item}")
            return 2
        ret = demuxer.setFormatOption(key, value, 0)
        if ret < 0:
            print(f"setFormatOption({key}) failed: {ret}")
            return 1
        print(f"Format option {key}={demuxer.getFormatOption(key, 0)}")
    demuxer.setTimeOut(args.timeout_us)
    demuxer.setBufferCount(args.buffers)
    demuxer.setMediaBufferProduceHooker(monitor.output_callback)
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
    print(f"Video codec={demuxer.getVideoCodec()}, audio codec={demuxer.getAudioCodec()}")
    for media_type, label in ((ff.BUFFER_TYPE_VIDEO, "video"),
                              (ff.BUFFER_TYPE_AUDIO, "audio")):
        extra = demuxer.getExtraBuffer(media_type)
        print(f"{label} extra data: {extra.getActiveSize() if extra else 0} bytes")

    external = None
    external_frames = [0]
    if args.external_consumer:
        def external_callback(_name, _queue_size, _buffer):
            external_frames[0] += 1
        external = demuxer.addExternalConsumer("ffmpeg-demux-external", external_callback)
    if args.dump_pipe:
        demuxer.dumpPipe()

    monitor.reset()
    demuxer.start()
    monitor.wait()
    demuxer.stop()
    if args.dump_pipe:
        demuxer.dumpPipeSummary()
    monitor.print_summary("FFmpeg demuxer")
    if external is not None:
        print(f"External consumer received {external_frames[0]} buffers")
    return 1 if monitor.abnormal else 0


if __name__ == "__main__":
    raise SystemExit(main())
