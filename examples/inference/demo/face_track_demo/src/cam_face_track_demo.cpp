// #include "module/vi/module_fileReader.hpp"
// #include "module/vp/module_mppdec.hpp"
#include "module/vi/module_cam.hpp"
#include "module/vp/module_mppdec.hpp"
#include "infer/module_infer_rknn2_retina_face.hpp"
#include "track/module_byte_track.hpp"
#include "osd/module_osd_face.hpp"
#include "module/vp/module_mppenc.hpp"
#include "module/vo/module_rtspServer.hpp"

#include "pan_tilt_pwm_controller.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <atomic>
#include <csignal>
#include <thread>


class FaceTrackDemo
{
public:
    FaceTrackDemo(const std::string& camera_path, const std::string& model_path);
    ~FaceTrackDemo();

private:
    int creadModuleLink();
    void osdCallback(std::shared_ptr<FFMedia::MediaBuffer> buffer);

    double calcBtmAngle(const FFMedia::FFFaceTarget& target, int image_width);
    double calcTopAngle(const FFMedia::FFFaceTarget& target, int image_height);

private:
    std::string camera_path_;
    std::string model_path_;
    std::shared_ptr<FFMedia::ModuleMedia> src_;
    std::shared_ptr<PanTiltPWMController> btm_ctrl_;
    std::shared_ptr<PanTiltPWMController> top_ctrl_;

    int track_id_;
    int track_loss_count_;
    const int max_track_loss_count_ = 120;
    double btm_angle_ = 90.0;
    double top_angle_ = 90.0;
};

FaceTrackDemo::FaceTrackDemo(const std::string& camera_path, const std::string& model_path)
    : camera_path_(camera_path), model_path_(model_path),
      track_id_(-1), track_loss_count_(0)
{
    try {
        btm_ctrl_ = std::make_shared<PanTiltPWMController>(2, 0, btm_angle_, 0, 180, 500000, 1900000);
    } catch (const std::exception& e) {
        ff_warn("Failed to init bottom pan-tilt controller: %s\n", e.what());
    }

    try {
        top_ctrl_ = std::make_shared<PanTiltPWMController>(1, 0, top_angle_, 25, 155, 1000000, 2000000);
    } catch (const std::exception& e) {
        ff_warn("Failed to init top pan-tilt controller: %s\n", e.what());
    }

    if (creadModuleLink() != 0) {
        exit(1);
    }
}

FaceTrackDemo::~FaceTrackDemo()
{
    src_->stop();
}

int FaceTrackDemo::creadModuleLink()
{
    int ret;
    FFMedia::ImagePara image_param;
    std::shared_ptr<FFMedia::ModuleMedia> last_module;
    src_ = std::make_shared<FFMedia::ModuleCam>(camera_path_);
    src_->setBufferCount(4);
    ret = src_->init();
    if (ret < 0) {
        ff_error("Failed to init file reader module\n");
        return ret;
    }
    last_module = src_;

    image_param = last_module->getOutputImagePara();
    if (FFMedia::v4l2fmtIsCompressed(image_param.v4l2Fmt)) {
        auto dec_module = std::make_shared<FFMedia::ModuleMppDec>(image_param);
        dec_module->setProductor(last_module);
        ret = dec_module->init();
        if (ret < 0) {
            ff_error("Failed to init mpp dec module\n");
            return ret;
        }
        last_module = dec_module;
    }

    image_param = last_module->getOutputImagePara();
    auto infer_module = std::make_shared<FFMedia::ModuleInferRKNN2RetinaFace>("retina_face_infer", model_path_);
    infer_module->setProductor(last_module);
    infer_module->setInputImagePara(image_param);
    ret = infer_module->init();
    if (ret < 0) {
        ff_error("Failed to init infer module\n");
        return ret;
    }

    auto track_module = std::make_shared<FFMedia::ModuleByteTrack>("face_track", FFMedia::ModuleTrackFor::FACE);
    track_module->setProductor(infer_module);
    track_module->setInputImagePara(image_param);
    ret = track_module->init();
    if (ret < 0) {
        ff_error("Failed to init track module\n");
        return ret;
    }

    image_param = track_module->getOutputImagePara();
    auto osd_module = std::make_shared<FFMedia::ModuleOsdFace>("face_osd");
    osd_module->setProductor(track_module);
    osd_module->setInputImagePara(image_param);
    ret = osd_module->init();
    if (ret < 0) {
        ff_error("Failed to init osd module\n");
        return ret;
    }

    auto enc_module = std::make_shared<FFMedia::ModuleMppEnc>(FFMedia::EncodeType::ENCODE_TYPE_H265, image_param);
    enc_module->setProductor(osd_module);
    ret = enc_module->init();
    if (ret < 0) {
        ff_error("Failed to init enc module\n");
        return ret;
    }

    image_param = enc_module->getOutputImagePara();
    auto rtsp_server = std::make_shared<FFMedia::ModuleRtspServer>(image_param, "/live/0", 8554);
    rtsp_server->setProductor(enc_module);
    ret = rtsp_server->init();
    if (ret < 0) {
        ff_error("Failed to init rtsp server module\n");
        return ret;
    }

    osd_module->setMediaBufferProduceHooker(std::bind(&FaceTrackDemo::osdCallback, this, std::placeholders::_3));
    src_->start();
    return 0;
}

static int selectTrackId(const std::vector<FFMedia::FFFaceTarget>& tracks, int image_width)
{
    int center_left = image_width / 3;
    int center_right = image_width * 2 / 3;
    int track_index = -1;
    int max_area = -1;
    for (size_t i = 0; i < tracks.size(); i++) {
        int center_x = tracks[i].x + tracks[i].width / 2;
        if (center_x >= center_left && center_x <= center_right) {
            int area = tracks[i].width * tracks[i].height;
            if (area > max_area) {
                max_area = area;
                track_index = static_cast<int>(i);
            }
        }
    }
    return track_index;
}

static int findTrack(const std::vector<FFMedia::FFFaceTarget>& tracks, int track_id)
{
    for (size_t i = 0; i < tracks.size(); i++) {
        if (tracks[i].track_id == track_id)
            return i;
    }
    return -1;
}

void FaceTrackDemo::osdCallback(std::shared_ptr<FFMedia::MediaBuffer> buffer)
{
    auto infer_buffer = std::dynamic_pointer_cast<FFMedia::InferBuffer>(buffer);
    if (infer_buffer == nullptr) {
        return;
    }

    auto image_param = infer_buffer->getImagePara();
    int index = -1;
    // 选择目标
    if (track_id_ == -1) {
        index = selectTrackId(infer_buffer->face_targets, image_param.width);
        if (index >= 0)
            track_id_ = infer_buffer->face_targets[index].track_id;
    } else {
        index = findTrack(infer_buffer->face_targets, track_id_);
        if (index == -1) {
            track_loss_count_++;
            // 目标丢失超过120帧，重新选择目标
            if (track_loss_count_ > max_track_loss_count_) {
                track_id_ = -1;
                track_loss_count_ = 0;
            }
        } else {
            track_loss_count_ = 0;
        }
    }

    // 更新云台角度
    if (index >= 0) {
        auto& face = infer_buffer->face_targets[index];
        auto next_btm_angle = calcBtmAngle(face, image_param.width);
        auto next_top_angle = calcTopAngle(face, image_param.height);
        if (next_btm_angle != btm_angle_ || next_top_angle != top_angle_) {
            ff_info("target: %d, btm_angle: %.2f, top_angle: %.2f\n", face.track_id, next_btm_angle, next_top_angle);
            if (btm_ctrl_) {
                btm_ctrl_->setAngle(next_btm_angle);
                btm_angle_ = btm_ctrl_->getAngle();
            }

            if (top_ctrl_) {
                top_ctrl_->setAngle(next_top_angle);
                top_angle_ = top_ctrl_->getAngle();
            }
        }
    }

    // cv::Mat img(image_param.height, image_param.width, CV_8UC3, infer_buffer->getActiveData(), image_param.hstride * 3);
    // cv::imshow("face track demo", img);
    // cv::waitKey(1);
}


double FaceTrackDemo::calcBtmAngle(const FFMedia::FFFaceTarget& target, int image_width)
{
    static auto last_time = std::chrono::steady_clock::now();
    static double kp = 5;
    static double kd = 0.5;
    static double offset_dead_block = 0.02;

    static double prev_offset = 0;

    double next_btm_angle, target_x, offset_x;

    target_x = static_cast<double>(target.x + target.width / 2.0);
    offset_x = static_cast<double>((target_x / image_width - 0.5) * 2);

    if (std::abs(offset_x) < offset_dead_block)
        offset_x = 0;

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> dt = now - last_time;
    if (dt.count() <= 0.01) {
        dt = std::chrono::duration<double>(0.04);
        prev_offset = offset_x;
    }
    last_time = now;

    double derivation = (offset_x - prev_offset) / dt.count();
    prev_offset = offset_x;

    next_btm_angle = btm_angle_ - (offset_x * kp + derivation * kd);
    return next_btm_angle;
}

double FaceTrackDemo::calcTopAngle(const FFMedia::FFFaceTarget& target, int image_height)
{
    static auto last_time = std::chrono::steady_clock::now();
    static double kp = 3;
    static double kd = 0.3;
    static double offset_dead_block = 0.02;

    static double prev_offset = 0;
    double next_top_angle, target_y, offset_y;

    target_y = static_cast<double>(target.y + target.height / 2.0);
    offset_y = static_cast<double>((target_y / image_height - 0.5) * 2);
    if (std::abs(offset_y) < offset_dead_block)
        offset_y = 0;

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> dt = now - last_time;
    if (dt.count() <= 0.01) {
        dt = std::chrono::duration<double>(0.04);
        prev_offset = offset_y;
    }
    last_time = now;

    double derivation = (offset_y - prev_offset) / dt.count();
    prev_offset = offset_y;

    next_top_angle = top_angle_ - (offset_y * kp + derivation * kd);
    return next_top_angle;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        ff_error("The number of parameters is incorrect\n");
        ff_error("\nUsage: %s /dev/video11 ./model/retina_face.rknn\n", argv[0]);
        return -1;
    }

    auto face_track = std::make_shared<FaceTrackDemo>(argv[1], argv[2]);

    static volatile sig_atomic_t wait_flag = 1;
    /* Set the exit signal processing function */
    signal(SIGINT, [](int) {
        ff_info(" SIGINT: exit\n");
        signal(SIGINT, SIG_IGN);
        wait_flag = 0;
    });

    signal(SIGTERM, [](int) {
        ff_info(" SIGTERM: exit\n");
        signal(SIGTERM, SIG_IGN);
        wait_flag = 0;
    });

    while (wait_flag) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
