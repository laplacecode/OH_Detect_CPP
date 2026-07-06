#include "../package/OH_Detect_Station_Module.h"

#include "image_pipeline.h"
#include "protocol_codec.h"
#include "seg_runtime.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <vector>

namespace reference_qt_adapter {
namespace {

// 代理测量阶段使用的几何统计结果。
struct ClassGeometry {
    int component_count = 0;
    double centroid_x_offset = 0.0;
    double centroid_y_offset = 0.0;
};

// 每个适配器实例对应一份独立状态，避免把参考实现硬塞回原始头文件成员。
struct AdapterState {
    InitParasData init_paras;
    ReceiveDataData receive_fields;
    SendDataData send_fields;
    SegRuntime runtime;
    PreprocessedImages preprocessed;
    CropInfo crop_info;
    cv::Mat seg_map;
    cv::Mat seg_map255;
    bool has_init = false;
    bool has_receive = false;
    bool has_run = false;
};

// 由于交付头文件没有为参考实现预留私有成员，这里用外部状态表托管运行时状态。
std::map<const OH_Detect_Station_Module*, std::unique_ptr<AdapterState>>& state_map() {
    static std::map<const OH_Detect_Station_Module*, std::unique_ptr<AdapterState>> instance;
    return instance;
}

AdapterState& state_for(const OH_Detect_Station_Module* instance) {
    return *state_map().at(instance);
}

std::wstring to_wstring_path(const QString& path) {
    return QDir::toNativeSeparators(path).toStdWString();
}

double compute_mean_16u(const cv::Mat& image16) {
    const cv::Scalar mean_value = cv::mean(image16);
    return mean_value[0];
}

ClassGeometry compute_class_geometry(const cv::Mat& seg_map, int class_value, double scale) {
    cv::Mat mask = (seg_map == class_value);
    if (cv::countNonZero(mask) == 0) {
        return {};
    }

    cv::Mat labels;
    const int component_count = cv::connectedComponents(mask, labels);
    std::vector<cv::Point> points;
    cv::findNonZero(mask, points);
    double sum_x = 0.0;
    double sum_y = 0.0;
    for (const cv::Point& point : points) {
        sum_x += point.x;
        sum_y += point.y;
    }

    const double center_x = (seg_map.cols - 1) * 0.5;
    const double center_y = (seg_map.rows - 1) * 0.5;
    ClassGeometry result;
    result.component_count = std::max(component_count - 1, 0);
    result.centroid_x_offset = ((sum_x / points.size()) - center_x) * scale;
    result.centroid_y_offset = ((sum_y / points.size()) - center_y) * scale;
    return result;
}

int evaluate_layer_counts(const InitParasData& init_paras, int positive_count, int negative_count) {
    // 参考实现中的层数判定只复现当前仓库验证逻辑，不引入原始生产规则。
    if (!init_paras.enable_layer_count_check) {
        return 1;
    }

    const bool positive_ok = positive_count >= static_cast<int>(std::lround(init_paras.positive_layer_target));
    const bool negative_ok = negative_count >= static_cast<int>(std::lround(init_paras.negative_layer_target));
    return (positive_ok && negative_ok) ? 1 : 2;
}

int evaluate_oh_proxy(const InitParasData& init_paras, const cv::Mat& seg255) {
    // 这里的 OH 子结果是基于分割图边界的代理测量值。
    if (!init_paras.enable_horizontal_oh && !init_paras.enable_vertical_oh) {
        return 1;
    }
    if (cv::countNonZero(seg255) == 0) {
        return 2;
    }

    std::vector<cv::Point> points;
    cv::findNonZero(seg255, points);
    int min_x = seg255.cols;
    int min_y = seg255.rows;
    for (const cv::Point& point : points) {
        min_x = std::min(min_x, point.x);
        min_y = std::min(min_y, point.y);
    }

    const double left_margin = (min_x * init_paras.pixel_to_physical_scale) + init_paras.horizontal_oh_compensation;
    const double top_margin = (min_y * init_paras.pixel_to_physical_scale) + init_paras.vertical_oh_compensation;

    bool ok = true;
    if (init_paras.enable_horizontal_oh) {
        ok = ok && (init_paras.horizontal_oh_min_limit <= left_margin && left_margin <= init_paras.horizontal_oh_max_limit);
    }
    if (init_paras.enable_vertical_oh) {
        ok = ok && (init_paras.vertical_oh_min_limit <= top_margin && top_margin <= init_paras.vertical_oh_max_limit);
    }
    return ok ? 1 : 2;
}

int evaluate_alignment_proxy(
    const InitParasData& init_paras,
    const ClassGeometry& positive_geometry,
    const ClassGeometry& negative_geometry,
    double positive_horizontal,
    double negative_horizontal,
    double positive_vertical,
    double negative_vertical) {
    // 对齐度判定同样采用当前验证流程中的代理几何值。
    std::vector<bool> checks;
    if (init_paras.enable_horizontal_alignment) {
        checks.push_back(
            positive_geometry.component_count > 0 &&
            std::abs(positive_horizontal) <= init_paras.horizontal_positive_alignment_limit);
        if (init_paras.horizontal_negative_alignment_limit > 0.0) {
            checks.push_back(
                negative_geometry.component_count > 0 &&
                std::abs(negative_horizontal) <= init_paras.horizontal_negative_alignment_limit);
        }
    }
    if (init_paras.enable_vertical_alignment) {
        checks.push_back(
            positive_geometry.component_count > 0 &&
            std::abs(positive_vertical) <= init_paras.vertical_positive_alignment_limit);
        if (init_paras.vertical_negative_alignment_limit > 0.0) {
            checks.push_back(
                negative_geometry.component_count > 0 &&
                std::abs(negative_vertical) <= init_paras.vertical_negative_alignment_limit);
        }
    }

    if (checks.empty()) {
        return 1;
    }
    return std::all_of(checks.begin(), checks.end(), [](bool value) { return value; }) ? 1 : 2;
}

SendDataData build_send_fields(
    const InitParasData& init_paras,
    const ReceiveDataData& receive_fields,
    const PreprocessedImages& preprocessed,
    const cv::Mat& seg_map,
    const cv::Mat& seg255) {
    // sendData 字段直接按照当前 Python 验证链路的规则组装。
    const int scan_result = receive_fields.product_qr.isEmpty() ? 2 : 1;
    const ClassGeometry positive_geometry = compute_class_geometry(seg_map, 1, init_paras.pixel_to_physical_scale);
    const ClassGeometry negative_geometry = compute_class_geometry(seg_map, 2, init_paras.pixel_to_physical_scale);

    const double positive_horizontal = positive_geometry.centroid_x_offset + init_paras.horizontal_positive_alignment_compensation;
    const double negative_horizontal = negative_geometry.centroid_x_offset + init_paras.horizontal_negative_alignment_compensation;
    const double positive_vertical = positive_geometry.centroid_y_offset + init_paras.vertical_positive_alignment_compensation;
    const double negative_vertical = negative_geometry.centroid_y_offset + init_paras.vertical_negative_alignment_compensation;

    const int oh_sub_result = evaluate_oh_proxy(init_paras, seg255);
    const int alignment_sub_result = evaluate_alignment_proxy(
        init_paras,
        positive_geometry,
        negative_geometry,
        positive_horizontal,
        negative_horizontal,
        positive_vertical,
        negative_vertical);
    const int layer_count_sub_result = evaluate_layer_counts(
        init_paras,
        positive_geometry.component_count,
        negative_geometry.component_count);

    const int final_result = (scan_result == 1 && oh_sub_result == 1 && alignment_sub_result == 1 && layer_count_sub_result == 1) ? 1 : 2;

    SendDataData result;
    result.camera_name = receive_fields.camera_name;
    result.current_time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    result.product_qr = receive_fields.product_qr;
    result.scan_result = scan_result;
    result.image_gray_value = compute_mean_16u(preprocessed.standardized16);
    result.tube_voltage = receive_fields.tube_voltage;
    result.tube_current = receive_fields.tube_current;
    result.tube_life = receive_fields.tube_life;
    result.horizontal_oh_max_limit = init_paras.horizontal_oh_max_limit;
    result.horizontal_oh_min_limit = init_paras.horizontal_oh_min_limit;
    result.vertical_oh_max_limit = init_paras.vertical_oh_max_limit;
    result.vertical_oh_min_limit = init_paras.vertical_oh_min_limit;
    result.oh_sub_result = oh_sub_result;
    result.horizontal_positive_alignment_value = positive_horizontal;
    result.horizontal_negative_alignment_value = negative_horizontal;
    result.vertical_positive_alignment_value = positive_vertical;
    result.vertical_negative_alignment_value = negative_vertical;
    result.alignment_sub_result = alignment_sub_result;
    result.positive_layer_count = positive_geometry.component_count;
    result.negative_layer_count = negative_geometry.component_count;
    result.layer_count_sub_result = layer_count_sub_result;
    result.fixture_id = receive_fields.fixture_id;
    result.final_result_flag = final_result;
    return result;
}

} // namespace
} // namespace reference_qt_adapter

OH_Detect_Station_Module::OH_Detect_Station_Module(QObject* parent)
    : Base() {
    setParent(parent);
    reference_qt_adapter::state_map()[this] = std::make_unique<reference_qt_adapter::AdapterState>();
}

OH_Detect_Station_Module::~OH_Detect_Station_Module() {
    reference_qt_adapter::state_map().erase(this);
}

bool OH_Detect_Station_Module::initParas(QByteArray& config) {
    auto& state = reference_qt_adapter::state_for(this);
    QString error;
    // initParas 负责协议解码与 ONNX 会话初始化。
    if (!reference_qt_adapter::decode_init_paras(config, state.init_paras, &error)) {
        qWarning() << "initParas decode failed:" << error;
        return false;
    }

    QFileInfo model_info(state.init_paras.onnx_model_path);
    QString resolved_model_path = model_info.absoluteFilePath();
    if (!model_info.exists()) {
        qWarning() << "initParas model path does not exist:" << resolved_model_path;
        return false;
    }

    if (!state.runtime.init(reference_qt_adapter::to_wstring_path(resolved_model_path), &error)) {
        qWarning() << "initParas runtime init failed:" << error;
        return false;
    }

    state.has_init = true;
    state.has_receive = false;
    state.has_run = false;
    b_initFlag = true;
    this->model_path = QDir::toNativeSeparators(resolved_model_path).toStdString();
    this->saveFolder_Path = state.init_paras.image_and_csv_save_path.toStdString();
    return true;
}

bool OH_Detect_Station_Module::reciveData(std::vector<cv::Mat>& images, QByteArray& data) {
    auto& state = reference_qt_adapter::state_for(this);
    if (!state.has_init) {
        qWarning() << "reciveData called before initParas.";
        return false;
    }
    if (images.empty()) {
        qWarning() << "reciveData requires one input image.";
        return false;
    }

    QString error;
    // reciveData 只处理一次触发输入和图像预处理，不做推理。
    if (!reference_qt_adapter::decode_receive_data(data, state.receive_fields, &error)) {
        qWarning() << "reciveData decode failed:" << error;
        return false;
    }

    if (!reference_qt_adapter::prepare_input_images(
            images.front(),
            state.init_paras.window_width,
            state.init_paras.window_level,
            state.preprocessed,
            &error)) {
        qWarning() << "reciveData image preprocessing failed:" << error;
        return false;
    }

    m_rawImage_16bit = images.front().clone();
    g_roiImage_16bit = state.preprocessed.standardized16;
    m_rawImage_8bit = state.preprocessed.raw8;
    m_enhancedImage_8bit = state.preprocessed.enhanced8;
    g_roiImage_8bit = state.preprocessed.enhanced8;
    state.has_receive = true;
    state.has_run = false;
    return true;
}

bool OH_Detect_Station_Module::run() {
    auto& state = reference_qt_adapter::state_for(this);
    if (!state.has_init || !state.has_receive) {
        qWarning() << "run called before initParas/reciveData.";
        return false;
    }

    QString error;
    // run 串起裁切、推理、整图恢复和 sendData 中间结果生成。
    cv::Mat crop_bgr = reference_qt_adapter::build_model_input_bgr(state.preprocessed.enhanced8, state.crop_info);
    cv::Mat seg_crop;
    cv::Mat seg_crop255;
    if (!state.runtime.run(crop_bgr, seg_crop, seg_crop255, &error)) {
        qWarning() << "run inference failed:" << error;
        return false;
    }

    state.seg_map = reference_qt_adapter::restore_crop_to_full_size(seg_crop, state.preprocessed.enhanced8.size(), state.crop_info);
    state.seg_map255 = reference_qt_adapter::restore_crop_to_full_size(seg_crop255, state.preprocessed.enhanced8.size(), state.crop_info);
    m_segImage = state.seg_map.clone();
    m_segImageTo255 = state.seg_map255.clone();
    m_drawingImage_8bit = reference_qt_adapter::build_drawing_image(state.preprocessed.enhanced8, state.seg_map255);
    state.send_fields = reference_qt_adapter::build_send_fields(state.init_paras, state.receive_fields, state.preprocessed, state.seg_map, state.seg_map255);
    state.has_run = true;
    return true;
}

bool OH_Detect_Station_Module::sendData(std::vector<cv::Mat>& images, QByteArray& data) {
    auto& state = reference_qt_adapter::state_for(this);
    if (!state.has_run) {
        qWarning() << "sendData called before run.";
        return false;
    }

    // 输出顺序严格对齐 PDF 中约定的 5 张图。
    images.clear();
    images.push_back(m_drawingImage_8bit.clone());
    images.push_back(m_rawImage_8bit.clone());
    images.push_back(m_enhancedImage_8bit.clone());
    images.push_back(m_segImage.clone());
    images.push_back(m_segImageTo255.clone());
    data = reference_qt_adapter::encode_send_data(state.send_fields);
    return true;
}

bool OH_Detect_Station_Module::encodeResultValue(QVector<std::string> append_value, QByteArray& resultByte) {
    Q_UNUSED(append_value);
    auto& state = reference_qt_adapter::state_for(this);
    if (!state.has_run) {
        return false;
    }
    resultByte = reference_qt_adapter::encode_send_data(state.send_fields);
    return true;
}

QVector<QString> OH_Detect_Station_Module::decodeResult(QByteArray byte) {
    // 参考实现沿用逐行 UTF-8 文本字段约定，便于直接检查内容。
    const QString text = QString::fromUtf8(byte);
    return text.split('\n').toVector();
}

extern "C" {
Q_DECL_EXPORT OH_Detect_Station_Module* create() {
    return new OH_Detect_Station_Module();
}

Q_DECL_EXPORT void destory(OH_Detect_Station_Module* instance) {
    delete instance;
}
}
