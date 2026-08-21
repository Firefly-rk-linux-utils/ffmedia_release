#pragma once

#include "Rect.h"

namespace byte_track
{
struct Object {
    Rect<float> rect;
    size_t label;
    float prob;

    Object(const Rect<float>& _rect,
           const size_t& _label,
           const float& _prob);
};
}  // namespace byte_track