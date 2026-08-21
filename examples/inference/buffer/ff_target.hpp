#pragma once

#include <string>
#include <memory>
#include <vector>
#include "ff_rect.hpp"

namespace FFMedia
{

class FFTarget
{
public:
    FFTarget(int x,
             int y,
             int width,
             int height,
             int class_id,
             float score,
             std::string label = "");
    ~FFTarget();
    FFRect getRect() const;

public:
    int x;
    int y;
    int width;
    int height;
    int class_id;
    float score;
    std::string label;
    int track_id = -1;
    std::vector<FFRect> tracks;
};

}  // namespace FFMedia
