#!/usr/bin/env python3

import sys
import ff_pymedia as ff

def output_callback(name, queue_size, buffer):
    print(
        f"{name}: queue {queue_size}, channel {buffer.getMediaChannelId()}, "
        f"type {buffer.getMediaBufferType()}, codec {buffer.getMediaCodec()}, "
        f"bytes {buffer.getActiveSize()}, pts {buffer.getPUstimestamp()}, "
        f"dts {buffer.getDUstimestamp()}"
    )

def status_change_callback(name, status):
    print(f"{name} module state has changed: ", status)


def dump_output_channels(module):
    print(f"{module.getName()} output channels:")
    for channel in module.getOutputMediaChannels():
        extra_size = channel.extra_data.getActiveSize() if channel.extra_data else 0
        if channel.media_type == ff.BUFFER_TYPE_VIDEO:
            image = channel.image_para
            print(
                f"  Channel[{channel.id}] {channel.name}: video, "
                f"codec {channel.codec}, format {image.v4l2Fmt}, "
                f"size {image.width}x{image.height}, "
                f"stride {image.hstride}x{image.vstride}, extra {extra_size} bytes"
            )
        elif channel.media_type == ff.BUFFER_TYPE_AUDIO:
            sample = channel.sample_info
            print(
                f"  Channel[{channel.id}] {channel.name}: audio, "
                f"codec {channel.codec}, format {sample.fmt}, "
                f"channels {sample.channels}, sample rate {sample.sample_rate}, "
                f"samples {sample.nb_samples}, extra {extra_size} bytes"
            )
        else:
            print(
                f"  Channel[{channel.id}] {channel.name}: "
                f"type {channel.media_type}, codec {channel.codec}, "
                f"extra {extra_size} bytes"
            )


def main():
    if len(sys.argv) > 1:
        file_path = sys.argv[1]
    else:
        file_path = input("Please enter the file path or url: ")

    # create FFmpeg demux module.
    input_source = ff.ModuleFFmpegDemux(file_path, -1)
    input_source.setFormatOption("probesize", "200K", 0)
    # Set the callback function for the demuxer.
    input_source.setMediaBufferProduceHooker(output_callback)
    input_source.setMediaStatusChangeHooker(status_change_callback)
    ret = input_source.init()
    if ret < 0:
        print("Failed to init demuxer")
        return 1

    # View stream information through the module output channels.
    dump_output_channels(input_source)

    print("\n============================START=============================\n")
    input_source.start()
    input("wait...")
    input_source.stop()
    print("\n============================STOP==============================\n")

if __name__ == "__main__":
    main()
