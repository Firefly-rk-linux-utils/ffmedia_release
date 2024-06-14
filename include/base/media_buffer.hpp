#ifndef __MEDIA_BUFFER_HPP__
#define __MEDIA_BUFFER_HPP__

#include <inttypes.h>
#include <atomic>
#include <mutex>
#include <memory>
#include "ff_type.hpp"
#include "pixel_fmt.hpp"

using namespace std;
class MediaBuffer
{
protected:
    uint16_t index;
    void* data;
    size_t size;
    void* active_data;
    size_t active_size;
    int64_t p_ustimestamp;
    int64_t d_ustimestamp;
    bool eos;
    void* private_data;
    shared_ptr<MediaBuffer> extra_data;
    MEDIA_BUFFER_TYPE media_type;
    std::atomic_bool status;
    std::atomic_uint16_t ref_count;
    mutex mtx;

    union {
        SampleInfo a;
        ImagePara v;
    } mediaPara;

public:
    MediaBuffer(size_t _size = 0);
    virtual ~MediaBuffer();
    virtual void allocBuffer(size_t _size);
    virtual void fillWithBlack();

public:
    static const bool STATUS_CLEAN = true;
    static const bool STATUS_DIRTY = false;

public:
    uint16_t getIndex() const { return index; }
    void setIndex(const uint16_t& index_) { index = index_; }

    void* getData() const { return data; }
    void setData(void* data_) { data = data_; }

    size_t getSize() const { return size; }
    void setSize(const size_t& size_) { size = size_; }

    void* getActiveData() const { return active_data; }
    void setActiveData(void* activeData) { active_data = activeData; }

    size_t getActiveSize() const { return active_size; }
    void setActiveSize(const size_t& activeSize) { active_size = activeSize; }

    int64_t getPUstimestamp() const { return p_ustimestamp; }
    void setPUstimestamp(const int64_t& ustimestamp_) { p_ustimestamp = ustimestamp_; }

    int64_t getDUstimestamp() const { return d_ustimestamp; }
    void setDUstimestamp(const int64_t& ustimestamp_) { d_ustimestamp = ustimestamp_; }

    void* getPrivateData() const { return private_data; }
    void setPrivateData(void* privateData) { private_data = privateData; }

    shared_ptr<MediaBuffer> getExtraData() const { return extra_data; }
    void setExtraData(shared_ptr<MediaBuffer> extraData) { extra_data = extraData; }

    bool getEos() const { return eos; }
    void setEos(const bool& eos_) { eos = eos_; }

    bool getStatus();
    void setStatus(bool _status);

    uint16_t increaseRefCount();
    uint16_t decreaseRefCount();
    uint16_t getRefCount();
    void setRefCount(uint16_t refCount);
    MEDIA_BUFFER_TYPE getMediaBufferType() { return media_type; }
    void setMediaBufferType(MEDIA_BUFFER_TYPE _media_type) { media_type = _media_type; }

    ImagePara getImagePara() const { return mediaPara.v; }
    void setImagePara(const ImagePara& para) { mediaPara.v = para; }
    SampleInfo getSamplePara() const { return mediaPara.a; }
    void setSamplePara(const SampleInfo& para) { mediaPara.a = para; }
};

#endif
