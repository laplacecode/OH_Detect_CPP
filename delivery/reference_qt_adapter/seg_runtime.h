#ifndef DELIVERY_REFERENCE_QT_ADAPTER_SEG_RUNTIME_H
#define DELIVERY_REFERENCE_QT_ADAPTER_SEG_RUNTIME_H

#include <QString>
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

#include <memory>
#include <string>
#include <vector>

namespace reference_qt_adapter {

// 标准 YOLOv8-seg 双输出运行时封装。
class SegRuntime {
public:
    SegRuntime();
    ~SegRuntime();

    bool init(const std::wstring& model_path, QString* error = nullptr);
    bool run(const cv::Mat& input_bgr, cv::Mat& seg_map, cv::Mat& seg_map255, QString* error = nullptr);
    bool is_ready() const;

private:
    // 单个候选目标在后处理阶段的中间表示。
    struct Detection {
        int class_id = 0;
        float score = 0.0f;
        cv::Rect box_xyxy;
        std::vector<float> mask_coefficients;
    };

    bool select_output_indices(
        const std::vector<std::vector<int64_t>>& output_shapes,
        int& boxes_index,
        int& proto_index,
        QString* error) const;

    bool parse_detections(
        const Ort::Value& boxes_tensor,
        const Ort::Value& proto_tensor,
        float confidence_threshold,
        std::vector<Detection>& detections,
        QString* error) const;

    std::vector<Detection> apply_nms(
        const std::vector<Detection>& detections,
        float confidence_threshold,
        float iou_threshold) const;

    bool build_segmentation_maps(
        const std::vector<Detection>& detections,
        const Ort::Value& proto_tensor,
        int input_width,
        int input_height,
        float mask_threshold,
        cv::Mat& seg_map,
        cv::Mat& seg_map255,
        QString* error) const;

    // ONNX Runtime 会话及输入输出元数据缓存。
    Ort::Env env_;
    Ort::SessionOptions session_options_;
    std::unique_ptr<Ort::Session> session_;
    std::string input_name_;
    std::vector<std::string> output_names_;
};

} // namespace reference_qt_adapter

#endif // DELIVERY_REFERENCE_QT_ADAPTER_SEG_RUNTIME_H
