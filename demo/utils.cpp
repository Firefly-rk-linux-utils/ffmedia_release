#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <linux/types.h>
#include "utils.hpp"
#include "base/ff_log.h"

void dump_normalbuffer_to_file(shared_ptr<VideoBuffer> buffer, FILE* fp)
{
    if (NULL == fp || NULL == buffer)
        return;
    unsigned char* base = (unsigned char*)(buffer->getActiveData());
    size_t size = buffer->getActiveSize();
    fwrite(base, 1, size, fp);
}

void dump_videobuffer_to_file(shared_ptr<VideoBuffer> buffer, FILE* fp)
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t h_stride = 0;
    uint32_t v_stride = 0;
    uint32_t fmt = 0;
    unsigned char* base = NULL;

    if (NULL == fp || NULL == buffer)
        return;

    width = buffer->getImagePara().width;
    height = buffer->getImagePara().height;
    h_stride = buffer->getImagePara().hstride;
    v_stride = buffer->getImagePara().vstride;
    fmt = buffer->getImagePara().v4l2Fmt;
    base = (unsigned char*)(buffer->getActiveData());

    switch (fmt) {
        case V4L2_PIX_FMT_NV16: {
            /* YUV422SP -> YUV422P for better display */
            uint32_t i, j;
            unsigned char* base_y = base;
            unsigned char* base_c = base + h_stride * v_stride;
            unsigned char* tmp = (unsigned char*)malloc(h_stride * height * 2);
            unsigned char* tmp_u = tmp;
            unsigned char* tmp_v = tmp + width * height / 2;

            for (i = 0; i < height; i++, base_y += h_stride)
                fwrite(base_y, 1, width, fp);

            for (i = 0; i < height; i++, base_c += h_stride) {
                for (j = 0; j < width / 2; j++) {
                    tmp_u[j] = base_c[2 * j + 0];
                    tmp_v[j] = base_c[2 * j + 1];
                }
                tmp_u += width / 2;
                tmp_v += width / 2;
            }

            fwrite(tmp, 1, width * height, fp);
            free(tmp);
        } break;
        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_NV12: {
            uint32_t i;
            unsigned char* base_y = base;
            unsigned char* base_c = base + h_stride * v_stride;

            for (i = 0; i < height; i++, base_y += h_stride) {
                fwrite(base_y, 1, width, fp);
            }
            for (i = 0; i < height / 2; i++, base_c += h_stride) {
                fwrite(base_c, 1, width, fp);
            }
        } break;
        case V4L2_PIX_FMT_YUV420: {
            uint32_t i;
            unsigned char* base_y = base;
            unsigned char* base_c = base + h_stride * v_stride;

            for (i = 0; i < height; i++, base_y += h_stride) {
                fwrite(base_y, 1, width, fp);
            }
            for (i = 0; i < height / 2; i++, base_c += h_stride / 2) {
                fwrite(base_c, 1, width / 2, fp);
            }
            for (i = 0; i < height / 2; i++, base_c += h_stride / 2) {
                fwrite(base_c, 1, width / 2, fp);
            }
        } break;
        case V4L2_PIX_FMT_NV24: {
            /* YUV444SP -> YUV444P for better display */
            uint32_t i, j;
            unsigned char* base_y = base;
            unsigned char* base_c = base + h_stride * v_stride;
            unsigned char* tmp = (unsigned char*)malloc(h_stride * height * 2);
            unsigned char* tmp_u = tmp;
            unsigned char* tmp_v = tmp + width * height;

            for (i = 0; i < height; i++, base_y += h_stride)
                fwrite(base_y, 1, width, fp);

            for (i = 0; i < height; i++, base_c += h_stride * 2) {
                for (j = 0; j < width; j++) {
                    tmp_u[j] = base_c[2 * j + 0];
                    tmp_v[j] = base_c[2 * j + 1];
                }
                tmp_u += width;
                tmp_v += width;
            }

            fwrite(tmp, 1, width * height * 2, fp);
            free(tmp);
        } break;
        case V4L2_PIX_FMT_GREY: {
            uint32_t i;
            unsigned char* base_y = base;

            for (i = 0; i < height; i++, base_y += h_stride)
                fwrite(base_y, 1, width, fp);

        } break;
        case V4L2_PIX_FMT_ARGB32:
        case V4L2_PIX_FMT_ABGR32:
        case V4L2_PIX_FMT_RGB32:
        case V4L2_PIX_FMT_BGR32: {
            uint32_t i;
            unsigned char* base_y = base;

            for (i = 0; i < height; i++, base_y += h_stride * 4)
                fwrite(base_y, 1, width * 4, fp);

        } break;
        case V4L2_PIX_FMT_RGB565:
        case V4L2_PIX_FMT_RGB555:
        case V4L2_PIX_FMT_RGB444: {
            uint32_t i;
            unsigned char* base_y = base;

            for (i = 0; i < height; i++, base_y += h_stride * 2)
                fwrite(base_y, 1, width * 2, fp);

        } break;
        default: {
            ff_error("not supported format %d\n", fmt);
        } break;
    }
}
