#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <getopt.h>
#include <queue>
#include <math.h>
#include <termios.h>

#include "utils.hpp"
#include "module/vi/module_cam.hpp"
#include "module/vi/module_rtspClient.hpp"
#include "module/vi/module_rtmpClient.hpp"
#include "module/vi/module_fileReader.hpp"
#include "module/vp/module_rga.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vp/module_mppenc.hpp"
#include "module/vo/module_fileWriter.hpp"
#include "module/vo/module_drmDisplay.hpp"
#include "module/vo/module_rtspServer.hpp"
#include "module/vo/module_rtmpServer.hpp"
#include "module/vo/module_gb28181Client.hpp"

#if OPENGL_SUPPORT
#include "module/vo/module_rendererVideo.hpp"
#endif

#if AUDIO_SUPPORT
#include "module/vp/module_aacdec.hpp"
#include "module/vp/module_aacenc.hpp"
#include "module/vi/module_alsaCapture.hpp"
#include "module/vo/module_alsaPlayBack.hpp"
#endif

#if FFMPEG_SUPPORT
#include "module/vi/module_ffmpegDemux.hpp"
#endif

struct timeval curr_time;
struct timeval start_time;

unsigned long curr, start;
using namespace std;

#define USE_COMMON_SOURCE false
shared_ptr<ModuleMedia> common_source_module;

typedef struct _demo_config {
    int drm_display_plane_id = 0;
    int drm_display_conn_id = 0;
    int drm_display_plane_zpos = 0xFF;
    char dump_filename[256] = "";
    char output_filename[256] = "";
#if FFMPEG_SUPPORT
    char ffmpeg_informat[64] = "";
#endif
    char aplay[64] = "";
    char arecord[64] = "";
    char input_source[256] = "";
    char rtmp_url[256] = "";
    char gb28181_user_id[64] = "";
    char gb28181_server_id[64] = "";
    char gb28181_server_ip[64] = "";
    int gb28181_server_port = 5060;
    RgaRotate rotate = RGA_ROTATE_NONE;
    EncodeType encode_type = ENCODE_TYPE_H264;
    ImagePara input_image_para = {0, 0, 0, 0, 0};
    ImagePara output_image_para = {0, 0, 0, 0, V4L2_PIX_FMT_NV12};
    int push_port = -1;
    int push_type = 0;
    int sync_opt = 0;
    RTSP_STREAM_TYPE rtsp_transport = RTSP_STREAM_TYPE_UDP;
    int instance_count = 1;

    bool cam_enabled = false;
    bool file_r_enabled = false;
    bool ffmpeg_demux_enabled = false;
    bool loop = false;
    bool dec_enabled = false;
    bool rga_enabled = false;
    bool drmdisplay_enabled = false;
    bool x11display_enabled = false;
    bool enc_enabled = false;
    bool file_w_enabled = false;
    bool rtsp_c_enabled = false;
    bool rtmp_c_enabled = false;
    bool push_enabled = false;
    bool savetofile_enabled = false;
    bool audio_enable = false;
    bool gb28181_enable = false;
} DemoConfig;

typedef struct _demo_data {
    DemoConfig config;
    shared_ptr<Synchronize> sync = nullptr;
    shared_ptr<ModuleMedia> last_module = nullptr;
    shared_ptr<ModuleMedia> source_module = nullptr;
    shared_ptr<ModuleMedia> source_audio_module = nullptr;
    shared_ptr<ModuleMedia> last_audio_module = nullptr;
    FILE* file_data = nullptr;

} DemoData;

static int mygetch(void)
{
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

static void usage(char** argv)
{
    ff_info(
        "Usage: %s <Input source> [Options]\n\n"
        "Options:\n"
        "-i, --input                  Input image size\n"
        "-o, --output                 Output image size, default same as input\n"
        "-a, --inputfmt               Input image format, default MJPEG\n"
        "-b, --outputfmt              Output image format, default NV12\n"
#if FFMPEG_SUPPORT
        "--use_ffmpeg_demux           Use ffmpeg demux. e.g. --use_ffmpeg_demux or --use_ffmpeg_demux=kmsgrab\n"
#endif
        "-c, --count                  Instance count, default 1\n"
        "-d, --drmdisplay             Drm display, set display plane, set 0 to auto find plane, default disabled\n"
        "    --connector              Set drm display connector, default 0 to auto find connector\n"
#if OPENGL_SUPPORT
        "-x, --x11                    X11 window displays, render the video using opengl. default disabled\n"
#endif
        "-z, --zpos                   Drm display plane zpos, default auto select\n"
        "-e, --encodetype             Encode encode, set encode type, default disabled\n"
        "-f, --file                   Enable save source output data to file, set filename, default disabled\n"
        "-p, --port                   Enable push stream, default rtsp stream, set push port, depend on encode enabled, default disabled\n"
        "    --push_type              Set push stream type, default rtsp. e.g. --push_type rtmp\n"
        "    --rtmp_url               Set the rtmp client push address. e.g. --rtmp_url rtmp://xxx\n"
        "--rtsp_transport             Set the rtsp transport type, default udp.\n"
        "                               e.g. --rtsp_transport tcp | --rtsp_transport multicast\n"
        "-m, --enmux                  Enable save encode data to file. Enable package as mp4, mkv, flv, ts, ps or raw stream files, muxer type depends on the filename suffix.\n"
        "                               default disabled. e.g. -m out.mp4 | -m out.mkv | -m out.yuv\n"
        "-s, --sync                   Enable synchronization module, default disabled. Enable the default audio.\n"
        "                               e.g. -s | --sync=video | --sync=abs\n"
        "--audio                      Enable audio, default disabled.\n"
        "--aplay                      Enable play audio, default disabled. e.g. --aplay plughw:3,0\n"
        "--arecord                    Enable record audio, default disabled. e.g. --arecord plughw:3,0\n"
        "-l, --loop                   Loop reads the media file.\n"
        "--gb28181_user_id            Enable gb28181 client, default disabled. set user id\n"
        "--gb28181_server_id          Set the server id of gb28181 client\n"
        "--gb28181_server_ip          Set the server ip of gb28181 client\n"
        "--gb28181_server_port        Set the server port of gb28181 client\n"
        "-r, --rotate                 Image rotation degree, default 0\n"
        "                               0:   none\n"
        "                               1:   vertical mirror\n"
        "                               2:   horizontal mirror\n"
        "                               90:  90 degree\n"
        "                               180: 180 degree\n"
        "                               270: 270 degree\n"
        "\n",
        argv[0]);
}

static const char* short_options = "i:o:a:b:c:d:z:e:f:p:m:r:s::xl";

// clang-format off
static struct option long_options[] = {
    {"input", required_argument, NULL, 'i'},
    {"output", required_argument, NULL, 'o'},
    {"inputfmt", required_argument, NULL, 'a'},
    {"outputfmt", required_argument, NULL, 'b'},
    {"count", required_argument, NULL, 'c'},
    {"drmdisplay", required_argument, NULL, 'd'},
    {"connector", required_argument, NULL, 'C'},
    {"zpos", required_argument, NULL, 'z'},
    {"encodetype", required_argument, NULL, 'e'},
    {"file", required_argument, NULL, 'f'},
    {"port",  required_argument, NULL, 'p'},
    {"enmux",  required_argument, NULL, 'm'},
    {"rotate", required_argument, NULL, 'r'},
    {"audio", no_argument, NULL, 'A'},
    {"aplay", required_argument, NULL, 'A'},
    {"arecord", required_argument, NULL, 'A'},
    {"sync", optional_argument, NULL, 's' },
    {"rtsp_transport", required_argument, NULL, 'P'},
    {"push_type", required_argument, NULL, 't'},
    {"rtmp_url", required_argument, NULL, 'R'},
#if OPENGL_SUPPORT
    {"x11", no_argument, NULL, 'x'},
#endif
#if FFMPEG_SUPPORT
    {"use_ffmpeg_demux", optional_argument, NULL, 'F'},
#endif
    {"loop", no_argument, NULL, 'l'},
    {"gb28181_user_id", required_argument, NULL, 'G'},
    {"gb28181_server_id", required_argument, NULL, 'G'},
    {"gb28181_server_ip", required_argument, NULL, 'G'},
    {"gb28181_server_port", required_argument, NULL, 'G'},
    {NULL, 0, NULL, 0}
};
// clang-format on

static int parse_encode_parameters(char* str, EncodeType* encode_type)
{
    if (strstr(str, "264") != NULL) {
        *encode_type = ENCODE_TYPE_H264;
    } else if (strstr(str, "265") != NULL) {
        *encode_type = ENCODE_TYPE_H265;
    } else if (strstr(str, "jpeg") != NULL) {
        *encode_type = ENCODE_TYPE_MJPEG;
    } else {
        ff_error("Encode Type %s is not Support\n", str);
        exit(-1);
    }

    return 0;
}

static int parse_format_parameters(char* str, ImagePara* para)
{
    uint32_t format = v4l2GetFmtByName(str);
    if (format != 0) {
        para->v4l2Fmt = format;
    } else {
        ff_error("Format %s is not Support\n", str);
        exit(-1);
    }

    return 0;
}

static int parse_size_parameters(char* str, ImagePara* para)
{
    char *p, *buf;
    const char* delims = "x";
    uint32_t v[2] = {0, 0};
    int i = 0;

    if (strstr(str, delims) == NULL) {
        ff_error("set size format like 640x480 \n");
        exit(-1);
    }

    buf = strdup(str);
    p = strtok(buf, delims);
    while (p != NULL) {
        v[i++] = atoi(p);
        p = strtok(NULL, delims);

        if (i >= 2)
            break;
    }

    para->width = v[0];
    para->height = v[1];
    para->hstride = para->width;
    para->vstride = para->height;
    return 0;
}

void callback_savetofile(void* ctx, shared_ptr<MediaBuffer> buffer)
{
    DemoData* demo = (DemoData*)ctx;
    void* data;
    size_t size;
    if (buffer == NULL)
        return;
    data = buffer->getActiveData();
    size = buffer->getActiveSize();
    if (demo->file_data)
        fwrite(data, size, 1, demo->file_data);
}

void callback_dumpFrametofile(void* ctx, shared_ptr<MediaBuffer> buffer)
{
    DemoData* demo = (DemoData*)ctx;
    if (buffer == NULL)
        return;
    shared_ptr<VideoBuffer> buf = dynamic_pointer_cast<VideoBuffer>(buffer);

    if (demo->file_data) {
        if (buffer->getMediaBufferType() != BUFFER_TYPE_VIDEO || v4l2fmtIsCompressed(buf->getImagePara().v4l2Fmt))
            dump_normalbuffer_to_file(buf, demo->file_data);
        else
            dump_videobuffer_to_file(buf, demo->file_data);
    }
}

void add_index_to_filename(char* filename, int index)
{
    char suffix[5];
    char extension[20];
    sprintf(suffix, "%02d", index);

    if ((filename == nullptr) || (strlen(filename) == 0))
        return;
    char* t = strstr(filename, ".");
    if (t != NULL) {
        int len = t - filename;
        strncpy(extension, t, sizeof(extension) - 1);
        sprintf(filename + len, "_%s%s", suffix, extension);
    } else {
        strcat(filename, "_");
        strcat(filename, suffix);
    }
}

int start_instance(DemoData* inst, int inst_index, int inst_count)
{
    int ret;
    ImagePara productor_output_para;
    DemoConfig* inst_conf = &(inst->config);

    if (inst_count > 1) {
        add_index_to_filename(inst_conf->dump_filename, inst_index);
        add_index_to_filename(inst_conf->output_filename, inst_index);
    }

    ff_info("\n\n==========================================\n");
    if ((inst_conf->input_image_para.width > 0) && (inst_conf->input_image_para.height > 0)) {
        inst_conf->input_image_para.hstride = inst_conf->input_image_para.width;
        inst_conf->input_image_para.vstride = inst_conf->input_image_para.height;
    }

    if (strlen(inst_conf->input_source) == 0) {
        ff_error("input source is not set\n");
        exit(1);
    }

    if (strncmp(inst_conf->input_source, "rtsp", strlen("rtsp")) == 0) {
        ff_info("enable rtsp client\n");
        inst_conf->rtsp_c_enabled = true;
    } else if (strncmp(inst_conf->input_source, "rtmp", strlen("rtmp")) == 0) {
        ff_info("enable rtmp client\n");
        inst_conf->rtmp_c_enabled = true;
    } else {
        struct stat st;
        if (!stat(inst_conf->input_source, &st)) {
            switch (st.st_mode & S_IFMT) {
                case S_IFCHR:
                    ff_info("enable v4l2 camera\n");
                    inst_conf->cam_enabled = true;
                    break;
                case S_IFREG:
                    ff_info("enable file reader\n");
                    inst_conf->file_r_enabled = true;
                    break;
                case S_IFBLK:
                case S_IFDIR:
                case S_IFIFO:
                case S_IFLNK:
                case S_IFSOCK:
                default:
                    inst_conf->ffmpeg_demux_enabled = true;
                    break;
            }

            if (strstr(inst_conf->input_source, "mp4")) {
                inst_conf->dec_enabled = true;
            } else if (strstr(inst_conf->input_source, "mkv")) {
                inst_conf->dec_enabled = true;
            }
        } else {
            inst_conf->ffmpeg_demux_enabled = true;
        }
    }

    if (common_source_module != NULL) {
        inst->source_module = common_source_module;
        goto SOURCE_CREATED;
    }

#if FFMPEG_SUPPORT
    if (inst_conf->ffmpeg_demux_enabled) {
        shared_ptr<ModuleFFmpegDemux> ffmpeg_demux = make_shared<ModuleFFmpegDemux>(inst_conf->input_source, inst_conf->loop ? -1 : 1);
        ffmpeg_demux->setOutputImagePara(inst_conf->input_image_para);
        ffmpeg_demux->setInputFormat(inst_conf->ffmpeg_informat);
        ret = ffmpeg_demux->init();
        if (ret < 0) {
            ff_error("Failed to init ffmpeg demux\n");
            goto FAILED;
        }
        inst->last_module = ffmpeg_demux;
        inst->source_module = ffmpeg_demux;
        goto SOURCE_CREATED;
    }
#else
    if (inst_conf->ffmpeg_demux_enabled) {
        ff_error("%s is not support\n", inst_conf->input_source);
        goto FAILED;
    }
#endif

    if (inst_conf->cam_enabled) {
        shared_ptr<ModuleCam> cam = make_shared<ModuleCam>(inst_conf->input_source);
        cam->setOutputImagePara(inst_conf->input_image_para);  // setOutputImage
        cam->setProductor(NULL);
        cam->setBufferCount(8);
        ret = cam->init();
        if (ret < 0) {
            ff_error("camera init failed\n");
            goto FAILED;
        }
        inst->last_module = cam;
    } else if (inst_conf->file_r_enabled) {
        shared_ptr<ModuleFileReader> file_reader = make_shared<ModuleFileReader>(inst_conf->input_source, inst_conf->loop);
        if ((inst_conf->input_image_para.width > 0) || (inst_conf->input_image_para.height > 0)) {
            file_reader->setOutputImagePara(inst_conf->input_image_para);
        }
        file_reader->setProductor(NULL);
        file_reader->setBufferCount(20);
        ret = file_reader->init();
        if (ret < 0) {
            ff_error("file reader init failed\n");
            goto FAILED;
        }
        inst->last_module = file_reader;
    } else if (inst_conf->rtsp_c_enabled) {
        shared_ptr<ModuleRtspClient> rtsp_c = make_shared<ModuleRtspClient>(inst_conf->input_source, inst_conf->rtsp_transport,
                                                                            true, inst_conf->audio_enable);
        rtsp_c->setProductor(NULL);
        rtsp_c->setBufferCount(20);
        ret = rtsp_c->init();
        if (ret < 0) {
            ff_error("rtsp client init failed\n");
            goto FAILED;
        }
        inst->last_module = rtsp_c;
    } else if (inst_conf->rtmp_c_enabled) {
        shared_ptr<ModuleRtmpClient> rtmp_c = make_shared<ModuleRtmpClient>(inst_conf->input_source);
        rtmp_c->setProductor(NULL);
        rtmp_c->setBufferCount(20);
        ret = rtmp_c->init();
        if (ret < 0) {
            ff_error("rtsp client init failed\n");
            goto FAILED;
        }
        inst->last_module = rtmp_c;
    }

    inst->source_module = inst->last_module;
#if USE_COMMON_SOURCE
    common_source_module = inst->last_module;
#endif

SOURCE_CREATED:

    if (inst_conf->sync_opt) {
        inst->sync = make_shared<Synchronize>(SynchronizeType(inst_conf->sync_opt - 1));
        inst->sync->setFirstFrameDuration(30000);
    }

#if AUDIO_SUPPORT
    if (strlen(inst_conf->arecord)) {
        SampleInfo info;
        info.channels = 2;
        info.fmt = SAMPLE_FMT_S16;
        info.nb_samples = 1024;
        info.sample_rate = 48000;
        auto capture = make_shared<ModuleAlsaCapture>(inst_conf->arecord, info);
        ret = capture->init();
        if (ret < 0) {
            ff_error("capture init failed\n");
            goto FAILED;
        }
        inst->source_audio_module = capture;
        inst->last_audio_module = capture;

        auto aac_enc = make_shared<ModuleAacEnc>();
        aac_enc->setProductor(inst->last_audio_module);
        ret = aac_enc->init();
        if (ret < 0) {
            ff_error("aac_enc init failed\n");
            goto FAILED;
        }
        inst->last_audio_module = aac_enc;
    }

    if (strlen(inst_conf->aplay)) {
        shared_ptr<ModuleAacDec> aac_dec = make_shared<ModuleAacDec>();
        aac_dec->setProductor(inst->last_audio_module ? inst->last_audio_module : inst->last_module);
        ret = aac_dec->init();
        if (ret < 0) {
            ff_error("aac_dec init failed\n");
            goto FAILED;
        }

        auto aplay = make_shared<ModuleAlsaPlayBack>(inst_conf->aplay);
        aplay->setProductor(aac_dec);
        aplay->setSynchronize(inst->sync);
        ret = aplay->init();
        if (ret < 0) {
            ff_error("Audio playback init failed\n");
            goto FAILED;
        }
    }
#endif

    if (inst->source_module != nullptr) {
        const ImagePara& source_module_output_para = inst->source_module->getOutputImagePara();
        inst_conf->input_image_para = source_module_output_para;

        if ((inst_conf->output_image_para.width == 0) || (inst_conf->output_image_para.height == 0)) {
            inst_conf->output_image_para.width = source_module_output_para.width;
            inst_conf->output_image_para.height = source_module_output_para.height;
        }

        // hstride and vstride are aligned to 16 bytes
        inst_conf->output_image_para.hstride = ALIGN(inst_conf->output_image_para.width, 16);
        inst_conf->output_image_para.vstride = ALIGN(inst_conf->output_image_para.height, 16);

        //(input_para.v4l2Fmt == V4L2_PIX_FMT_VP8) ||
        //(input_para.v4l2Fmt == V4L2_PIX_FMT_VP9))
        if ((source_module_output_para.v4l2Fmt == V4L2_PIX_FMT_MJPEG)
            || (source_module_output_para.v4l2Fmt == V4L2_PIX_FMT_H264)
            || (source_module_output_para.v4l2Fmt == V4L2_PIX_FMT_HEVC)) {
            inst_conf->dec_enabled = true;
        }
    } else {
        goto FAILED;
    }

    inst->last_module = inst->source_module;

    // inst->dec_enabled = false;
    if (inst_conf->dec_enabled) {
        shared_ptr<ModuleMppDec> dec = make_shared<ModuleMppDec>();
        dec->setProductor(inst->last_module);
        dec->setBufferCount(10);
        ret = dec->init();
        if (ret < 0) {
            ff_error("Dec init failed\n");
            goto FAILED;
        }
        inst->last_module = dec;
    }

    {

        if (inst_conf->rotate != RGA_ROTATE_NONE) {
            inst_conf->rga_enabled = true;
        }

        const ImagePara& input_para = inst->last_module->getOutputImagePara();
        if ((input_para.height != inst_conf->output_image_para.height)
            || (input_para.width != inst_conf->output_image_para.width)
            || (input_para.v4l2Fmt != inst_conf->output_image_para.v4l2Fmt)) {
            inst_conf->rga_enabled = true;
        }

        if ((inst_conf->rotate == RGA_ROTATE_90) || (inst_conf->rotate == RGA_ROTATE_270)) {
            uint32_t t = inst_conf->output_image_para.width;
            inst_conf->output_image_para.width = inst_conf->output_image_para.height;
            inst_conf->output_image_para.height = t;
            t = inst_conf->output_image_para.hstride;
            inst_conf->output_image_para.hstride = inst_conf->output_image_para.vstride;
            inst_conf->output_image_para.vstride = t;
        }
    }

    if (inst_conf->rga_enabled) {
        shared_ptr<ModuleRga> rga = make_shared<ModuleRga>(inst_conf->output_image_para, inst_conf->rotate);
        rga->setProductor(inst->last_module);
        rga->setBufferCount(2);
        ret = rga->init();
        if (ret < 0) {
            ff_error("rga init failed\n");
            goto FAILED;
        }
        inst->last_module = rga;
    }

    if (inst_conf->drmdisplay_enabled) {
        const ImagePara& input_para = inst->last_module->getOutputImagePara();
        shared_ptr<ModuleDrmDisplay> drm_display = make_shared<ModuleDrmDisplay>(input_para);
        drm_display->setPlanePara(V4L2_PIX_FMT_NV12, inst_conf->drm_display_plane_id,
                                  PLANE_TYPE_OVERLAY_OR_PRIMARY, inst_conf->drm_display_plane_zpos,
                                  1, inst_conf->drm_display_conn_id);
        // inst->drm_display->setPlaneSize(0, 0, 1280, 800);
        drm_display->setBufferCount(1);
        drm_display->setProductor(inst->last_module);
        drm_display->setSynchronize(inst->sync);
        ret = drm_display->init();
        if (ret < 0) {
            ff_error("drm display init failed\n");
            goto FAILED;
        }

        uint32_t t_h, t_v;
        drm_display->getPlaneSize(&t_h, &t_v);
        int hc, vc;
        int s = sqrt(inst_count);
        if ((s * s) < inst_count) {
            if ((s * (s + 1)) < inst_count)
                vc = s + 1;
            else
                vc = s;
            hc = s + 1;
        } else {
            hc = vc = s;
        }


        ff_info("t_h t_v %d %d\n", t_h, t_v);
        ff_info("hc vc %d %d\n", hc, vc);
        int h_o = inst_index % hc;
        int v_o = inst_index / hc;
        uint32_t dw = t_h / hc;
        uint32_t dh = t_v / vc;
        ff_info("dw dh %d %d\n", dw, dh);
        ff_info("w h %d %d\n", input_para.width, input_para.height);
        uint32_t w = std::min(dw, input_para.width);
        uint32_t h = std::min(dh, input_para.height);
        uint32_t x = (dw - w) / 2 + h_o * dw;
        uint32_t y = (dh - h) / 2 + v_o * dh;

        ff_info("x y w h %d %d %d %d\n", x, y, w, h);

        drm_display->setWindowRect(x, y, w, h);
    }
#if OPENGL_SUPPORT
    else if (inst_conf->x11display_enabled) {
        shared_ptr<ModuleRendererVideo> x11_display = make_shared<ModuleRendererVideo>(inst_conf->input_source);
        x11_display->setProductor(inst->last_module);
        x11_display->setSynchronize(inst->sync);
        ret = x11_display->init();
        if (ret < 0) {
            ff_error("x11 display init failed\n");
            goto FAILED;
        }
    }
#endif

    if (inst_conf->enc_enabled) {
        shared_ptr<ModuleMppEnc> enc = make_shared<ModuleMppEnc>(inst_conf->encode_type);
        enc->setProductor(inst->last_module);
        enc->setBufferCount(8);
        enc->setDuration(0);  // Use the input source timestamp
        ret = enc->init();
        if (ret < 0) {
            ff_error("Enc init failed\n");
            goto FAILED;
        }
        inst->last_module = enc;
    }

    if (inst_conf->file_w_enabled) {
        shared_ptr<ModuleFileWriter> file_writer = make_shared<ModuleFileWriter>(inst_conf->output_filename);
        file_writer->setProductor(inst->last_module);
        ret = file_writer->init();
        if (ret < 0) {
            ff_error("ModuleFileWriter init failed\n");
            goto FAILED;
        }

        if (inst_conf->audio_enable) {
            auto file_writer_a = make_shared<ModuleFileWriterExtend>(file_writer, string());
            file_writer_a->setProductor(inst->last_audio_module ? inst->last_audio_module : inst->source_module);
            file_writer_a->setAudioParameter(0, 0, 0, MEDIA_CODEC_AUDIO_AAC);
            ret = file_writer_a->init();
            if (ret < 0) {
                ff_error("audio writer init failed\n");
                goto FAILED;
            }
        }
    }

    if (inst_conf->push_enabled) {
        char push_path[256] = "";
        sprintf(push_path, "/live/%d", inst_index);
        if (inst_conf->push_type) {
            shared_ptr<ModuleRtmpServer> rtmp_s = make_shared<ModuleRtmpServer>(push_path,
                                                                                inst_conf->push_port);
            rtmp_s->setProductor(inst->last_module);
            rtmp_s->setBufferCount(0);
            if (inst_conf->sync_opt)
                rtmp_s->setSynchronize(make_shared<Synchronize>(SynchronizeType(inst_conf->sync_opt - 1)));

            ret = rtmp_s->init();
            if (ret) {
                ff_error("rtmp server init failed\n");
                goto FAILED;
            }
        } else {
            shared_ptr<ModuleRtspServer> rtsp_s = make_shared<ModuleRtspServer>(push_path,
                                                                                inst_conf->push_port);
            rtsp_s->setProductor(inst->last_module);
            rtsp_s->setBufferCount(0);
            if (inst_conf->sync_opt)
                rtsp_s->setSynchronize(make_shared<Synchronize>(SynchronizeType(inst_conf->sync_opt - 1)));
            ret = rtsp_s->init();
            if (ret) {
                ff_error("rtsp server init failed\n");
                goto FAILED;
            }
            if (inst_conf->audio_enable) {
                auto rtsp_s_a = make_shared<ModuleRtspServerExtend>(rtsp_s, push_path, inst_conf->push_port);
                rtsp_s_a->setProductor(inst->last_audio_module ? inst->last_audio_module : inst->source_module);
                rtsp_s_a->setAudioParameter(MEDIA_CODEC_AUDIO_AAC);
                ret = rtsp_s_a->init();
                if (ret) {
                    ff_error("rtsp server audio init failed\n");
                    goto FAILED;
                }
            }
        }
        ff_info("\n Start push stream: %s://LocalIpAddr:%d%s\n\n", inst_conf->push_type ? "rtmp" : "rtsp", inst_conf->push_port, push_path);
    }

    if (strlen(inst_conf->rtmp_url) > 0) {
        shared_ptr<ModuleRtmpClient> rtmp_c_push = make_shared<ModuleRtmpClient>(inst_conf->rtmp_url, ImagePara(), 0);
        rtmp_c_push->setProductor(inst->last_module);
        if (inst_conf->sync_opt)
            rtmp_c_push->setSynchronize(make_shared<Synchronize>(SynchronizeType(inst_conf->sync_opt - 1)));

        ret = rtmp_c_push->init();
        if (ret) {
            ff_error("Fail to init rtmp client push\n");
            goto FAILED;
        }
        ff_info("Rtmp client start push stream: %s\n\n", inst_conf->rtmp_url);
    }

    if (inst_conf->gb28181_enable) {
        auto gb28181 = make_shared<ModuleGB28181Client>(inst_conf->gb28181_user_id, "ffmedia");
        gb28181->setProductor(inst->last_module);
        if (inst_conf->sync_opt)
            gb28181->setSynchronize(make_shared<Synchronize>(SynchronizeType(inst_conf->sync_opt - 1)));

        gb28181->setServerConfig(inst_conf->gb28181_server_id, inst_conf->gb28181_server_ip,
                                 inst_conf->gb28181_server_ip, inst_conf->gb28181_server_port);
        ret = gb28181->init();
        if (ret) {
            ff_error("Failed to init gb28181\n");
            goto FAILED;
        }
    }

    if (inst_conf->savetofile_enabled) {
        inst->file_data = fopen(inst_conf->dump_filename, "w+");
        inst->source_module->setOutputDataCallback(inst, callback_dumpFrametofile);
    }

    // clang-format off
	ff_info("\n"
             "Input Source:   %s\n"
			 "Input format:   %dx%d %s\n"
			 "Output format:  %dx%d %s\n"
			 "Encode type:    %s\n"
			 "Decoder:        %s\n"
			 "Rga:            %s\n"
			 "Encoder:        %s\n"
			 "RtspClient:     %s\n"
             "File writer:    %s\n"
			 "File:           %s\n"
			 "%s push:      %s\n",
			 inst_conf->input_source,
			 inst_conf->input_image_para.width, inst_conf->input_image_para.height, v4l2GetFmtName(inst_conf->input_image_para.v4l2Fmt),
			 inst_conf->output_image_para.width, inst_conf->output_image_para.height, v4l2GetFmtName(inst_conf->output_image_para.v4l2Fmt),
			 inst_conf->encode_type == ENCODE_TYPE_H264 ? "H264" : "H265",
			 inst_conf->dec_enabled ? "enable" : "disable",
			 inst_conf->rga_enabled ? "enable" : "disable",
			 inst_conf->enc_enabled ? "enable" : "disable",
			 inst_conf->rtsp_c_enabled ? "enable" : "disable",
			 inst_conf->file_w_enabled ? inst_conf->output_filename : "disable",
 			 inst_conf->savetofile_enabled ? inst_conf->dump_filename : "disable",
             inst_conf->push_type ? "Rtmp" : "Rtsp",
			 inst_conf->push_enabled ? to_string(inst_conf->push_port).c_str() : "disable");
    // clang-format on

    return 0;

FAILED:
    return -1;
}

static int parse_config(int argc, char** argv, DemoConfig* config)
{
    int ret;
    int i, c, option_index = 0;
    strcpy(config->input_source, argv[1]);

    /* Dealing with options  */
    while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (c) {
            case 'i':
                parse_size_parameters(optarg, &(config->input_image_para));
                break;
            case 'o':
                parse_size_parameters(optarg, &(config->output_image_para));
                break;
            case 'a':
                parse_format_parameters(optarg, &(config->input_image_para));
                break;
            case 'b':
                parse_format_parameters(optarg, &(config->output_image_para));
                break;
            case 'c':
                config->instance_count = atoi(optarg);
                break;
            case 'd':
                config->drm_display_plane_id = atoi(optarg);
                config->drmdisplay_enabled = true;
                break;
            case 'C':
                config->drm_display_conn_id = atoi(optarg);
                break;
            case 'x':
                config->x11display_enabled = true;
                break;
            case 'l':
                config->loop = true;
                break;
            case 'z':
                config->drm_display_plane_zpos = atoi(optarg);
                break;
            case 'e':
                ret = parse_encode_parameters(optarg, &(config->encode_type));
                if (!ret)
                    config->enc_enabled = true;
                break;
            case 'f':
                strcpy(config->dump_filename, optarg);
                config->savetofile_enabled = true;
                break;
            case 'p':
                config->push_port = atoi(optarg);
                config->push_enabled = true;
                break;
            case 't':
                if (strcmp(optarg, "rtmp") == 0)
                    config->push_type = 1;
                else
                    config->push_type = 0;
                break;
            case 'R':
                strcpy(config->rtmp_url, optarg);
                break;
            case 'P':
                if (strcmp(optarg, "udp") == 0)
                    config->rtsp_transport = RTSP_STREAM_TYPE_UDP;
                else if (strcmp(optarg, "tcp") == 0)
                    config->rtsp_transport = RTSP_STREAM_TYPE_TCP;
                else if (strcmp(optarg, "multicast") == 0)
                    config->rtsp_transport = RTSP_STREAM_TYPE_MULTICAST;
                else
                    config->rtsp_transport = RTSP_STREAM_TYPE_UDP;
                break;
            case 'm':
                strcpy(config->output_filename, optarg);
                config->file_w_enabled = true;
                break;
            case 's':
                if (optarg == nullptr)
                    config->sync_opt = 2;
                else {
                    if (strcmp(optarg, "video") == 0)
                        config->sync_opt = 1;
                    else if (strcmp(optarg, "abs") == 0)
                        config->sync_opt = 3;
                    else
                        config->sync_opt = 2;
                }
                break;
            case 'A':
                config->audio_enable = true;
                if (strcmp(long_options[option_index].name, "aplay") == 0) {
                    strncpy(config->aplay, optarg, sizeof(config->aplay) - 1);
                    config->aplay[sizeof(config->aplay) - 1] = '\0';
                } else if (strcmp(long_options[option_index].name, "arecord") == 0) {
                    strncpy(config->arecord, optarg, sizeof(config->arecord) - 1);
                    config->arecord[sizeof(config->arecord) - 1] = '\0';
                }
                break;
            case 'G':
                config->gb28181_enable = true;
                if (strcmp(long_options[option_index].name, "gb28181_user_id") == 0) {
                    strncpy(config->gb28181_user_id, optarg, sizeof(config->gb28181_user_id) - 1);
                    config->gb28181_user_id[sizeof(config->gb28181_user_id) - 1] = '\0';
                } else if (strcmp(long_options[option_index].name, "gb28181_server_id") == 0) {
                    strncpy(config->gb28181_server_id, optarg, sizeof(config->gb28181_server_id) - 1);
                    config->gb28181_server_id[sizeof(config->gb28181_server_id) - 1] = '\0';
                } else if (strcmp(long_options[option_index].name, "gb28181_server_ip") == 0) {
                    strncpy(config->gb28181_server_ip, optarg, sizeof(config->gb28181_server_ip) - 1);
                    config->gb28181_user_id[sizeof(config->gb28181_user_id) - 1] = '\0';
                } else if (strcmp(long_options[option_index].name, "gb28181_server_port") == 0) {
                    config->gb28181_server_port = atoi(optarg);
                }

                break;
            case 'r':
                i = atoi(optarg);
                switch (i) {
                    case 0:
                        config->rotate = RGA_ROTATE_NONE;
                        break;
                    case 1:
                        config->rotate = RGA_ROTATE_VFLIP;
                        break;
                    case 2:
                        config->rotate = RGA_ROTATE_HFLIP;
                        break;
                    case 90:
                        config->rotate = RGA_ROTATE_90;
                        break;
                    case 180:
                        config->rotate = RGA_ROTATE_180;
                        break;
                    case 270:
                        config->rotate = RGA_ROTATE_270;
                        break;
                    default:
                        ff_error("Roate(%d) is not supported\n", i);
                        return -1;
                }
                break;
#if FFMPEG_SUPPORT
            case 'F':
                config->ffmpeg_demux_enabled = true;
                if (optarg) {
                    strncpy(config->ffmpeg_informat, optarg, sizeof config->ffmpeg_informat);
                    config->ffmpeg_informat[sizeof(config->ffmpeg_informat) - 1] = '\0';
                }
                break;
#endif
            default:
                return -1;
        }
    }
    return 0;
}

int main(int argc, char** argv)
{
    int instance_count = 1;

    DemoConfig ori_config;

    if (argc < 2) {
        usage(argv);
        exit(1);
    }

    if (parse_config(argc, argv, &ori_config)) {
        usage(argv);
        exit(1);
    }

    if (ori_config.instance_count > 1)
        instance_count = ori_config.instance_count;

    common_source_module = NULL;

    DemoData* insts = new DemoData[instance_count];
    for (int i = 0; i < instance_count; i++) {
        memcpy(&((insts + i)->config), &ori_config, sizeof(DemoConfig));
        if (start_instance(insts + i, i, instance_count))
            goto EXIT;
    }

    if (common_source_module != NULL) {
        common_source_module->start();
        common_source_module->dumpPipe();
    } else {
        for (int i = 0; i < instance_count; i++) {
            insts[i].source_module->start();
            insts[i].source_module->dumpPipe();
            if (insts[i].source_audio_module) {
                insts[i].source_audio_module->start();
                insts[i].source_audio_module->dumpPipe();
            }
        }
    }

    while (mygetch() != 'q') {
        usleep(10000);
    }

EXIT:

    if (common_source_module != NULL) {
        common_source_module->dumpPipeSummary();
        common_source_module->stop();
    } else {
        for (int i = 0; i < instance_count; i++) {
            if (insts + i != NULL) {
                if (insts[i].source_module == NULL)
                    continue;

                if (insts[i].source_audio_module) {
                    insts[i].source_audio_module->dumpPipeSummary();
                    insts[i].source_audio_module->stop();
                }

                insts[i].source_module->dumpPipeSummary();
                insts[i].source_module->stop();
            }
        }
    }

    for (int i = 0; i < instance_count; i++) {
        if (insts + i != NULL) {
            if (insts[i].file_data != nullptr)
                fclose(insts[i].file_data);
        }
    }

    delete[] insts;
}
