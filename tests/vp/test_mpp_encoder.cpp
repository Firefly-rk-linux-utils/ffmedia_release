#include "module/vi/module_memReader.hpp"
#include "module/vp/module_mppenc.hpp"
#include "tests/media_channel_dump.hpp"

#include <cerrno>
#include <cstring>

using namespace FFMedia;
using namespace std;

// #define TEST_PARALLEL_ENCODING

#ifdef TEST_PARALLEL_ENCODING
#include "module/vp/module_rga.hpp"
#endif

void status_change_callback(const string& name, MediaStatus status)
{
    ff_info("%s status changed to %d\n", name.c_str(), static_cast<int>(status));
}

void output_callback(void* ctx, const string& name, int queue_size,
                     shared_ptr<MediaBuffer> buffer)
{
    FILE* fp = (FILE*)ctx;
    fwrite(buffer->getActiveData(), buffer->getActiveSize(), 1, fp);

    static uint64_t frame_count;
    static auto start_time = std::chrono::high_resolution_clock::now();
    if (++frame_count % 100 == 0) {
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now() - start_time)
                        .count();
        ff_info("%s: queue %d, encoded %ld frames, %ld ms, %ld fps\n",
                name.c_str(), queue_size, frame_count, diff,
                frame_count * 1000 / diff);
    }
}

void fill_nv12_image(shared_ptr<VideoBuffer> buffer, int buffer_index)
{
    int x, y, i;
    auto image_param = buffer->getImagePara();
    int width = image_param.width;
    int height = image_param.height;
    int h_stride = image_param.hstride;
    int v_stride = image_param.vstride;

    uint8_t* data_y = (uint8_t*)buffer->getActiveData();
    uint8_t* data_c = data_y + h_stride * v_stride;
    i = buffer_index;

    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            data_y[y * h_stride + x] = x + y + i * 3;

    for (y = 0; y < height / 2; y++) {
        for (x = 0; x < width; x++) {
            data_c[y * h_stride + x++] = 128 + y + i * 2;
            data_c[y * h_stride + x] = 128 + x + i * 5;
        }
    }
}

int main(int argc, char** argv)
{
    int ret;
    std::shared_ptr<ModuleMedia> last_mod;

    if (argc < 2) {
        ff_error("\nUsage: %s output.h265\n", argv[0]);
        return -1;
    };

    auto fp = fopen(argv[1], "w+");
    if (fp == nullptr) {
        ff_error("Failed to open %s: %s\n", argv[1], strerror(errno));
        return -1;
    }

    /* Prepare a dummy input image. */
    ImagePara input_para = ImagePara(1920, 1080, 1920, 1080,
                                     V4L2_PIX_FMT_NV12);
    auto input_buffer = make_shared<VideoBuffer>(
        VideoBuffer::DRM_BUFFER_NONCACHEABLE);
    input_buffer->allocBuffer(input_para);
    if (input_buffer->getActiveData() == nullptr) {
        ff_error("Failed to alloc buffer\n");
        fclose(fp);
        return -1;
    }
    fill_nv12_image(input_buffer, 0);

    /* Create a memory reader module. */
    auto r_mem = make_shared<ModuleMemReader>(input_buffer->getImagePara());
    r_mem->setMediaStatusChangeHooker(status_change_callback);
    ret = r_mem->init();
    if (ret < 0) {
        ff_error("Failed to init memory reader, %d\n", ret);
        fclose(fp);
        return ret;
    }
    dumpOutputMediaChannels(*r_mem);
    last_mod = r_mem;

#ifdef TEST_PARALLEL_ENCODING
    /* Copy the data to more buffer queues to improve encoding parallelism. */
    auto rga = make_shared<ModuleRga>(last_mod->getOutputImagePara(),
                                      RGA_ROTATE_NONE);
    ret = rga->connectProducer(last_mod);
    if (ret < 0) {
        ff_error("Failed to connect memory reader to rga converter, %d\n", ret);
        fclose(fp);
        return ret;
    }
    /* Set the buffer queue length to 5. */
    rga->setBufferCount(5);
    ret = rga->init();
    if (ret < 0) {
        ff_error("Failed to init rga converter, %d\n", ret);
        fclose(fp);
        return ret;
    }
    dumpOutputMediaChannels(*rga);
    last_mod = rga;
#endif

    /* Create a mpp encoder module. */
    auto v_enc = make_shared<ModuleMppEnc>(MEDIA_CODEC_VIDEO_H265);
    ret = v_enc->connectProducer(last_mod);
    if (ret < 0) {
        ff_error("Failed to connect producer to mpp encoder, %d\n", ret);
        fclose(fp);
        return ret;
    }
    ret = v_enc->init();
    if (ret < 0) {
        ff_error("Failed to init mpp encoder, %d\n", ret);
        fclose(fp);
        return ret;
    }

    /* Print stream information through the encoder output channels. */
    dumpOutputMediaChannels(*v_enc);

    /* Set the output callback function for the encoder */
    v_enc->setMediaBufferProduceHooker([fp](const string& name, int queue_size,
                                            shared_ptr<MediaBuffer> buffer) {
        output_callback(fp, name, queue_size, buffer);
    });

    ff_info("\n========================================================\n\n");

    /* The start module will start all consumer modules under the module and
        all consumer modules under the consumer module. */
    r_mem->start();
    int frame_count = 0;
    while (frame_count++ < 2000) {
        /* There is no need to draw images in real time when testing the
            encoding rate. */
#ifndef TEST_PARALLEL_ENCODING
        fill_nv12_image(input_buffer, frame_count);
#endif

        ret = r_mem->setInputBuffer(input_buffer);
        if (ret != 0) {
            ff_error("Failed to set the input buffer\n");
            break;
        }

        ret = r_mem->waitProcess(2000);
        if (ret != 0) {
            ff_warn("Wait timeout\n");
            break;
        }
    }

    r_mem->setProcessStatus(ModuleMemReader::PROCESS_STATUS_EXIT);

    /* The stop module will stop all consumer modules under the module and all
        consumer modules under the consumer module. */
    r_mem->stop();
    fclose(fp);
    ff_info("Exit.\n");
    return 0;
}
