#pragma once

#include <memory>
#include <vector>
#include "ff_rect.hpp"

namespace FFMedia
{

struct FFPoseKeypoint {
    int x;
    int y;
    float score;
};

class FFPoseTarget
{
public:
    FFPoseTarget(int x,
                 int y,
                 int width,
                 int height,
                 int class_id,
                 float score);
    ~FFPoseTarget();
    FFRect getRect() const;

public:
    int x;
    int y;
    int width;
    int height;
    int class_id;
    float score;
    std::vector<FFPoseKeypoint> keypoints;
    int track_id = -1;
    std::vector<FFRect> tracks;
};

}  // namespace FFMedia
