#!/usr/bin/env python3

import sys
import time
from functools import partial
import ff_pymedia as ff

test_parallel_encoding = 1
input_frame_counter = 0
output_frame_counter = 0
init_time = time.time()
loop_time = time.time()

def output_callback(file, name, queue_size, buffer):
    global output_frame_counter, loop_time

    output_frame_counter += 1

    if output_frame_counter % 100 == 0:
        duration = time.time() - loop_time
        fps = round(100 / duration)
        loop_time = time.time()
        print(f"{name}: queue {queue_size}, encoded 100 frames, {duration:.3f}s, {fps} fps")

    # get memoryview object
    data = buffer.getActiveData()
    file.write(data)
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


def fill_nv12_image(buffer, buffer_index):
    image_param = buffer.getImagePara()
    width = image_param.width
    height = image_param.height
    h_stride = image_param.hstride
    v_stride = image_param.vstride

    # get memoryview object
    data = buffer.getActiveData()
    i = buffer_index

    # Y plane fill
    for y in range(height):
        y_offset = y * h_stride
        for x in range(width):
            data[y_offset + x] = (x + y + i * 3) & 0xFF

    # UV plane fill
    uv_start = h_stride * v_stride
    for y in range(height // 2):
        uv_offset = uv_start + y * h_stride
        for x in range(0, width, 2):
            data[uv_offset + x] = (128 + y + i * 2) & 0xFF      # U
            data[uv_offset + x + 1] = (128 + x + i * 5) & 0xFF  # V


def main():
    global input_frame_counter, output_frame_counter, init_time, loop_time, test_parallel_encoding
    if len(sys.argv) > 1:
        file_path = sys.argv[1]
    else:
        file_path = input("Please enter the output file path: ")

    file = open(file_path, "wb")

    # Prepare a dummy input image.
    image_param = ff.ImagePara(1920, 1080, 1920, 1080, ff.v4l2GetFmtByName("NV12"))
    input_buffer = ff.VideoBuffer(ff.DRM_BUFFER_NONCACHEABLE)
    input_buffer.allocBuffer(image_param)
    if input_buffer.getSize() == 0:
        print("Failed to alloc buffer")
        return -1
    fill_nv12_image(input_buffer, 0)

    # Create a memory reader module.
    mem_reader = ff.ModuleMemReader(input_buffer.getImagePara())
    mem_reader.setMediaStatusChangeHooker(status_change_callback)
    ret = mem_reader.init()
    if ret < 0:
        print("Failed to init memory reader, ", ret)
        return ret
    dump_output_channels(mem_reader)

    last_mod = mem_reader
    if test_parallel_encoding != 0:
        # Copy the data to more buffer queues to improve encoding parallelism.
        rga_converter = ff.ModuleRga(last_mod.getOutputImagePara(), ff.RGA_ROTATE_NONE)
        ret = rga_converter.connectProducer(last_mod)
        if ret < 0:
            print("Failed to connect memory reader to rga converter, ", ret)
            return ret
        # Set the buffer queue length to 5.
        rga_converter.setBufferCount(5)
        ret = rga_converter.init()
        if ret < 0:
            print("Failed to init rga converter, ", ret)
            return ret
        dump_output_channels(rga_converter)

        last_mod = rga_converter


    # Create a mpp encoder module.
    v_encoder = ff.ModuleMppEnc(ff.MEDIA_CODEC_VIDEO_H265)
    ret = v_encoder.connectProducer(last_mod)
    if ret < 0:
        print("Failed to connect producer to mpp encoder, ", ret)
        return ret
    # Set the callback function for the encoder.
    v_encoder.setMediaBufferProduceHooker(partial(output_callback, file))
    ret = v_encoder.init()
    if ret < 0:
        print("Failed to init mpp encoder, ", ret)
        return ret
    dump_output_channels(v_encoder)


    print("\n============================START=============================\n")
    mem_reader.start()
    init_time = time.time()
    loop_time = init_time
    while input_frame_counter < 2000:
        ret = mem_reader.setInputBuffer(input_buffer)
        if ret != 0:
            print("Failed to set the input buffer, ", ret)
            break

        ret = mem_reader.waitProcess(2000)
        if ret != 0:
            print("Wait timeout")
            break

        input_frame_counter += 1
        # There is no need to draw images in real time when testing the
        # encoding rate.
        #if test_parallel_encoding == 0:
        #    fill_nv12_image(input_buffer, input_frame_counter)

    mem_reader.setProcessStatus(ff.PROCESS_STATUS_EXIT)
    mem_reader.stop()
    file.close()
    print("\n============================STOP==============================\n")
    print(f"Input frames {input_frame_counter}, output frames {output_frame_counter}")
    print("Average fps: ", input_frame_counter / (time.time() - init_time))

if __name__ == "__main__":
    main()
