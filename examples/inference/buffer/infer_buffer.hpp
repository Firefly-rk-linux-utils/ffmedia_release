#pragma once

#include <vector>
#include "base/video_buffer.hpp"
#include "ff_target.hpp"
#include "ff_face_target.hpp"
#include "ff_pose_target.hpp"

namespace FFMedia
{

class InferBuffer : public VideoBuffer
{
public:
    InferBuffer(VideoBuffer::BUFFER_TYPE type);
    InferBuffer(const InferBuffer& other);
    ~InferBuffer() override;
    std::shared_ptr<MediaBuffer> clone() const override;

    /** Copy base frame metadata and inference results without image payload. */
    void copyMetadata(const MediaBuffer& other) override;

public:
    std::shared_ptr<MediaBuffer> private_buffer;

    std::string description;
    std::vector<FFTarget> targets;
    std::vector<FFFaceTarget> face_targets;
    std::vector<FFPoseTarget> pose_targets;
};

}  // namespace FFMedia
