#ifndef DELIVERY_REFERENCE_QT_ADAPTER_PROTOCOL_CODEC_H
#define DELIVERY_REFERENCE_QT_ADAPTER_PROTOCOL_CODEC_H

#include <QByteArray>
#include <QString>

namespace reference_qt_adapter {

// 与 PDF 约定的 initParas 字段顺序一一对应。
struct InitParasData {
    QString reserved;
    QString onnx_model_path;
    QString image_and_csv_save_path;
    double pixel_to_physical_scale = 1.0;
    double window_width = 40000.0;
    double window_level = 32768.0;
    double horizontal_oh_max_limit = 0.0;
    double horizontal_oh_min_limit = 0.0;
    bool enable_horizontal_oh = false;
    double vertical_oh_max_limit = 0.0;
    double vertical_oh_min_limit = 0.0;
    bool enable_vertical_oh = false;
    double horizontal_positive_alignment_limit = 0.0;
    double horizontal_negative_alignment_limit = 0.0;
    bool enable_horizontal_alignment = false;
    double vertical_positive_alignment_limit = 0.0;
    double vertical_negative_alignment_limit = 0.0;
    bool enable_vertical_alignment = false;
    double positive_layer_target = 0.0;
    double negative_layer_target = 0.0;
    bool enable_layer_count_check = false;
    double product_angle_deg = 0.0;
    double xray_angle_deg = 0.0;
    bool enable_external_angle = false;
    double horizontal_oh_compensation = 0.0;
    double vertical_oh_compensation = 0.0;
    bool enable_oh_compensation = false;
    double horizontal_positive_alignment_compensation = 0.0;
    double horizontal_negative_alignment_compensation = 0.0;
    bool enable_horizontal_alignment_compensation = false;
    double vertical_positive_alignment_compensation = 0.0;
    double vertical_negative_alignment_compensation = 0.0;
    bool enable_vertical_alignment_compensation = false;
    int left_side_oh_layer_index = 0;
    int right_side_oh_layer_index = 0;
    bool enable_both_side_oh_layers = false;
};

// 与 reciveData 固定顺序字段对应。
struct ReceiveDataData {
    QString camera_name;
    QString product_qr;
    double tube_voltage = 0.0;
    double tube_current = 0.0;
    double tube_life = 0.0;
    double fixture_id = 0.0;
};

// 与 sendData 固定顺序字段对应。
struct SendDataData {
    QString camera_name;
    QString current_time;
    QString product_qr;
    int scan_result = 2;
    double image_gray_value = 0.0;
    double tube_voltage = 0.0;
    double tube_current = 0.0;
    double tube_life = 0.0;
    double horizontal_oh_max_limit = 0.0;
    double horizontal_oh_min_limit = 0.0;
    double vertical_oh_max_limit = 0.0;
    double vertical_oh_min_limit = 0.0;
    int oh_sub_result = 2;
    double horizontal_positive_alignment_value = 0.0;
    double horizontal_negative_alignment_value = 0.0;
    double vertical_positive_alignment_value = 0.0;
    double vertical_negative_alignment_value = 0.0;
    int alignment_sub_result = 2;
    int positive_layer_count = 0;
    int negative_layer_count = 0;
    int layer_count_sub_result = 2;
    double fixture_id = 0.0;
    int final_result_flag = 2;
};

// 从 UTF-8 行文本形式的 QByteArray 中解码初始化参数。
bool decode_init_paras(const QByteArray& raw, InitParasData& result, QString* error = nullptr);
// 从 UTF-8 行文本形式的 QByteArray 中解码单次触发输入参数。
bool decode_receive_data(const QByteArray& raw, ReceiveDataData& result, QString* error = nullptr);
// 按固定字段顺序编码 sendData 输出结果。
QByteArray encode_send_data(const SendDataData& value);

} // namespace reference_qt_adapter

#endif // DELIVERY_REFERENCE_QT_ADAPTER_PROTOCOL_CODEC_H
