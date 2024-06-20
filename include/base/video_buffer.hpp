#ifndef __VIDEO_BUFFER_HPP__
#define __VIDEO_BUFFER_HPP__

#include <inttypes.h>
#include "pixel_fmt.hpp"
#include "media_buffer.hpp"

class DrmBuffer;
typedef void* MppBuffer;
typedef void* MppBufferGroup;

class VideoBuffer : public MediaBuffer
{
public:
    enum BUFFER_TYPE {
        DRM_BUFFER_NONCACHEABLE,
        DRM_BUFFER_CACHEABLE,
        MALLOC_BUFFER,
        EXTERNAL_BUFFER
    };

private:
    DrmBuffer* drm_buf;
    MppBuffer mpp_buf;
    BUFFER_TYPE buffer_type;
    int buf_fd;

public:
    VideoBuffer(BUFFER_TYPE type);
    ~VideoBuffer();
    void resetBuffer();
    void allocBuffer(ImagePara para);
    void allocBuffer(size_t _size);
    void fillWithBlack();
    void fillWithBlack(uint32_t x, uint32_t y, uint32_t w, uint32_t h);
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

#endif
