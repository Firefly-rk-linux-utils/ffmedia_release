#pragma once

#include <vector>
#include <memory>
#include "ff_rect.hpp"

namespace FFMedia
{

class FFFaceTarget
{
public:
    FFFaceTarget(int x,
                 int y,
                 int width,
                 int height,
                 float score,
                 int cls,
                 std::vector<std::pair<int, int>> key_points = std::vector<std::pair<int, int>>());
    ~FFFaceTarget();
    FFRect getRect() const;

public:
    int x;
    int y;
    int width;
    int height;
    float score;
    int cls;
    std::vector<std::pair<int, int>> key_points;
    int track_id = -1;
    std::vector<FFRect> tracks;
};
}  // namespace FFMedia
