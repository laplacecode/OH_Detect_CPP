#include "seg_runtime.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace reference_qt_adapter {
namespace {

// 与 Python 参考实现保持一致的 sigmoid 截断范围。
float sigmoid(float x) {
    const float clipped = std::clamp(x, -50.0f, 50.0f);
    return 1.0f / (1.0f + std::exp(-clipped));
}

std::vector<int64_t> get_shape(const Ort::Value& value) {
    return value.GetTensorTypeAndShapeInfo().GetShape();
}

} // namespace

SegRuntime::SegRuntime()
    : env_(ORT_LOGGING_LEVEL_WARNING, "qt_reference_adapter") {
}

SegRuntime::~SegRuntime() = default;

bool SegRuntime::init(const std::wstring& model_path, QString* error) {
    try {
        // 这里只启用 CPU provider，避免额外引入 GPU 环境差异。
        session_options_ = Ort::SessionOptions();
        session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        session_ = std::make_unique<Ort::Session>(env_, model_path.c_str(), session_options_);

        Ort::AllocatorWithDefaultOptions allocator;
        auto input_name = session_->GetInputNameAllocated(0, allocator);
        input_name_ = input_name.get();

        output_names_.clear();
        const size_t output_count = session_->GetOutputCount();
        for (size_t index = 0; index < output_count; ++index) {
            auto output_name = session_->GetOutputNameAllocated(index, allocator);
            output_names_.push_back(output_name.get());
        }
        return true;
    } catch (const std::exception& ex) {
        if (error != nullptr) {
            *error = QString("Failed to initialize ONNX Runtime session: %1").arg(ex.what());
        }
        session_.reset();
        return false;
    }
}

bool SegRuntime::is_ready() const {
    return session_ != nullptr;
}

bool SegRuntime::select_output_indices(
    const std::vector<std::vector<int64_t>>& output_shapes,
    int& boxes_index,
    int& proto_index,
    QString* error) const {
    boxes_index = -1;
    proto_index = -1;
    for (int index = 0; index < static_cast<int>(output_shapes.size()); ++index) {
        const size_t rank = output_shapes.at(index).size();
        if (rank == 3 && boxes_index < 0) {
            boxes_index = index;
        } else if (rank == 4 && proto_index < 0) {
            proto_index = index;
        }
    }

    if (boxes_index >= 0 && proto_index >= 0) {
        return true;
    }

    if (error != nullptr) {
        *error = "Expected one 3D boxes tensor and one 4D proto tensor.";
    }
    return false;
}

bool SegRuntime::parse_detections(
    const Ort::Value& boxes_tensor,
    const Ort::Value& proto_tensor,
    float confidence_threshold,
    std::vector<Detection>& detections,
    QString* error) const {
    // 这里按标准 YOLOv8-seg 双输出格式解析 boxes/proto。
    const std::vector<int64_t> boxes_shape = get_shape(boxes_tensor);
    const std::vector<int64_t> proto_shape = get_shape(proto_tensor);
    if (boxes_shape.size() != 3 || proto_shape.size() != 4) {
        if (error != nullptr) {
            *error = "Unexpected output tensor ranks.";
        }
        return false;
    }

    const int proto_channels = static_cast<int>(proto_shape[1]);
    const bool channels_first = boxes_shape[1] < boxes_shape[2];
    const int num_channels = static_cast<int>(channels_first ? boxes_shape[1] : boxes_shape[2]);
    const int num_predictions = static_cast<int>(channels_first ? boxes_shape[2] : boxes_shape[1]);
    const int class_count = num_channels - 4 - proto_channels;
    if (class_count <= 0) {
        if (error != nullptr) {
            *error = "Failed to resolve class count from outputs.";
        }
        return false;
    }

    const float* boxes_data = boxes_tensor.GetTensorData<float>();
    detections.clear();
    detections.reserve(num_predictions);

    for (int prediction_index = 0; prediction_index < num_predictions; ++prediction_index) {
        auto at = [&](int channel) -> float {
            if (channels_first) {
                return boxes_data[(channel * num_predictions) + prediction_index];
            }
            return boxes_data[(prediction_index * num_channels) + channel];
        };

        int best_class_id = 0;
        float best_score = at(4);
        for (int class_index = 1; class_index < class_count; ++class_index) {
            const float score = at(4 + class_index);
            if (score > best_score) {
                best_score = score;
                best_class_id = class_index;
            }
        }
        if (best_score < confidence_threshold) {
            continue;
        }

        const float cx = at(0);
        const float cy = at(1);
        const float w = at(2);
        const float h = at(3);
        const int x = std::max(0, static_cast<int>(std::floor(cx - (w * 0.5f))));
        const int y = std::max(0, static_cast<int>(std::floor(cy - (h * 0.5f))));
        const int bw = std::max(1, static_cast<int>(std::ceil(w)));
        const int bh = std::max(1, static_cast<int>(std::ceil(h)));

        Detection detection;
        detection.class_id = best_class_id;
        detection.score = best_score;
        detection.box_xyxy = cv::Rect(x, y, bw, bh);
        detection.mask_coefficients.resize(proto_channels);
        for (int proto_index = 0; proto_index < proto_channels; ++proto_index) {
            detection.mask_coefficients[proto_index] = at(4 + class_count + proto_index);
        }
        detections.push_back(std::move(detection));
    }

    return true;
}

std::vector<SegRuntime::Detection> SegRuntime::apply_nms(
    const std::vector<Detection>& detections,
    float confidence_threshold,
    float iou_threshold) const {
    if (detections.empty()) {
        return {};
    }

    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    boxes.reserve(detections.size());
    scores.reserve(detections.size());
    for (const Detection& detection : detections) {
        boxes.push_back(detection.box_xyxy);
        scores.push_back(detection.score);
    }

    std::vector<int> kept_indices;
    cv::dnn::NMSBoxes(boxes, scores, confidence_threshold, iou_threshold, kept_indices);

    std::vector<Detection> filtered;
    filtered.reserve(kept_indices.size());
    for (int index : kept_indices) {
        filtered.push_back(detections.at(static_cast<size_t>(index)));
    }
    return filtered;
}

bool SegRuntime::build_segmentation_maps(
    const std::vector<Detection>& detections,
    const Ort::Value& proto_tensor,
    int input_width,
    int input_height,
    float mask_threshold,
    cv::Mat& seg_map,
    cv::Mat& seg_map255,
    QString* error) const {
    // 将 proto 与 mask 系数组合，恢复实例掩码后再汇总为类别图。
    const std::vector<int64_t> proto_shape = get_shape(proto_tensor);
    if (proto_shape.size() != 4) {
        if (error != nullptr) {
            *error = "Proto tensor rank is invalid.";
        }
        return false;
    }

    const int mask_channels = static_cast<int>(proto_shape[1]);
    const int mask_height = static_cast<int>(proto_shape[2]);
    const int mask_width = static_cast<int>(proto_shape[3]);
    const float* proto_data = proto_tensor.GetTensorData<float>();

    seg_map = cv::Mat(input_height, input_width, CV_8UC1, cv::Scalar(0));
    seg_map255 = cv::Mat(input_height, input_width, CV_8UC1, cv::Scalar(0));

    for (const Detection& detection : detections) {
        cv::Mat mask(mask_height, mask_width, CV_32FC1, cv::Scalar(0));
        for (int y = 0; y < mask_height; ++y) {
            for (int x = 0; x < mask_width; ++x) {
                float value = 0.0f;
                const int hw_index = (y * mask_width) + x;
                for (int channel = 0; channel < mask_channels; ++channel) {
                    const int proto_index = (channel * mask_height * mask_width) + hw_index;
                    value += detection.mask_coefficients.at(static_cast<size_t>(channel)) * proto_data[proto_index];
                }
                mask.at<float>(y, x) = sigmoid(value);
            }
        }

        cv::Mat resized_mask;
        cv::resize(mask, resized_mask, cv::Size(input_width, input_height), 0.0, 0.0, cv::INTER_LINEAR);
        cv::Mat binary = cv::Mat::zeros(input_height, input_width, CV_8UC1);

        const int x1 = std::clamp(detection.box_xyxy.x, 0, input_width);
        const int y1 = std::clamp(detection.box_xyxy.y, 0, input_height);
        const int x2 = std::clamp(detection.box_xyxy.x + detection.box_xyxy.width, 0, input_width);
        const int y2 = std::clamp(detection.box_xyxy.y + detection.box_xyxy.height, 0, input_height);
        if (x1 >= x2 || y1 >= y2) {
            continue;
        }

        for (int y = y1; y < y2; ++y) {
            for (int x = x1; x < x2; ++x) {
                if (resized_mask.at<float>(y, x) > mask_threshold) {
                    binary.at<uchar>(y, x) = 255;
                }
            }
        }

        const uchar class_value = static_cast<uchar>(detection.class_id + 1);
        for (int y = 0; y < input_height; ++y) {
            const uchar* binary_ptr = binary.ptr<uchar>(y);
            uchar* seg_ptr = seg_map.ptr<uchar>(y);
            uchar* seg255_ptr = seg_map255.ptr<uchar>(y);
            for (int x = 0; x < input_width; ++x) {
                if (binary_ptr[x] > 0) {
                    seg_ptr[x] = class_value;
                    seg255_ptr[x] = 255;
                }
            }
        }
    }

    return true;
}

bool SegRuntime::run(const cv::Mat& input_bgr, cv::Mat& seg_map, cv::Mat& seg_map255, QString* error) {
    if (!is_ready()) {
        if (error != nullptr) {
            *error = "ONNX Runtime session is not initialized.";
        }
        return false;
    }
    if (input_bgr.empty() || input_bgr.type() != CV_8UC3) {
        if (error != nullptr) {
            *error = "Model input must be a non-empty 8-bit 3-channel image.";
        }
        return false;
    }

    const int input_width = input_bgr.cols;
    const int input_height = input_bgr.rows;
    // OpenCV 图像按 CHW 排列写入 ONNX 输入张量。
    std::vector<float> input_tensor(static_cast<size_t>(input_width * input_height * 3), 0.0f);
    for (int y = 0; y < input_height; ++y) {
        for (int x = 0; x < input_width; ++x) {
            const cv::Vec3b pixel = input_bgr.at<cv::Vec3b>(y, x);
            const size_t base = static_cast<size_t>(y * input_width + x);
            input_tensor[base] = static_cast<float>(pixel[0]) / 255.0f;
            input_tensor[static_cast<size_t>(input_width * input_height) + base] = static_cast<float>(pixel[1]) / 255.0f;
            input_tensor[static_cast<size_t>(2 * input_width * input_height) + base] = static_cast<float>(pixel[2]) / 255.0f;
        }
    }

    std::vector<int64_t> input_shape{1, 3, input_height, input_width};
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input = Ort::Value::CreateTensor<float>(
        memory_info,
        input_tensor.data(),
        input_tensor.size(),
        input_shape.data(),
        input_shape.size());

    std::vector<const char*> output_name_ptrs;
    output_name_ptrs.reserve(output_names_.size());
    for (const std::string& name : output_names_) {
        output_name_ptrs.push_back(name.c_str());
    }
    const char* input_names[] = {input_name_.c_str()};

    std::vector<Ort::Value> outputs;
    try {
        outputs = session_->Run(
            Ort::RunOptions{nullptr},
            input_names,
            &input,
            1,
            output_name_ptrs.data(),
            output_name_ptrs.size());
    } catch (const std::exception& ex) {
        if (error != nullptr) {
            *error = QString("ONNX inference failed: %1").arg(ex.what());
        }
        return false;
    }

    std::vector<std::vector<int64_t>> output_shapes;
    output_shapes.reserve(outputs.size());
    for (const Ort::Value& output : outputs) {
        output_shapes.push_back(get_shape(output));
    }

    int boxes_index = -1;
    int proto_index = -1;
    if (!select_output_indices(output_shapes, boxes_index, proto_index, error)) {
        return false;
    }

    std::vector<Detection> detections;
    if (!parse_detections(outputs.at(static_cast<size_t>(boxes_index)), outputs.at(static_cast<size_t>(proto_index)), 0.001f, detections, error)) {
        return false;
    }
    // 阈值与当前 Python 验证流程保持一致，优先保证行为接近。
    detections = apply_nms(detections, 0.001f, 0.45f);

    return build_segmentation_maps(
        detections,
        outputs.at(static_cast<size_t>(proto_index)),
        input_width,
        input_height,
        0.5f,
        seg_map,
        seg_map255,
        error);
}

} // namespace reference_qt_adapter
