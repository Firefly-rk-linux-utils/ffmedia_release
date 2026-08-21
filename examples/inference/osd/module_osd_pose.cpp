#include "module_osd_pose.hpp"
#include <opencv2/imgproc.hpp>

namespace FFMedia
{

// 骨架连接定义
int skeleton[38] = {16, 14, 14, 12, 17, 15, 15, 13, 12, 13, 6, 12, 7, 13, 6, 7, 6, 8,
                    7, 9, 8, 10, 9, 11, 2, 3, 1, 2, 1, 3, 2, 4, 3, 5, 4, 6, 5, 7};


ModuleOsdPose::ModuleOsdPose(const std::string& name)
    : ModuleOsd(name)
{
}

ModuleOsdPose::~ModuleOsdPose()
{
}

void ModuleOsdPose::osd(std::shared_ptr<InferBuffer>& buffer)
{
    auto image_param = buffer->getImagePara();

    ff_debug_m("pose_targets.size=%zu, targets.size=%zu\n",
               buffer->pose_targets.size(), buffer->targets.size());

    if (buffer->getActiveData() == nullptr) {
        ff_error_m("active data is null!\n");
        return;
    }

    cv::Mat img(image_param.height, image_param.width, CV_8UC3,
                buffer->getActiveData(), image_param.hstride * 3);

    if (buffer->pose_targets.empty()) {
        ff_debug_m("pose_targets is empty, nothing to draw\n");
        return;
    }

    // 骨架连接线颜色（可以用不同颜色区分躯干/四肢）
    cv::Scalar skeleton_color(255, 128, 0);  // 橙色骨架线

    for (auto& it : buffer->pose_targets) {

        // 1. 先画骨架连接线（在关键点圆圈下层，避免遮挡）
        if (it.keypoints.size() >= 17) {     // 确保有足够的关键点
            for (int s = 0; s < 38; s += 2) {
                int idx1 = skeleton[s] - 1;  // skeleton 数组是 1-based 索引
                int idx2 = skeleton[s + 1] - 1;

                // 检查两个关键点都有效（score > 0 且坐标有效）
                if (idx1 >= 0 && idx1 < (int)it.keypoints.size() && idx2 >= 0 && idx2 < (int)it.keypoints.size() && it.keypoints[idx1].score > 0.1f && it.keypoints[idx2].score > 0.1f) {

                    cv::Point p1(it.keypoints[idx1].x, it.keypoints[idx1].y);
                    cv::Point p2(it.keypoints[idx2].x, it.keypoints[idx2].y);
                    cv::line(img, p1, p2, skeleton_color, 2, cv::LINE_AA);
                }
            }
        }

        // 2. 画检测框
        cv::rectangle(img, cv::Rect(it.x, it.y, it.width, it.height), cv::Scalar(0, 255, 0), 2);

        // 3. 画 track_id
        if (it.track_id != -1) {
            auto id = std::to_string(it.track_id);
            cv::putText(img, id, cv::Point(it.x, it.y - 5),
                        cv::FONT_HERSHEY_PLAIN, 1.6, cv::Scalar(0, 0, 255), 2);
        }

        // 4. 画关键点（在骨架线上方，覆盖连接点）
        int drawn_kpts = 0;
        for (size_t k = 0; k < it.keypoints.size(); ++k) {
            FFPoseKeypoint& key_point = it.keypoints[k];
            if (key_point.score < 0.1f)
                continue;
            ++drawn_kpts;

            // 不同部位用不同颜色
            cv::Scalar kp_color;
            if (k <= 4) {
                kp_color = cv::Scalar(0, 255, 255);  // 黄色: 头部 (0-鼻子, 1-2左右眼, 3-4左右耳)
            } else if (k <= 10) {
                kp_color = cv::Scalar(255, 0, 0);    // 蓝色: 上半身 (5-6肩, 7-8肘, 9-10腕)
            } else {
                kp_color = cv::Scalar(0, 255, 0);    // 绿色: 下半身 (11-12髋, 13-14膝, 15-16踝)
            }

            cv::circle(img, cv::Point(key_point.x, key_point.y), 5, kp_color, -1);
        }
    }
}


}  // namespace FFMedia