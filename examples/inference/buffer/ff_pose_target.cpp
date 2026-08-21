#include "ff_pose_target.hpp"

namespace FFMedia
{

FFPoseTarget::FFPoseTarget(int x,
                           int y,
                           int width,
                           int height,
                           int class_id,
                           float score)
    : x(x),
      y(y),
      width(width),
      height(height),
      class_id(class_id),
      score(score)
{
}

FFPoseTarget::~FFPoseTarget()
{
}

FFRect FFPoseTarget::getRect() const
{
    return FFRect(x, y, width, height);
}

}  // namespace FFMedia