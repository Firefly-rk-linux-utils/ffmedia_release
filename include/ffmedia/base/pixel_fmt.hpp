#pragma once

#include <linux/videodev2.h>
#include <cstdint>
#include "ff_log.h"
#include "ff_type.hpp"

#define ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))

namespace FFMedia
{
enum class ImageCompression : uint8_t {
    Linear = 0,
    Afbc16x16
};

FFMEDIA_API const char* v4l2GetFmtName(uint32_t v4l2_fmt);
FFMEDIA_API const char* drmGetFmtName(uint32_t drm_fmt);

struct ImagePara {
    uint32_t width;
    uint32_t height;
    uint32_t hstride;
    uint32_t vstride;
    uint32_t v4l2Fmt;
    ImageCompression compression;
    bool operator==(const ImagePara& b) const
    {
        return (this->width == b.width)
               && (this->height == b.height)
               && (this->v4l2Fmt == b.v4l2Fmt)
               && (this->hstride == b.hstride)
               && (this->vstride == b.vstride)
               && (this->compression == b.compression);
    };

    ImagePara(uint32_t w, uint32_t h, uint32_t hs, uint32_t vs, uint32_t fmt,
              ImageCompression compression_ = ImageCompression::Linear)
        : width(w), height(h), hstride(hs), vstride(vs), v4l2Fmt(fmt),
          compression(compression_){};
    ImagePara()
        : width(0), height(0), hstride(0), vstride(0), v4l2Fmt(0),
          compression(ImageCompression::Linear){};
    void dump()
    {
        ff_info("size(%d x %d), stride(%d x %d), format(%s), compression(%s)\n",
                width, height, hstride, vstride, v4l2GetFmtName(v4l2Fmt),
                compression == ImageCompression::Afbc16x16 ? "AFBC 16x16"
                                                           : "linear");
    }
};

struct ImageCrop {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
};

FFMEDIA_API uint32_t v4l2ToDrmFormat(uint32_t v4l2_fmt);
FFMEDIA_API size_t v4l2GetFrameSize(uint32_t v4l2_fmt, int width, int height);
FFMEDIA_API uint32_t v4l2GetFmtByName(const char* name);
FFMEDIA_API ImageCrop getCenterCrop(ImagePara& src_para, ImagePara& dst_para);
FFMEDIA_API ImageCrop getLetterboxCrop(const ImagePara& src_para, const ImagePara& dst_para);
FFMEDIA_API bool v4l2fmtIsCompressed(uint32_t v4l2_fmt);
}  // namespace FFMedia
