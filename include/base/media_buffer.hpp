#pragma once


#include <atomic>
#include <mutex>
#include <memory>
#include <functional>
#include "ff_type.hpp"
#include "pixel_fmt.hpp"

namespace FFMedia
{

enum MEDIA_BUFFER_TYPE {
    BUFFER_TYPE_VIDEO,
    BUFFER_TYPE_AUDIO,
    BUFFER_TYPE_ETC
};

class FFMEDIA_API MediaBuffer
{
public:
    using onRefZeroCB = std::function<void(const std::shared_ptr<MediaBuffer>&)>;

    MediaBuffer(size_t _size = 0);
    MediaBuffer(const MediaBuffer& other);
    MediaBuffer& operator=(const MediaBuffer&) = delete;
    virtual ~MediaBuffer();
    virtual void allocBuffer(size_t _size);
    virtual void fillWithBlack();

    /**
     * Create a deep copy through the dynamic type's copy constructor.
     * Derived classes should override this and return
     * std::make_shared<Derived>(*this).
     */
    virtual std::shared_ptr<MediaBuffer> clone() const;

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

    std::shared_ptr<MediaBuffer> getExtraData() const { return extra_data; }
    void setExtraData(std::shared_ptr<MediaBuffer> extraData) { extra_data = extraData; }

    bool getEos() const { return eos; }
    void setEos(const bool& eos_) { eos = eos_; }

    int getFlags() const { return flags; }
    void setFlags(const int& flags_) { flags = flags_; }

    bool getStatus();
    void setStatus(bool _status);

    uint16_t increaseRefCount();
    uint16_t decreaseRefCount();
    uint16_t getRefCount();
    void setRefCount(uint16_t refCount);
    void setOnRefZeroCallback(void* owner, onRefZeroCB cb);
    void onRefZero(const std::shared_ptr<MediaBuffer>&);

    MEDIA_BUFFER_TYPE getMediaBufferType() { return media_type; }
    void setMediaBufferType(MEDIA_BUFFER_TYPE _media_type) { media_type = _media_type; }

    const ImagePara& getImagePara() const { return mediaPara.v; }
    void setImagePara(const ImagePara& para) { mediaPara.v = para; }
    const SampleInfo& getSamplePara() const { return mediaPara.a; }
    void setSamplePara(const SampleInfo& para) { mediaPara.a = para; }
    media_codec_t getMediaCodec() const { return media_codec; }
    void setMediaCodec(media_codec_t codec) { media_codec = codec; }

    /** Producer-local output channel carrying this buffer. */
    uint32_t getMediaChannelId() const { return media_channel_id; }
    void setMediaChannelId(uint32_t channel_id) { media_channel_id = channel_id; }

protected:
    uint16_t index;
    void* data;
    size_t size;
    void* active_data;
    size_t active_size;
    int64_t p_ustimestamp;
    int64_t d_ustimestamp;
    bool eos;
    int flags;
    void* private_data;
    std::shared_ptr<MediaBuffer> extra_data;
    MEDIA_BUFFER_TYPE media_type;
    bool status;
    std::atomic_uint16_t ref_count;
    std::mutex mtx;

    union {
        SampleInfo a;
        ImagePara v;
    } mediaPara;
    media_codec_t media_codec;
    uint32_t media_channel_id;

    void* owner;
    onRefZeroCB on_ref_zero_cb;
};

}  // namespace FFMedia
