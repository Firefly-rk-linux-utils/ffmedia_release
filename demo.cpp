#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <getopt.h>
#include <queue>
#include <math.h>
#include <termios.h>

#include "utils.hpp"
#include "vi/module_cam.hpp"
#include "vi/module_rtspClient.hpp"
#include "vi/module_fileReader.hpp"
#include "vp/module_rga.hpp"
#include "vp/module_mppdec.hpp"
#include "vp/module_mppenc.hpp"
#include "vo/module_mp4enm.hpp"
#include "vo/module_drmDisplay.hpp"
#include "vo/module_rtspServer.hpp"
#include "vp/module_aacdec.hpp"

struct timeval curr_time;
struct timeval start_time;

unsigned long curr, start;
using namespace std;

#define USE_COMMON_SOURCE false
ModuleMedia* common_source_module;

typedef struct _demo_data {
    ModuleCam* cam;
    ModuleMppEnc* enc;
    ModuleMppDec* dec;
    ModuleRga* rga;
    ModuleMp4Enm* enm;
    ModuleRtspClient* rtsp_c;
    ModuleDrmDisplay* drm_display;
    ModuleFileReader* file_reader;
    ModuleRtspServer* rtsp_s;
    ModuleAacDec* aac_dec;
    Synchronize* sync;

    int drm_display_plane_id;
    int drm_display_plane_zpos;
    FILE* file_data;
    char filename[256];
    char mp4mux_filename[256];
    uint64_t mp4mux_filemaxsize;
    char alsa_device[64];

    bool cam_enabled;
    bool file_r_enabled;
    bool dec_enabled;
    bool rga_enabled;
    bool drmdisplay_enabled;
    bool enc_enabled;
    bool enm_enabled;
    bool rtsp_c_enabled;
    bool rtsppush_enabled;
    bool savetofile_enabled;
    bool aplay_enable;

    ModuleMedia* last_module;
    ModuleMedia* source_module;

    char input_source[256];
    RgaRotate rotate;
    EncodeType encode_type;
    ImagePara* input_para;
    ImagePara* output_para;
    char rtsp_push_path[256];
    int rtsp_push_port;
    int sync_opt;
    int rtsp_transport;

} DemoData;

int mygetch(void)
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
        "-c, --count                  Instance count, default 1\n"
        "-d, --drmdisplay             Drm display, set display plane, set 0 to auto find plane, default disabled\n"
        "-z, --zpos                   Drm display plane zpos, default auto select\n"
        "-e, --encodetype             Encode encode, set encode type, default disabled\n"
        "-f, --file                   Enable save dec output data to file, set filename, default disabled\n"
        "-p, --port                   Enable rtsp stream, set push port, depend on encode enabled, default disabled\n"
        "--rtsp_transport             Set the rtsp transport type，default udp.\n"
        "                               e.g. --rtsp_transport tcp | --rtsp_transport multicast\n"
        "-m, --enmux                  Enable save encode data to mp4 file, depend on encode enabled, default disabled\n"
        "--enmux_filemaxsize          Set the maximum size of the saved MP4 file in bytes, default unrestricted size\n"
        "-s, --sync                   Enable synchronization module, default disabled. Enable the default audio.\n"
        "                               e.g. -s | --sync=video | --sync=abs\n"
        "-A, --aplay                  Enable play audio, default disabled. e.g. --aplay plughw:3,0\n"
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

static const char* short_options = "i:o:a:b:c:d:z:e:f:p:m:r:s::A:";

// clang-format off
static struct option long_options[] = {
    {"input", required_argument, NULL, 'i'},
    {"output", required_argument, NULL, 'o'},
    {"inputfmt", required_argument, NULL, 'a'},
    {"outputfmt", required_argument, NULL, 'b'},
    {"count", required_argument, NULL, 'c'},
    {"drmdisplay", required_argument, NULL, 'd'},
    {"zpos", required_argument, NULL, 'z'},
    {"encodetype", required_argument, NULL, 'e'},
    {"file", required_argument, NULL, 'f'},
    {"port",  required_argument, NULL, 'p'},
    {"enmux",  required_argument, NULL, 'm'},
    {"rotate", required_argument, NULL, 'r'},
    {"aplay", required_argument, NULL, 'A'},
    {"sync", optional_argument, NULL, 's' },
    {"rtsp_transport", required_argument, NULL, 'P'},
    {"enmux_filemaxsize", required_argument, NULL, 'M'},
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

void callback_savetofile(void* ctx, MediaBuffer* buffer)
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

void callback_dumpFrametofile(void* ctx, MediaBuffer* buffer)
{
    DemoData* demo = (DemoData*)ctx;
    if (buffer == NULL || buffer->getMediaBufferType() != BUFFER_TYPE_VIDEO)
        return;
    VideoBuffer* buf = static_cast<VideoBuffer*>(buffer);

    if (demo->file_data) {
        if (v4l2fmtIsCompressed(buf->getImagePara().v4l2Fmt))
            dump_normalbuffer_to_file(buf, demo->file_data);
        else
            dump_videobuffer_to_file(buf, demo->file_data);
    }
}

int start_instance(DemoData* inst_data, int inst_index, int inst_count)
{
    int ret;
    char suffix[5];
    sprintf(suffix, "%02d", inst_index);
    ImagePara source_output_para;

    ff_info("\n\n==========================================\n");
    if ((inst_data->input_para->width > 0) && (inst_data->input_para->height > 0)) {
        inst_data->input_para->hstride = inst_data->input_para->width;
        inst_data->input_para->vstride = inst_data->input_para->height;
    }

    if (strlen(inst_data->input_source) == 0) {
        ff_error("input source is not set\n");
        exit(1);
    }

    if (strncmp(inst_data->input_source, "rtsp", strlen("rtsp")) == 0) {
        ff_info("enable rtsp client\n");
        inst_data->rtsp_c_enabled = true;
    } else {
        struct stat st;
        if (stat(inst_data->input_source, &st) == -1) {
            perror(inst_data->input_source);
            exit(1);
        }

        switch (st.st_mode & S_IFMT) {
            case S_IFCHR:
                ff_info("enable v4l2 camera\n");
                inst_data->cam_enabled = true;
                break;
            case S_IFREG:
                ff_info("enable file reader\n");
                inst_data->file_r_enabled = true;
                break;
            case S_IFBLK:
            case S_IFDIR:
            case S_IFIFO:
            case S_IFLNK:
            case S_IFSOCK:
            default:
                ff_error("%s is not support\n", inst_data->input_source);
                exit(1);
                break;
        }

        if (strstr(inst_data->input_source, "mp4")) {
            inst_data->dec_enabled = true;
        } else if (strstr(inst_data->input_source, "mkv")) {
            inst_data->dec_enabled = true;
        }
    }

    if (common_source_module != NULL) {
        inst_data->source_module = common_source_module;
        goto SOURCE_CREATED;
    }

    if (inst_data->sync_opt)
        inst_data->sync = new Synchronize(SynchronizeType(inst_data->sync_opt - 1));

    if (inst_data->cam_enabled) {
        inst_data->cam = new ModuleCam(inst_data->input_source);
        if ((inst_data->input_para->width > 0) || (inst_data->input_para->height > 0)) {
            inst_data->cam->setOutputImagePara(*(inst_data->input_para));  // setOutputImage
        }
        inst_data->cam->setProductor(NULL);
        inst_data->cam->setBufferCount(1);
        ret = inst_data->cam->init();
        if (ret < 0) {
            ff_error("camera init failed\n");
            goto FAILED;
        }
        inst_data->last_module = inst_data->cam;
    }

    if (inst_data->file_r_enabled) {
        inst_data->file_reader = new ModuleFileReader(inst_data->input_source, false);
        if ((inst_data->input_para->width > 0) || (inst_data->input_para->height > 0)) {
            inst_data->file_reader->setOutputImagePara(*(inst_data->input_para));
        }
        inst_data->file_reader->setProductor(NULL);
        inst_data->file_reader->setBufferCount(20);
        ret = inst_data->file_reader->init();
        if (ret < 0) {
            ff_error("file reader init failed\n");
            goto FAILED;
        }
        inst_data->last_module = inst_data->file_reader;
        inst_data->file_reader->setSynchronize(inst_data->sync);

        if (inst_data->aplay_enable) {
            inst_data->aac_dec = new ModuleAacDec(inst_data->file_reader->audioExtraData(),
                                                  inst_data->file_reader->audioExtraDataSize(), -1);
            inst_data->aac_dec->setProductor(inst_data->last_module);
            inst_data->aac_dec->setBufferCount(1);
            inst_data->aac_dec->setAlsaDevice(inst_data->alsa_device);
            inst_data->aac_dec->setSynchronize(inst_data->sync);

            ret = inst_data->aac_dec->init();
            if (ret < 0) {
                ff_error("aac_dec init failed\n");
                goto FAILED;
            }
        }
    }

    if (inst_data->rtsp_c_enabled) {
        inst_data->rtsp_c = new ModuleRtspClient(inst_data->input_source, inst_data->rtsp_transport);
        inst_data->rtsp_c->setProductor(NULL);
        inst_data->rtsp_c->setBufferCount(20);
        ret = inst_data->rtsp_c->init();
        if (ret < 0) {
            ff_error("rtsp client init failed\n");
            goto FAILED;
        }
        inst_data->last_module = inst_data->rtsp_c;
        /* The synchronization module is not being used for the time being due to
         * a problem with the order in which the RTP package gets the timestamps
         */
        // inst_data->sync = new Synchronize(SYNCHRONIZETYPE_AUDIO);

        if (inst_data->aplay_enable) {
            inst_data->aac_dec = new ModuleAacDec(inst_data->rtsp_c->audioExtraData(),
                                                  inst_data->rtsp_c->audioExtraDataSize(), -1);
            inst_data->aac_dec->setProductor(inst_data->last_module);
            inst_data->aac_dec->setBufferCount(1);
            inst_data->aac_dec->setAlsaDevice(inst_data->alsa_device);
            inst_data->aac_dec->setSynchronize(inst_data->sync);
            ret = inst_data->aac_dec->init();
            if (ret < 0) {
                ff_error("aac_dec init failed\n");
                goto FAILED;
            }
        }
    }

    inst_data->source_module = inst_data->last_module;
#if USE_COMMON_SOURCE
    common_source_module = inst_data->last_module;
#endif

SOURCE_CREATED:

    source_output_para = inst_data->source_module->getOutputImagePara();

    inst_data->input_para = &source_output_para;
    if ((inst_data->output_para->width == 0) || (inst_data->output_para->height == 0)) {
        inst_data->output_para->width = inst_data->input_para->width;
        inst_data->output_para->height = inst_data->input_para->height;
        inst_data->output_para->hstride = inst_data->input_para->hstride;
        inst_data->output_para->vstride = inst_data->input_para->hstride;
    } else {
        inst_data->output_para->width = ALIGN(inst_data->output_para->width, 8);
        inst_data->output_para->height = ALIGN(inst_data->output_para->height, 8);
        inst_data->output_para->hstride = inst_data->output_para->width;
        inst_data->output_para->vstride = inst_data->output_para->height;
    }

    //(input_para.v4l2Fmt == V4L2_PIX_FMT_VP8) ||
    //(input_para.v4l2Fmt == V4L2_PIX_FMT_VP9))
    if ((inst_data->input_para->v4l2Fmt == V4L2_PIX_FMT_MJPEG)
        || (inst_data->input_para->v4l2Fmt == V4L2_PIX_FMT_H264)
        || (inst_data->input_para->v4l2Fmt == V4L2_PIX_FMT_HEVC)) {
        inst_data->dec_enabled = true;
    }

    inst_data->last_module = inst_data->source_module;

    // inst_data->dec_enabled = false;
    if (inst_data->dec_enabled) {
        ImagePara input_para = inst_data->last_module->getOutputImagePara();
        inst_data->dec = new ModuleMppDec(input_para);
        inst_data->dec->setProductor(inst_data->last_module);
        inst_data->dec->setBufferCount(10);
        ret = inst_data->dec->init();
        if (ret < 0) {
            ff_error("Dec init failed\n");
            goto FAILED;
        }
        inst_data->last_module = inst_data->dec;
    }

    {
        const ImagePara& para = inst_data->last_module->getOutputImagePara();
        if ((para.height != inst_data->output_para->height)
            || (para.width != inst_data->output_para->width)
            || (para.v4l2Fmt != inst_data->output_para->v4l2Fmt)
            || (inst_data->rotate != RGA_ROTATE_NONE)) {
            inst_data->rga_enabled = true;
        }

        if ((inst_data->rotate == RGA_ROTATE_90) || (inst_data->rotate == RGA_ROTATE_270)) {
            uint32_t t = inst_data->output_para->width;
            inst_data->output_para->width = inst_data->output_para->height;
            inst_data->output_para->height = t;
            t = inst_data->output_para->hstride;
            inst_data->output_para->hstride = inst_data->output_para->vstride;
            inst_data->output_para->vstride = t;
        }
    }

    if (inst_data->rga_enabled) {
        ImagePara input_para = inst_data->last_module->getOutputImagePara();
        inst_data->rga = new ModuleRga(input_para, (*inst_data->output_para), inst_data->rotate);
        inst_data->rga->setProductor(inst_data->last_module);
        inst_data->rga->setBufferCount(2);
        ret = inst_data->rga->init();
        if (ret < 0) {
            ff_error("rga init failed\n");
            goto FAILED;
        }
        inst_data->last_module = inst_data->rga;
    }

    if (inst_data->drmdisplay_enabled) {
        ImagePara input_para = inst_data->last_module->getOutputImagePara();
        inst_data->drm_display = new ModuleDrmDisplay(input_para);
        inst_data->drm_display->setPlanePara(V4L2_PIX_FMT_NV12, inst_data->drm_display_plane_id,
                                             ModuleDrmDisplay::PLANE_TYPE_OVERLAY_OR_PRIMARY, inst_data->drm_display_plane_zpos);
        // inst_data->drm_display->setPlaneSize(0, 0, 1280, 800);
        inst_data->drm_display->setBufferCount(1);
        inst_data->drm_display->setProductor(inst_data->last_module);
        inst_data->drm_display->setSynchronize(inst_data->sync);
        ret = inst_data->drm_display->init();
        if (ret < 0) {
            ff_error("drm display init failed\n");
            goto FAILED;
        }

        uint32_t t_h, t_v;
        inst_data->drm_display->getDisplayPlaneSize(&t_h, &t_v);
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

        inst_data->drm_display->setWindowSize(x, y, w, h);
    }

    if (inst_data->enc_enabled) {
        ImagePara input_para = inst_data->last_module->getOutputImagePara();
        inst_data->enc = new ModuleMppEnc(inst_data->encode_type, input_para);
        inst_data->enc->setProductor(inst_data->last_module);
        inst_data->enc->setBufferCount(8);
        ret = inst_data->enc->init();
        if (ret < 0) {
            ff_error("Enc init failed\n");
            goto FAILED;
        }
        inst_data->last_module = inst_data->enc;
    }

    if (inst_data->enm_enabled) {
        inst_data->file_data = fopen(inst_data->filename, "w+");
        ImagePara input_para = inst_data->last_module->getOutputImagePara();
        strcat(inst_data->mp4mux_filename, suffix);
        inst_data->enm = new ModuleMp4Enm(input_para, 0, 0, 30, inst_data->mp4mux_filename);
        inst_data->enm->setProductor(inst_data->last_module);
        if (inst_data->mp4mux_filemaxsize)
            inst_data->enm->setFileMaxSize(inst_data->mp4mux_filemaxsize);
        inst_data->enm->setBufferCount(0);
        ret = inst_data->enm->init();
        if (ret < 0) {
            ff_error("mp4 enm init failed\n");
            goto FAILED;
        }
    }

    if (inst_data->rtsppush_enabled) {
        sprintf(inst_data->rtsp_push_path, "/live/%d", inst_index);
        ImagePara input_para = inst_data->last_module->getOutputImagePara();
        inst_data->rtsp_s = new ModuleRtspServer(input_para, inst_data->rtsp_push_path,
                                                 inst_data->rtsp_push_port);
        inst_data->rtsp_s->setProductor(inst_data->last_module);
        inst_data->rtsp_s->setBufferCount(0);
        inst_data->rtsp_s->setSynchronize(inst_data->sync);
        ret = inst_data->rtsp_s->init();
        if (ret) {
            ff_error("rtsp server init failed\n");
            goto FAILED;
        }
        ff_info("\n Start Rtsp Stream: rtsp://LocalIpAddr:%d%s\n\n", inst_data->rtsp_push_port, inst_data->rtsp_push_path);
    }

    if (inst_data->savetofile_enabled) {
#if 0
        if (!inst_data->dec_enabled) {
            ff_error("save dec output depends on dec module enabled\n");
            goto FAILED;
        }
#endif

        strcat(inst_data->filename, suffix);
        inst_data->file_data = fopen(inst_data->filename, "w+");
#if 1
        inst_data->file_reader->setOutputDataCallback(inst_data, callback_dumpFrametofile);
#endif
#if 0
        inst_data->dec->setOutputDataCallback(inst_data, callback_dumpFrametofile);
#endif
#if 0
        inst_data->cam->setOutputDataCallback(inst_data, callback_savetofile);
#endif
#if 0
        inst_data->enc->setOutputDataCallback(inst_data, callback_savetofile);
#endif
#if 0
        inst_data->rga->setOutputDataCallback(inst_data, callback_savetofile);
#endif
#if 0
        inst_data->rtsp_c->setOutputDataCallback(inst_data, callback_savetofile);
#endif
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
             "Enmux:          %s\n"
			 "RtspClient:     %s\n"
			 "File:           %s\n"
			 "Rtsp push:      %s\n",
			 inst_data->input_source,
			 inst_data->input_para->width, inst_data->input_para->height, v4l2GetFmtName(inst_data->input_para->v4l2Fmt),
			 inst_data->output_para->width, inst_data->output_para->height, v4l2GetFmtName(inst_data->output_para->v4l2Fmt),
			 inst_data->encode_type == ENCODE_TYPE_H264 ? "H264" : "H265",
			 inst_data->dec_enabled ? "enable" : "disable",
			 inst_data->rga_enabled ? "enable" : "disable",
			 inst_data->enc_enabled ? "enable" : "disable",
			 inst_data->enm_enabled ? "enable" : "disable",
			 inst_data->rtsp_c_enabled ? "enable" : "disable",
			 inst_data->savetofile_enabled ? inst_data->filename : "disable",
			 inst_data->rtsppush_enabled ? to_string(inst_data->rtsp_push_port).c_str() : "disable");
    // clang-format on

    // inst_data->source_module->dumpPipe();
    // inst_data->source_module->start();

    return 0;

FAILED:
    return -1;
}

int main(int argc, char** argv)
{
    int ret, i, c;
    int instance_count = 1;

    DemoData demo;
    /* default parameter */
    ImagePara _input_para(0, 0, 0, 0, V4L2_PIX_FMT_MJPEG);
    ImagePara _output_para(0, 0, 0, 0, V4L2_PIX_FMT_NV12);
    memset(&demo, 0, sizeof(DemoData));
    demo.input_para = &_input_para;
    demo.output_para = &_output_para;
    demo.rotate = RGA_ROTATE_NONE;
    demo.encode_type = ENCODE_TYPE_H264;
    demo.drm_display_plane_zpos = 0xFF;

    strcpy(demo.filename, "");
    strcpy(demo.mp4mux_filename, "");
    demo.rtsp_push_port = -1;

    ff_log_init();

    if (argc < 2) {
        usage(argv);
        exit(1);
    }

    strcpy(demo.input_source, argv[1]);

    /* Dealing with options  */
    while ((c = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (c) {
            case 'i':
                parse_size_parameters(optarg, demo.input_para);
                break;
            case 'o':
                parse_size_parameters(optarg, demo.output_para);
                break;
            case 'a':
                parse_format_parameters(optarg, demo.input_para);
                break;
            case 'b':
                parse_format_parameters(optarg, demo.output_para);
                break;
            case 'c':
                instance_count = atoi(optarg);
                break;
            case 'd':
                demo.drm_display_plane_id = atoi(optarg);
                demo.drmdisplay_enabled = true;
                break;
            case 'z':
                demo.drm_display_plane_zpos = atoi(optarg);
                break;
            case 'e':
                ret = parse_encode_parameters(optarg, &demo.encode_type);
                if (!ret)
                    demo.enc_enabled = true;
                break;
            case 'f':
                strcpy(demo.filename, optarg);
                demo.savetofile_enabled = true;
                break;
            case 'p':
                demo.rtsp_push_port = atoi(optarg);
                demo.rtsppush_enabled = true;
                break;
            case 'P':
                if (strcmp(optarg, "tcp") == 0)
                    demo.rtsp_transport = 1;
                else if (strcmp(optarg, "multicast") == 0)
                    demo.rtsp_transport = 2;
                break;
            case 'm':
                strcpy(demo.mp4mux_filename, optarg);
                demo.enm_enabled = true;
                break;
            case 'M':
                demo.mp4mux_filemaxsize = strtoull(optarg, NULL, 10);
                break;
            case 's':
                if (optarg == NULL) {
                    demo.sync_opt = 1;
                } else {
                    if (strcmp(optarg, "video") == 0)
                        demo.sync_opt = 2;
                    else if (strcmp(optarg, "abs") == 0)
                        demo.sync_opt = 3;
                    else
                        demo.sync_opt = 1;
                }
                break;
            case 'A':
                strcpy(demo.alsa_device, optarg);
                demo.aplay_enable = true;
                break;
            case 'r':
                i = atoi(optarg);
                switch (i) {
                    case 0:
                        demo.rotate = RGA_ROTATE_NONE;
                        break;
                    case 1:
                        demo.rotate = RGA_ROTATE_VFLIP;
                        break;
                    case 2:
                        demo.rotate = RGA_ROTATE_HFLIP;
                        break;
                    case 90:
                        demo.rotate = RGA_ROTATE_90;
                        break;
                    case 180:
                        demo.rotate = RGA_ROTATE_180;
                        break;
                    case 270:
                        demo.rotate = RGA_ROTATE_270;
                        break;
                    default:
                        ff_error("Roate(%d) is not supported\n", i);
                        return -1;
                }
                break;
            default:
                usage(argv);
                return 0;
        }
    }

    common_source_module = NULL;
    DemoData* insts = new DemoData[instance_count];
    memset(insts, 0, sizeof(DemoData) * instance_count);
    for (int i = 0; i < instance_count; i++) {
        memcpy(insts + i, &demo, sizeof(DemoData));
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
        }
    }

    while (mygetch() != 'q') {
        usleep(10000);
    }

EXIT:
    for (int i = 0; i < instance_count; i++) {
        if (insts + i != NULL) {
            if (common_source_module != NULL) {
                common_source_module->dumpPipeSummary();
                common_source_module->stop();
                delete common_source_module;
                common_source_module = NULL;
            } else {
                if (insts[i].source_module == NULL)
                    continue;
                insts[i].source_module->dumpPipeSummary();
                insts[i].source_module->stop();
                delete insts[i].source_module;
                insts[i].source_module = NULL;
            }
            if (insts[i].file_data > 0)
                fclose(insts[i].file_data);

            if (insts[i].sync != NULL) {
                delete insts[i].sync;
                insts[i].sync = NULL;
            }
        }
    }

    delete[] insts;

    /* release memory */
}
