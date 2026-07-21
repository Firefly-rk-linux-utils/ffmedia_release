#pragma once


#include "media_buffer.hpp"

namespace FFMedia
{
class DrmBuffer;
typedef void* MppBuffer;
typedef void* MppBufferGroup;

class FFMEDIA_API VideoBuffer : public MediaBuffer
{
public:
    enum BUFFER_TYPE {
        DRM_BUFFER_NONCACHEABLE,
        DRM_BUFFER_CACHEABLE,
        MALLOC_BUFFER,
        EXTERNAL_BUFFER,
        DRM_BUFFER_NONCACHEABLE_DMA32,
        DRM_BUFFER_CACHEABLE_DMA32
    };

private:
    DrmBuffer* drm_buf;
    MppBuffer mpp_buf;
    BUFFER_TYPE buffer_type;
    int buf_fd;
    void fillWithColorInternal(uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                               uint32_t x, uint32_t y, uint32_t w, uint32_t h);

public:
    VideoBuffer(BUFFER_TYPE type);
    VideoBuffer(const VideoBuffer& other);
    VideoBuffer& operator=(const VideoBuffer&) = delete;
    ~VideoBuffer() override;
    void resetBuffer();
    void allocBuffer(ImagePara para);
    void allocBuffer(size_t _size) override;
    void fillWithBlack() override;
    std::shared_ptr<MediaBuffer> clone() const override;
    void fillWithBlack(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    void fillWithColor(uint8_t r, uint8_t g, uint8_t b);
    void fillWithColor(uint8_t r, uint8_t g, uint8_t b, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    int releaseMppBuffer();
    void initWithExternalBuffer(void* data_, size_t size_, int fd_);
    int importToMppBufferGroup(MppBufferGroup group);
    int importToMppBufferGroupUsed(MppBufferGroup group);
    int importToMppBufferGroupExtra(MppBufferGroup group, bool used);

public:
    MppBuffer getMppBuf() const { return mpp_buf; }
    void setMppBuf(const MppBuffer& mppBuf) { mpp_buf = mppBuf; }

    DrmBuffer* getDrmBuf() const { return drm_buf; }
    void setDrmBuf(DrmBuffer* drmBuf) { drm_buf = drmBuf; }

    int getBufFd() const { return buf_fd; }
    void setBufFd(int bufFd) { buf_fd = bufFd; }

    BUFFER_TYPE getBufferType() const { return buffer_type; }
    void setBufferType(const BUFFER_TYPE& bufferType) { buffer_type = bufferType; }

    void flushDrmBuf();
    void invalidateDrmBuf();
};

}  // namespace FFMedia
