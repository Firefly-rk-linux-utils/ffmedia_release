#!/usr/bin/env python3

import sys
import time
import ff_pymedia as ff

frame_counter = 0
init_time = time.time()

def output_callback(obj, buffer):
    global frame_counter, init_time

    frame_counter += 1

    if frame_counter == 1:
        init_time = time.time()

    if frame_counter % 100 == 0:
        duration = time.time() - init_time
        fps = frame_counter / duration
        print(f"{obj} decode {frame_counter} frames time {duration} fps: {fps}")

    # get memoryview object
    #data = buffer.getActiveData()
    #print("Data size ", data.nbytes)

def status_change_callback(obj, status):
    print(f"{obj} module state has changed: ", status)


def dump_mediabuffer(buffer):
    buffer_type = buffer.getMediaBufferType()
    if buffer_type == ff.BUFFER_TYPE_VIDEO:
        v_param = buffer.getImagePara()
        print("Video[{}] format {}, width {}, height {}, pts {}, dts {}".format\
                (buffer.getMediaCodec(), v_param.v4l2Fmt, v_param.width,\
                v_param.height, buffer.getPUstimestamp(),\
                buffer.getDUstimestamp()))

    elif buffer_type == ff.BUFFER_TYPE_AUDIO:
        a_param = buffer.getSamplePara()
        print("Audio[{}] format {}, channels {}, sample rate {}, pts {}, dts {}"\
                .format(buffer.getMediaCodec(), a_param.fmt, a_param.channels,\
                a_param.sample_rate, buffer.getPUstimestamp(),\
                buffer.getDUstimestamp()))

    # get memoryview object
    data = buffer.getActiveData()
    print("Data size ", data.nbytes)

def main():
    if len(sys.argv) > 1:
        file_path = sys.argv[1]
    else:
        file_path = input("Please enter the file path or url: ")

    # create FFmpeg demux module.
    demuxer = ff.ModuleFFmpegDemux(file_path, -1)
    demuxer.setFormatOption("probesize", "200K", 0)
    # Set the callback function for the demuxer.
    demuxer.setStatusChangeCallback("Demuxer", status_change_callback)
    ret = demuxer.init()
    if ret < 0:
        print("Failed to init demuxer")
        return 1

    # View the video stream information.
    v_extra = demuxer.getExtraBuffer(ff.BUFFER_TYPE_VIDEO)
    if v_extra:
        dump_mediabuffer(v_extra)
    # View the audio stream information.
    a_extra = demuxer.getExtraBuffer(ff.BUFFER_TYPE_AUDIO)
    if a_extra:
        dump_mediabuffer(a_extra)

    # Create a mpp decoder module.
    v_decoder = ff.ModuleMppDec()
    v_decoder.setProductor(demuxer)
    # Set the callback function for the decoder.
    v_decoder.setOutputDataCallback("MPP decoder", output_callback)
    ret = v_decoder.init()
    if ret < 0:
        print("Failed to init mpp decoder, \t", ret)


    print("\n============================START=============================\n")
    demuxer.start()
    input("wait...")
    demuxer.stop()
    print("\n============================STOP==============================\n")

if __name__ == "__main__":
    main()
