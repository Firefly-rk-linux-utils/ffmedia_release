#!/usr/bin/env python3

import sys
import time
import ff_pymedia as ff

test_parallel_encoding = 1
input_frame_counter = 0
output_frame_counter = 0
init_time = time.time()
loop_time = time.time()

def output_callback(file, buffer):
    global output_frame_counter, loop_time

    output_frame_counter += 1

    if output_frame_counter % 100 == 0:
        duration = time.time() - loop_time
        fps = round(100 / duration)
        loop_time = time.time()
        print(f"encode 100 frames time {duration} fps: {fps}")

    # get memoryview object
    data = buffer.getActiveData()
    file.write(data)
    #print("Data size ", data.nbytes)

def status_change_callback(obj, status):
    print(f"{obj} module state has changed: ", status)


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
    mem_reader.setStatusChangeCallback("Memory reader", status_change_callback)
    ret = mem_reader.init()
    if ret < 0:
        print("Failed to init memory reader, ", ret)
        return ret

    last_mod = mem_reader
    if test_parallel_encoding != 0:
        # Copy the data to more buffer queues to improve encoding parallelism.
        rga_converter = ff.ModuleRga(last_mod.getOutputImagePara(), ff.RGA_ROTATE_NONE)
        rga_converter.setProductor(last_mod)
        # Set the buffer queue length to 5.
        rga_converter.setBufferCount(5)
        ret = rga_converter.init()
        if ret < 0:
            print("Failed to init rga converter, ", ret)
            return ret

        last_mod = rga_converter


    # Create a mpp encoder module.
    v_encoder = ff.ModuleMppEnc(ff.ENCODE_TYPE_H265)
    v_encoder.setProductor(last_mod)
    # Set the callback function for the encoder.
    v_encoder.setOutputDataCallback(file, output_callback)
    ret = v_encoder.init()
    if ret < 0:
        print("Failed to init mpp encoder, ", ret)


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
