#include "ff_rect.hpp"

namespace FFMedia
{

FFRect::FFRect(int x, int y, int width, int height)
    : x(x), y(y), width(width), height(height)
{
}

FFRect::~FFRect()
{
}

}  // namespace FFMedia