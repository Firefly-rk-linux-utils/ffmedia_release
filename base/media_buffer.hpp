#ifndef __MEDIA_BUFFER_HPP__
#define __MEDIA_BUFFER_HPP__

#include <inttypes.h>
#include <atomic>
#include <mutex>
#include "ff_type.hpp"

using namespace std;
class MediaBuffer
{
protected:
    uint16_t index;
    void* data;
    size_t size;
    void* active_data;
    size_t active_size;
    int64_t ustimestamp;
    int64_t duration;
    bool eos;
    void* private_data;
    void* extra_data;
    int medai_type;
    std::atomic_bool status;
    std::atomic_uint16_t ref_count;
    mutex mtx;

public:
    MediaBuffer();
    virtual ~MediaBuffer();
    virtual void allocBuffer(size_t _size);
    virtual void fillToEmpty();

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

    int64_t getUstimestamp() const { return ustimestamp; }
    void setUstimestamp(const int64_t& ustimestamp_) { ustimestamp = ustimestamp_; }

    int64_t getDuration() { return duration; }
    void setDuration(int64_t _duration) { duration = _duration; }

    void* getPrivateData() const { return private_data; }
    void setPrivateData(void* privateData) { private_data = privateData; }

    void* getExtraData() const { return extra_data; }
    void setExtraData(void* extraData) { extra_data = extraData; }

    bool getEos() const { return eos; }
    void setEos(const bool& eos_) { eos = eos_; }

    bool getStatus();
    void setStatus(bool _status);

    uint16_t increaseRefCount();
    uint16_t decreaseRefCount();
    uint16_t getRefCount();
    void setRefCount(uint16_t refCount);
    int getMediaBufferType() { return medai_type; }
    void setMediaBufferType(int _medai_type) { medai_type = _medai_type; }
};

#endif
