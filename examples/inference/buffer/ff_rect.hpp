#pragma once

namespace FFMedia
{

class FFRect
{
public:
    FFRect() = default;
    FFRect(int x, int y, int width, int height);
    ~FFRect();

public:
    int x;
    int y;
    int width;
    int height;
};


}  // namespace FFMedia