#include "image_pipeline.h"

#include <algorithm>

namespace reference_qt_adapter {
namespace {

// 参考 Python 适配流程，将输入图统一到已验证的尺寸范围。
cv::Mat standardize_size(const cv::Mat& image16) {
    const cv::Size target_size(1536, 1152);
    if (image16.size() == target_size || image16.size() == cv::Size(2048, 1152)) {
        return image16.clone();
    }

    cv::Mat resized;
    cv::resize(image16, resized, target_size, 0.0, 0.0, cv::INTER_LINEAR);
    return resized;
}

// 按 initParas 中的窗宽窗位将 16 位灰度映射到 8 位。
cv::Mat window_to_8bit(const cv::Mat& image16, double window_width, double window_level) {
    const double width = std::max(window_width, 1.0);
    const double lower = window_level - (width * 0.5);
    const double upper = window_level + (width * 0.5);

    cv::Mat image32f;
    image16.convertTo(image32f, CV_32F);
    cv::Mat clipped;
    cv::min(cv::max(image32f, lower), upper, clipped);
    cv::Mat scaled = (clipped - lower) / std::max(upper - lower, 1e-6);
    cv::Mat result;
    scaled.convertTo(result, CV_8U, 255.0);
    return result;
}

// 复用当前验证流程中的 CLAHE + 高斯模糊增强策略。
cv::Mat enhance_8bit(const cv::Mat& image8) {
    auto clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    cv::Mat enhanced;
    clahe->apply(image8, enhanced);
    cv::Mat blurred;
    cv::GaussianBlur(enhanced, blurred, cv::Size(3, 3), 0.0);
    return blurred;
}

} // namespace

bool prepare_input_images(
    const cv::Mat& image16,
    double window_width,
    double window_level,
    PreprocessedImages& result,
    QString* error) {
    if (image16.empty()) {
        if (error != nullptr) {
            *error = "Input image is empty.";
        }
        return false;
    }
    if (image16.type() != CV_16UC1) {
        if (error != nullptr) {
            *error = "Input image must be single-channel CV_16U.";
        }
        return false;
    }

    // 这三张中间图后续既要参与推理，也要参与 sendData 输出。
    result.standardized16 = standardize_size(image16);
    result.raw8 = window_to_8bit(result.standardized16, window_width, window_level);
    result.enhanced8 = enhance_8bit(result.raw8);
    return true;
}

cv::Mat build_model_input_bgr(const cv::Mat& image8, CropInfo& crop_info) {
    // 当前参考实现采用中心裁切，保持与 Python 验证逻辑一致。
    const int crop_size = std::min(768, std::min(image8.cols, image8.rows));
    crop_info.start_x = std::max((image8.cols - crop_size) / 2, 0);
    crop_info.start_y = std::max((image8.rows - crop_size) / 2, 0);
    crop_info.crop_width = crop_size;
    crop_info.crop_height = crop_size;

    cv::Mat crop = image8(cv::Rect(crop_info.start_x, crop_info.start_y, crop_size, crop_size)).clone();
    if (crop_size != 768) {
        cv::resize(crop, crop, cv::Size(768, 768), 0.0, 0.0, cv::INTER_LINEAR);
    }

    cv::Mat crop_bgr;
    cv::cvtColor(crop, crop_bgr, cv::COLOR_GRAY2BGR);
    return crop_bgr;
}

cv::Mat restore_crop_to_full_size(const cv::Mat& seg_crop, const cv::Size& full_size, const CropInfo& crop_info) {
    cv::Mat full(full_size, CV_8UC1, cv::Scalar(0));
    cv::Mat restored = seg_crop;
    if (crop_info.crop_width != 768 || crop_info.crop_height != 768) {
        cv::resize(seg_crop, restored, cv::Size(crop_info.crop_width, crop_info.crop_height), 0.0, 0.0, cv::INTER_NEAREST);
    }

    restored.copyTo(full(cv::Rect(crop_info.start_x, crop_info.start_y, crop_info.crop_width, crop_info.crop_height)));
    return full;
}

cv::Mat build_drawing_image(const cv::Mat& enhanced8, const cv::Mat& seg255) {
    // 仅叠加外轮廓，便于客户快速观察分割结果位置。
    cv::Mat drawing;
    cv::cvtColor(enhanced8, drawing, cv::COLOR_GRAY2BGR);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(seg255, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    cv::drawContours(drawing, contours, -1, cv::Scalar(0, 255, 0), 2);
    return drawing;
}

} // namespace reference_qt_adapter
