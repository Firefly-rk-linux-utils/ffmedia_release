#!/usr/bin/env python3

import sys
import ff_pymedia as ff

def output_callback(obj, buffer):
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

def status_change_callback(obj, status):
    print("Module state has changed: ", status)


def main():
    if len(sys.argv) > 1:
        file_path = sys.argv[1]
    else:
        file_path = input("Please enter the file path or url: ")

    # create FFmpeg demux module.
    input_source = ff.ModuleFFmpegDemux(file_path, -1)
    input_source.setFormatOption("probesize", "200K", 0);
    # Set the callback function for the demuxer.
    input_source.setOutputDataCallback(None, output_callback);
    input_source.setStatusChangeCallback(None, status_change_callback);
    ret = input_source.init();
    if ret < 0:
        print("Failed to init demuxer")
        return 1

    # View the video stream information.
    v_extra = input_source.getExtraBuffer(ff.BUFFER_TYPE_VIDEO);
    if v_extra:
        output_callback(None, v_extra)
    # View the audio stream information.
    a_extra = input_source.getExtraBuffer(ff.BUFFER_TYPE_AUDIO);
    if a_extra:
        output_callback(None, a_extra)

    print("\n============================START=============================\n")
    input_source.start()
    input("wait...")
    input_source.stop()
    print("\n============================STOP==============================\n")

if __name__ == "__main__":
    main()
