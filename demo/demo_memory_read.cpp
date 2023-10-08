#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "module/vi/module_memReader.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vo/module_drmDisplay.hpp"


int H264ReadFrame(FILE* fp, char* in_buf, int in_buf_size)
{
    int bytes_used = 0, block_size = 4096;
    size_t seek = ftell(fp);
    bool is_find_end = false;
    int i = 5;  // Skip the first startcode

    while (!is_find_end) {
        if (in_buf_size < bytes_used + block_size)
            break;
        size_t bytes_read = fread(&in_buf[bytes_used], 1, block_size, fp);
        if (bytes_read == 0) {
            return bytes_used;
        }

        bytes_used += bytes_read;

        for (; i < bytes_used - 4; i++) {
            if (in_buf[i] == 0 && in_buf[i + 1] == 0 && in_buf[i + 2] == 0 && in_buf[i + 3] == 1) {
                is_find_end = true;
                break;
            }
        }
    }

    fseek(fp, seek + i, SEEK_SET);
    return i;
}


//./demo_memory_read xxxxx.h264 width height
int main(int argc, char** argv)
{
    int ret = -1;
    shared_ptr<ModuleMemReader> mem_r = NULL;
    shared_ptr<ModuleMppDec> dec = NULL;
    shared_ptr<ModuleDrmDisplay> drm_display = NULL;
    uint32_t width, height;
    char* buf = nullptr;
    uint32_t buf_size;
    FILE* fp = nullptr;

    if (argc < 4) {
        ff_error("The number of parameters is incorrect\n");
        return -1;
    }

    do {
        fp = fopen(argv[1], "rb");
        if (!fp) {
            ff_error("open file %s failed, reason = %s \n", argv[0], strerror(errno));
            break;
        }
        width = atoi(argv[2]);
        height = atoi(argv[3]);
        buf_size = width * height;
        if (buf_size == 0 || buf_size > 128 * 1024 * 1024) {
            ff_error("Image size error\n");
            break;
        }
        buf = new char[buf_size];

        // 1. memory reader module
        ImagePara input_para = ImagePara(width, height, width, height, V4L2_PIX_FMT_H264);
        mem_r = make_shared<ModuleMemReader>(input_para);
        ret = mem_r->init();
        if (ret < 0) {
            ff_error("memory reader init failed\n");
            break;
        }

        // 2. dec module
        input_para = mem_r->getOutputImagePara();
        dec = make_shared<ModuleMppDec>(input_para);
        dec->setProductor(mem_r);
        ret = dec->init();
        if (ret < 0) {
            ff_error("Dec init failed\n");
            break;
        }

        // 3. drm display module
        input_para = dec->getOutputImagePara();
        drm_display = make_shared<ModuleDrmDisplay>(input_para);
        drm_display->setPlanePara(V4L2_PIX_FMT_NV12);
        drm_display->setProductor(dec);
        ret = drm_display->init();
        if (ret < 0) {
            ff_error("drm display init failed\n");
            break;
        } else {
            uint32_t t_w, t_h;
            drm_display->getPlaneSize(&t_w, &t_h);
            uint32_t w = std::min(t_w / 2, input_para.width);
            uint32_t h = std::min(t_h / 2, input_para.height);
            uint32_t x = (t_w - w) / 2;
            uint32_t y = (t_h - h) / 2;

            ff_info("x y w h %d %d %d %d\n", x, y, w, h);
            drm_display->setWindowRect(x, y, w, h);
        }

        // 4. start origin producer
        mem_r->start();

        while (true) {
            int bytes = H264ReadFrame(fp, buf, buf_size);
            if (bytes == 0)
                break;

            ret = mem_r->setInputBuffer(buf, bytes);
            if (ret != 0) {
                ff_error("Failed to set the input buf\n");
                break;
            }
            ret = mem_r->waitProcess(2000);
            if (ret != 0) {
                ff_warn("Wait timeout\n");
                if (mem_r->waitProcess(2000))
                    break;
            }
        }

        mem_r->setProcessStatus(ModuleMemReader::DATA_PROCESS_STATUS::PROCESS_STATUS_EXIT);
        mem_r->stop();

    } while (0);

    if (buf)
        delete[] buf;
    if (fp)
        fclose(fp);
    return ret;
}
