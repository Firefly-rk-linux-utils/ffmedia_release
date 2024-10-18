#ifndef __TYPE_H__
#define __TYPE_H__

/*
 * Decode type support
 */
enum DecodeType {
    DECODE_TYPE_H264 = 0,
    DECODE_TYPE_H265,
    DECODE_TYPE_MJPEG,
    DECODE_TYPE_MAX,
};

/*
 * Encode type support
 */
enum EncodeType {
    ENCODE_TYPE_H264 = 0,
    ENCODE_TYPE_H265,
    ENCODE_TYPE_MJPEG,
    ENCODE_TYPE_MAX,
};

enum media_codec_t {
    MEDIA_CODEC_UNKNOWN = 0,
    MEDIA_CODEC_VIDEO_VCM,
    MEDIA_CODEC_VIDEO_MPEG4,
    MEDIA_CODEC_VIDEO_MPEG1,
    MEDIA_CODEC_VIDEO_MPEG2,
    MEDIA_CODEC_VIDEO_H264,
    MEDIA_CODEC_VIDEO_H265,
    MEDIA_CODEC_VIDEO_VP8,
    MEDIA_CODEC_VIDEO_VP9,
    MEDIA_CODEC_VIDEO_AV1,
    MEDIA_CODEC_VIDEO_MJPEG,
    MEDIA_CODEC_VIDEO_H266,

    MEDIA_CODEC_AUDIO_MP3 = 0x1000,
    MEDIA_CODEC_AUDIO_MP2,
    MEDIA_CODEC_AUDIO_MP1,
    MEDIA_CODEC_AUDIO_PCM_BE,
    MEDIA_CODEC_AUDIO_PCM_LE,
    MEDIA_CODEC_AUDIO_PCM_FLOAT,
    MEDIA_CODEC_AUDIO_MPC,
    MEDIA_CODEC_AUDIO_AC3,
    MEDIA_CODEC_AUDIO_ACM,
    MEDIA_CODEC_AUDIO_AAC,
    MEDIA_CODEC_AUDIO_PCM_A,
    MEDIA_CODEC_AUDIO_PCM_U,

    MEDIA_CODEC_SUBTITLE_TEXT = 0x2000,
    MEDIA_CODEC_SUBTITLE_SSA,
    MEDIA_CODEC_SUBTITLE_ASS,
    MEDIA_CODEC_SUBTITLE_USF,
};

/*
 * RcMode - rate control mode
 * 0 - cbr mode, Constant bit rate
 * 1 - vbr mode, variable bit rate
 */
enum EncodeRcMode {
    ENCODE_RC_MODE_CBR = 0,
    ENCODE_RC_MODE_VBR,
    ENCODE_RC_MODE_FIXQP,
    ENCODE_RC_MODE_AVBR,
};

/*
 * H.264 profile_idc parameter
 * 66  - Baseline profile
 * 77  - Main profile
 * 100 - High profile
 */
enum EncodeProfile {
    ENCODE_PROFILE_BASELINE = 0,
    ENCODE_PROFILE_MAIN,
    ENCODE_PROFILE_HIGH,
};

/*
 * Quality - quality parameter
 * mpp does not give the direct parameter in different protocol.
 * mpp provide total 5 quality level 1 ~ 5
 * 0 - worst
 * 1 - worse
 * 2 - medium
 * 3 - better
 * 4 - best
 */
enum EncodeQuality {
    ENCODE_QUALITY_WORST = 0,
    ENCODE_QUALITY_WORSE,
    ENCODE_QUALITY_MEDIUM,
    ENCODE_QUALITY_BETTER,
    ENCODE_QUALITY_BEST,
};

enum RgaRotate {
    RGA_ROTATE_NONE = 0,
    RGA_ROTATE_90,
    RGA_ROTATE_180,
    RGA_ROTATE_270,
    RGA_ROTATE_VFLIP,  // Vertical Mirror
    RGA_ROTATE_HFLIP,  // Horizontal Mirror
};

enum yuv2RgbMode {
    RGB_TO_RGB = 0,
    YUV_TO_YUV = 0,
    YUV_TO_RGB = 0x1 << 0,
    RGB_TO_YUV = 0x2 << 4,
};

enum SampleFormat {
    SAMPLE_FMT_NONE = -1,
    SAMPLE_FMT_U8,
    SAMPLE_FMT_S16,
    SAMPLE_FMT_S32,
    SAMPLE_FMT_FLT,
    SAMPLE_FMT_U8P,
    SAMPLE_FMT_S16P,
    SAMPLE_FMT_S32P,
    SAMPLE_FMT_FLTP,
    SAMPLE_FMT_G711A,
    SAMPLE_FMT_G711U,
    SAMPLE_FMT_NB
};

enum AI_LAYOUT_E {
    AI_LAYOUT_NORMAL = 0,    /* Normal      */
    AI_LAYOUT_MIC_REF,       /* MIC + REF, do clear ref*/
    AI_LAYOUT_REF_MIC,       /* REF + MIC, do clear ref*/
    AI_LAYOUT_2MIC_REF_NONE, /* MIC0 + MIC1 + REF0 + NONE, do clear ref*/
    AI_LAYOUT_2MIC_NONE_REF, /* MIC0 + MIC1 + NONE + REF1, do clear ref*/
    AI_LAYOUT_2MIC_2REF,     /* MIC0 + MIC1 + REF0 + REF1, do clear ref*/
    AI_LAYOUT_BUTT
};

struct SampleInfo {
    SampleFormat fmt;
    int channels;
    int sample_rate;
    int nb_samples;
    SampleInfo()
        : fmt(SAMPLE_FMT_NONE), channels(0), sample_rate(0), nb_samples(0){};
};

enum MEDIA_BUFFER_TYPE {
    BUFFER_TYPE_VIDEO,
    BUFFER_TYPE_AUDIO,
    BUFFER_TYPE_ETC
};

enum SynchronizeType {
    SYNCHRONIZETYPE_VIDEO,
    SYNCHRONIZETYPE_AUDIO,
    SYNCHRONIZETYPE_ABSOLUTE
};

enum RTSP_STREAM_TYPE {
    RTSP_STREAM_TYPE_UDP,
    RTSP_STREAM_TYPE_TCP,
    RTSP_STREAM_TYPE_MULTICAST
};

enum SIP_TRANSPORT_TYPE {
    TRANSPORT_TYPE_TCP,
    TRANSPORT_TYPE_UDP,
    TRANSPORT_TYPE_TLS,
    TRANSPORT_TYPE_DTLS
};

#endif
