#include "module_byte_track.hpp"

namespace FFMedia
{

ModuleByteTrack::ModuleByteTrack(const std::string& module_name, ModuleTrackFor track_for)
    : ModuleTrack(module_name, track_for), tracker(30, 60)
{
}

ModuleByteTrack::~ModuleByteTrack()
{
}

void ModuleByteTrack::track(std::shared_ptr<InferBuffer>& output_buffer)
{
    auto process_targets = [&](auto& targets) {
        std::vector<byte_track::Object> objects;
        objects.reserve(targets.size());

        for (size_t i = 0; i < targets.size(); ++i) {
            const auto& t = targets[i];
            objects.emplace_back(byte_track::Rect<float>(t.x, t.y, t.width, t.height), i, t.score);
        }

        auto stracks = tracker.update(objects);

        for (const auto& strack : stracks) {
            auto& target = targets[strack->getLabel()];
            target.track_id = strack->getTrackId();
            auto& info = track_data[target.track_id];
            info.tracks.push_back(target.getRect());
            info.pts = output_buffer->getPUstimestamp();
            target.tracks = info.tracks;
        }
    };

    switch (track_for) {
        case ModuleTrackFor::NORMAL:
            process_targets(output_buffer->targets);
            break;
        case ModuleTrackFor::FACE:
            process_targets(output_buffer->face_targets);
            break;
        case ModuleTrackFor::POSE:
            process_targets(output_buffer->pose_targets);
            break;
        default:
            ff_error_m("Unsupported track_for type: %d\n", static_cast<int>(track_for));
            break;
    }

    auto cur_pts = output_buffer->getPUstimestamp();
    for (auto it = track_data.begin(); it != track_data.end();) {
        int64_t diff = cur_pts - it->second.pts;
        if (diff > max_allowed_disappear_frames_duration || diff < 0) {
            it = track_data.erase(it);
        } else {
            ++it;
        }
    }
}


}  // namespace FFMedia