#pragma once

#include "module_track.hpp"
#include <unordered_map>
#include "bytetrack/BYTETracker.h"

namespace FFMedia
{

class ModuleByteTrack : public ModuleTrack
{
public:
    ModuleByteTrack(const std::string& module_name, ModuleTrackFor track_for = ModuleTrackFor::NORMAL);
    ~ModuleByteTrack();

protected:
    virtual void track(std::shared_ptr<InferBuffer>& output_buffer) override;

private:
    struct TrackInfo {
        std::vector<FFRect> tracks;
        uint64_t pts;
    };

    byte_track::BYTETracker tracker;
    std::unordered_map<int, TrackInfo> track_data;
    const int64_t max_allowed_disappear_frames_duration = 5 * 1000 * 1000;  // 5 seconds
};


}  // namespace FFMedia
