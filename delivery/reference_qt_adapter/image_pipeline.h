#ifndef DELIVERY_REFERENCE_QT_ADAPTER_IMAGE_PIPELINE_H
#define DELIVERY_REFERENCE_QT_ADAPTER_IMAGE_PIPELINE_H

#include <QString>
#include <opencv2/opencv.hpp>

namespace reference_qt_adapter {

// 预处理后的中间图像，供接口层和推理层复用。
struct PreprocessedImages {
    cv::Mat standardized16;
    cv::Mat raw8;
    cv::Mat enhanced8;
};

// 记录模型输入裁切区域，便于将 768x768 结果恢复回整图。
struct CropInfo {
    int start_x = 0;
    int start_y = 0;
    int crop_width = 0;
    int crop_height = 0;
};

// 完成输入图的尺寸标准化、窗宽窗位映射和增强。
bool prepare_input_images(
    const cv::Mat& image16,
    double window_width,
    double window_level,
    PreprocessedImages& result,
    QString* error = nullptr);

// 构造模型所需的三通道 768x768 输入图。
cv::Mat build_model_input_bgr(const cv::Mat& image8, CropInfo& crop_info);
// 将裁切区域的分割结果恢复到整图尺寸。
cv::Mat restore_crop_to_full_size(const cv::Mat& seg_crop, const cv::Size& full_size, const CropInfo& crop_info);
// 生成带轮廓覆盖的调试绘制图。
cv::Mat build_drawing_image(const cv::Mat& enhanced8, const cv::Mat& seg255);

} // namespace reference_qt_adapter

#endif // DELIVERY_REFERENCE_QT_ADAPTER_IMAGE_PIPELINE_H
