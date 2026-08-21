#include "infer_buffer.hpp"
#include <cstring>

namespace FFMedia
{

InferBuffer::InferBuffer(VideoBuffer::BUFFER_TYPE type)
    : VideoBuffer(type)
{
}

InferBuffer::InferBuffer(const InferBuffer& other)
    : VideoBuffer(other)
{
    copyMetadata(other);
}

InferBuffer::~InferBuffer()
{
}

void InferBuffer::copyMetadata(const MediaBuffer& other)
{
    VideoBuffer::copyMetadata(other);
    const InferBuffer* infer = dynamic_cast<const InferBuffer*>(&other);
    if (!infer) {
        description.clear();
        targets.clear();
        face_targets.clear();
        pose_targets.clear();
        return;
    }
    description = infer->description;
    targets = infer->targets;
    face_targets = infer->face_targets;
    pose_targets = infer->pose_targets;
}

std::shared_ptr<MediaBuffer> InferBuffer::clone() const
{
    auto destination = std::make_shared<InferBuffer>(*this);

    if (media_type == BUFFER_TYPE_VIDEO && mediaPara.v.v4l2Fmt && mediaPara.v.width && mediaPara.v.height) {
        destination->allocBuffer(mediaPara.v);
    } else {
        destination->allocBuffer(active_size);
    }

    if (destination->getSize() < active_size)
        return nullptr;

    memcpy(destination->getData(), active_data, active_size);
    return destination;
}

}  // namespace FFMedia
