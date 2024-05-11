#ifndef __FF_SYNCHRONIZE_HPP__
#define __FF_SYNCHRONIZE_HPP__

#include <sys/time.h>
#include <inttypes.h>
#include <stdlib.h>

#include "ff_type.hpp"

class Synchronize
{
public:
    struct Clock {
        int64_t start_time;
        int64_t current_pts;
    };

private:
    Clock audio;
    Clock video;
    Clock absolute;
    float ptsRatio;
    SynchronizeType type;

    int video_last_duration;
    int audio_last_duration;

    int min_refrsh_s;  // Video minimum delay interval，default 10 000us.
    int max_refrsh_s;  // Video maximum delay interval，default 100 000us

    /*
        For playing network video streams, caching more than 2
        frames before playing will have better smoothness.
    */
    int first_frame_duration;  // default 0us
public:
    explicit Synchronize(SynchronizeType _type);
    int64_t getCurrentTime();
    void reset();
    void setPtsRatio(float ratio);
    Clock& getMasterClock();
    int64_t getMasterTime();
    void setClockTime(SynchronizeType _type, int64_t pts);
    int64_t getClockTime(const Clock& clock);
    void setRefrshS(int maxUs, int minUs);
    void setFirstFrameDuration(int durationUs);
    int updateVideo(int64_t pts, int64_t duration);
    int updateAudio(int samples, int samplerate, int64_t pts);
    int updateAudioByBytesSize(unsigned bytesSize, int samplerate, int channels,
                               int bitsPerSample, int64_t pts);
};


#endif
