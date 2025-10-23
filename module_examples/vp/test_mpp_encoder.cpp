#include "module/vi/module_memReader.hpp"
#include "module/vp/module_mppenc.hpp"

void status_change_callback(void* ctx, ModuleStatus status)
{
    ff_info("Module state has changed(%d)\n", status);
}

void output_callback(void* ctx, shared_ptr<MediaBuffer> buffer)
{
    FILE* fp = (FILE*)ctx;
    fwrite(buffer->getActiveData(), buffer->getActiveSize(), 1, fp);

    /*
    auto buffer_type = buffer->getMediaBufferType();
    if (buffer_type == BUFFER_TYPE_VIDEO) {
        auto v_buf = dynamic_pointer_cast<VideoBuffer>(buffer);
        if (v_buf) {
            ff_info("Video[%d] pts %ld, dts %ld, fd %d, data %p, bytes %ld\n",
                    v_buf->getMediaCodec(), v_buf->getPUstimestamp(),
                    v_buf->getDUstimestamp(), v_buf->getBufFd(),
                    v_buf->getActiveData(), v_buf->getActiveSize());
        }

        auto param = buffer->getImagePara();
        ff_info("Video foramt %s, width %d, height %d\n\n", v4l2GetFmtName(param.v4l2Fmt),
                param.width, param.height);
    }
    */
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
    ImagePara input_para = ImagePara(1920, 1080, 1920, 1080, V4L2_PIX_FMT_NV12);
    auto input_buffer = make_shared<VideoBuffer>(VideoBuffer::DRM_BUFFER_NONCACHEABLE);
    input_buffer->allocBuffer(input_para);
    if (input_buffer->getActiveData() == nullptr) {
        ff_error("Failed to alloc buffer\n");
        fclose(fp);
        return -1;
    }


    /* Create a memory reader module. */
    auto r_mem = make_shared<ModuleMemReader>(input_buffer->getImagePara());
    ret = r_mem->init();
    if (ret < 0) {
        ff_error("Failed to init memory reader, %d\n", ret);
        fclose(fp);
        return ret;
    }
    r_mem->setStatusChangeCallback(nullptr, status_change_callback);

    /* Create a mpp encoder module. */
    auto v_enc = make_shared<ModuleMppEnc>(EncodeType::ENCODE_TYPE_H265);
    v_enc->setProductor(r_mem);
    ret = v_enc->init();
    if (ret < 0) {
        ff_error("Failed to init mpp module, %d\n", ret);
        fclose(fp);
        return ret;
    }

    /* Print the video stream information */
    auto v_extra = v_enc->getExtraBuffer();
    if (v_extra) {
        ff_info("Video: Extra codec %d , data %p, bytes %ld\n", v_extra->getMediaCodec(),
                v_extra->getActiveData(), v_extra->getActiveSize());

        auto param = v_extra->getImagePara();
        ff_info("foramt %s, width %d, height %d\n", v4l2GetFmtName(param.v4l2Fmt),
                param.width, param.height);
    }

    /* Set the output callback function for the encoder */
    v_enc->setOutputDataCallback(fp, output_callback);

    ff_info("\n===========================================================================\n\n");

    /* The start module will start all consumer modules under the module and all consumer modules under the consumer module. */
    r_mem->start();
    int frame_count = 600;
    while (frame_count--) {
        fill_nv12_image(input_buffer, frame_count);

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

    r_mem->setProcessStatus(ModuleMemReader::DATA_PROCESS_STATUS::PROCESS_STATUS_EXIT);

    /* The stop module will stop all consumer modules under the module and all consumer modules under the consumer module. */
    r_mem->stop();
    fclose(fp);
    return 0;
}
