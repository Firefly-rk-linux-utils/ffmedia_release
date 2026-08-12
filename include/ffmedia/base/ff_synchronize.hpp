#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "ff_type.hpp"

namespace FFMedia
{

enum SynchronizeType : int32_t {
    SYNCHRONIZETYPE_VIDEO,
    SYNCHRONIZETYPE_AUDIO,
    SYNCHRONIZETYPE_ABSOLUTE
};

/**
 * Audio/video presentation clock.
 *
 * All timestamps and delays use microseconds. updateVideo() returns a positive
 * delay, zero for immediate presentation, or a negative value when the frame
 * is late enough to drop. updateAudio() returns the number of PCM sample
 * frames that should be played from the current buffer.
 */
class FFMEDIA_API Synchronize
{
public:
    struct Clock {
        int64_t start_time;
        int64_t current_pts;
    };

    using TimeSource = std::function<int64_t()>;

    explicit Synchronize(SynchronizeType type);
    ~Synchronize();

    int64_t getCurrentTime();
    void reset();

    Clock& getMasterClock();
    int64_t getMasterTime();
    void setClockTime(SynchronizeType type, int64_t pts);
    int64_t getClockTime(const Clock& clock);

    void setMasterType(SynchronizeType type);
    SynchronizeType getMasterType() const;

    /** Maximum returned video delay and the value returned for a late frame. */
    void setRefrshS(int maxUs, int minUs);
    void setFirstFrameDuration(int durationUs);

    /** Timestamp/wall-clock gaps larger than this start a new timeline. */
    void setDiscontinuityThreshold(int64_t thresholdUs);

    /**
     * Configure gradual PCM correction.
     * gain and maxRatio are in [0, 1]; deadbandUs suppresses tiny corrections.
     */
    void setAudioCorrection(double gain, double maxRatio, int64_t deadbandUs);

    /** Override the monotonic clock, primarily for deterministic simulation. */
    void setTimeSource(TimeSource source);

    int updateVideo(int64_t pts, int64_t duration);
    int updateAudio(int samples, int samplerate, int64_t pts);
    /**
     * Update audio using the playback device clock.
     * delaySamples is the number of sample frames queued ahead of the DAC.
     * clockRunning must only be true after the device has started playback.
     */
    int updateAudioWithDeviceClock(int samples, int samplerate, int64_t pts,
                                   int64_t delaySamples, bool clockRunning);
    int updateAudioByBytesSize(unsigned bytesSize, int samplerate, int channels,
                               int bitsPerSample, int64_t pts);

    int64_t getLastVideoDifference() const;
    int64_t getLastAudioDifference() const;
    uint64_t getVideoDropCount() const;
    uint64_t getDiscontinuityCount() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace FFMedia
