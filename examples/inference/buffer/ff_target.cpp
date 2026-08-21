#include "ff_target.hpp"

namespace FFMedia
{

FFTarget::FFTarget(int x,
                   int y,
                   int width,
                   int height,
                   int class_id,
                   float score,
                   std::string label)
    : x(x),
      y(y),
      width(width),
      height(height),
      class_id(class_id),
      score(score),
      label(label)
{
}

FFTarget::~FFTarget()
{
}

FFRect FFTarget::getRect() const
{
    return FFRect(x, y, width, height);
}

}  // namespace FFMedia