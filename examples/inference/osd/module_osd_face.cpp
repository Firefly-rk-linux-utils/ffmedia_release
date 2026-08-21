#include "module_osd_face.hpp"
#include <opencv2/imgproc.hpp>

namespace FFMedia
{

ModuleOsdFace::ModuleOsdFace(const std::string& name)
    : ModuleOsd(name)
{
}

ModuleOsdFace::~ModuleOsdFace()
{
}

void ModuleOsdFace::osd(std::shared_ptr<InferBuffer>& buffer)
{
    auto image_param = buffer->getImagePara();
    cv::Mat img(image_param.height, image_param.width, CV_8UC3, buffer->getActiveData(), image_param.hstride * 3);

    for (auto& it : buffer->face_targets) {
        cv::rectangle(img, cv::Rect(it.x, it.y, it.width, it.height), cv::Scalar(0, 255, 0), 2);

        if (it.track_id != -1) {
            auto id = std::to_string(it.track_id);
            cv::putText(img, id, cv::Point(it.x, it.y - 5), cv::FONT_HERSHEY_PLAIN, 1.6, cv::Scalar(0, 0, 255), 2);
        }

        for (auto& key_point : it.key_points) {
            cv::circle(img, cv::Point(key_point.first, key_point.second), 2, cv::Scalar(255, 0, 0), 2);
        }
    }
}


}  // namespace FFMedia