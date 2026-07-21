#!/usr/bin/env python3

import sys
import time
import ff_pymedia as ff

frame_counter = 0
init_time = time.time()

def output_callback(name, queue_size, buffer):
    global frame_counter, init_time

    frame_counter += 1

    if frame_counter == 1:
        init_time = time.time()

    if frame_counter % 100 == 0:
        duration = time.time() - init_time
        fps = frame_counter / duration
        print(
            f"{name}: queue {queue_size}, decoded {frame_counter} frames, "
            f"{duration:.3f}s, {fps:.2f} fps"
        )

    # get memoryview object
    #data = buffer.getActiveData()
    #print("Data size ", data.nbytes)

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
    demuxer = ff.ModuleFFmpegDemux(file_path, -1)
    demuxer.setFormatOption("probesize", "200K", 0)
    # Set the callback function for the demuxer.
    demuxer.setMediaStatusChangeHooker(status_change_callback)
    ret = demuxer.init()
    if ret < 0:
        print("Failed to init demuxer")
        return 1

    # View stream information through the demuxer output channels.
    dump_output_channels(demuxer)

    # Create a mpp decoder module.
    v_decoder = ff.ModuleMppDec(demuxer.getOutputImagePara())
    ret = v_decoder.connectProducer(demuxer)
    if ret < 0:
        print("Failed to connect demuxer to mpp decoder, \t", ret)
        return ret
    # Set the callback function for the decoder.
    v_decoder.setMediaBufferProduceHooker(output_callback)
    ret = v_decoder.init()
    if ret < 0:
        print("Failed to init mpp decoder, \t", ret)
        return ret

    dump_output_channels(v_decoder)


    print("\n============================START=============================\n")
    demuxer.start()
    input("wait...")
    demuxer.stop()
    print("\n============================STOP==============================\n")

if __name__ == "__main__":
    main()
