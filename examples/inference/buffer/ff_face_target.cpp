#include "ff_face_target.hpp"

namespace FFMedia
{

FFFaceTarget::FFFaceTarget(int x,
                           int y,
                           int width,
                           int height,
                           float score,
                           int cls,
                           std::vector<std::pair<int, int>> key_points)
    : x(x),
      y(y),
      width(width),
      height(height),
      score(score),
      cls(cls),
      key_points(key_points)
{
}

FFFaceTarget::~FFFaceTarget()
{
}

FFRect FFFaceTarget::getRect() const
{
    return FFRect(x, y, width, height);
}

}  // namespace FFMedia
