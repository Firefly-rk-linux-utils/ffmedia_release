#ifndef __UTILS_HPP__
#define __UTILS_HPP__

#include <stdlib.h>
#include <stdio.h>

#include "base/video_buffer.hpp"

void dump_normalbuffer_to_file(shared_ptr<VideoBuffer> buffer, FILE* fp);
void dump_videobuffer_to_file(shared_ptr<VideoBuffer> buffer, FILE* fp);
#endif
