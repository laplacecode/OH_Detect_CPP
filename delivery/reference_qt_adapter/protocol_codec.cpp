#include "protocol_codec.h"

#include <QStringList>

namespace reference_qt_adapter {
namespace {

// 当前参考实现沿用 Python 验证链路中的“逐行 UTF-8 文本字段”约定。
QStringList decode_lines(const QByteArray& raw) {
    QString text = QString::fromUtf8(raw);
    QStringList lines = text.split('\n');
    for (QString& line : lines) {
        if (line.endsWith('\r')) {
            line.chop(1);
        }
    }
    return lines;
}

// 统一解析协议中的布尔字符串表示。
bool parse_bool_token(const QString& token, bool& result) {
    const QString lowered = token.trimmed().toLower();
    if (lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "y" || lowered == "on") {
        result = true;
        return true;
    }
    if (lowered == "false" || lowered == "0" || lowered == "no" || lowered == "n" || lowered == "off") {
        result = false;
        return true;
    }
    return false;
}

bool parse_int_token(const QString& token, int& result) {
    bool ok = false;
    const int value = token.trimmed().toInt(&ok);
    if (ok) {
        result = value;
    }
    return ok;
}

bool parse_double_token(const QString& token, double& result) {
    bool ok = false;
    const double value = token.trimmed().toDouble(&ok);
    if (ok) {
        result = value;
    }
    return ok;
}

void append_bool(QStringList& lines, bool value) {
    lines.push_back(value ? "true" : "false");
}

bool expect_count(const QStringList& tokens, int expected, QString* error) {
    if (tokens.size() == expected) {
        return true;
    }
    if (error != nullptr) {
        *error = QString("Expected %1 fields, got %2.").arg(expected).arg(tokens.size());
    }
    return false;
}

template <typename T>
bool parse_with(bool (*fn)(const QString&, T&), const QStringList& tokens, int index, T& target, QString* error, const char* field_name) {
    if (fn(tokens.at(index), target)) {
        return true;
    }
    if (error != nullptr) {
        *error = QString("Failed to parse field '%1' at index %2.").arg(field_name).arg(index);
    }
    return false;
}

} // namespace

bool decode_init_paras(const QByteArray& raw, InitParasData& result, QString* error) {
    // 字段顺序严格与 PDF 协议和 Python 验证实现保持一致。
    const QStringList tokens = decode_lines(raw);
    if (!expect_count(tokens, 36, error)) {
        return false;
    }

    result.reserved = tokens.at(0);
    result.onnx_model_path = tokens.at(1);
    result.image_and_csv_save_path = tokens.at(2);

    return parse_with(parse_double_token, tokens, 3, result.pixel_to_physical_scale, error, "pixel_to_physical_scale")
        && parse_with(parse_double_token, tokens, 4, result.window_width, error, "window_width")
        && parse_with(parse_double_token, tokens, 5, result.window_level, error, "window_level")
        && parse_with(parse_double_token, tokens, 6, result.horizontal_oh_max_limit, error, "horizontal_oh_max_limit")
        && parse_with(parse_double_token, tokens, 7, result.horizontal_oh_min_limit, error, "horizontal_oh_min_limit")
        && parse_with(parse_bool_token, tokens, 8, result.enable_horizontal_oh, error, "enable_horizontal_oh")
        && parse_with(parse_double_token, tokens, 9, result.vertical_oh_max_limit, error, "vertical_oh_max_limit")
        && parse_with(parse_double_token, tokens, 10, result.vertical_oh_min_limit, error, "vertical_oh_min_limit")
        && parse_with(parse_bool_token, tokens, 11, result.enable_vertical_oh, error, "enable_vertical_oh")
        && parse_with(parse_double_token, tokens, 12, result.horizontal_positive_alignment_limit, error, "horizontal_positive_alignment_limit")
        && parse_with(parse_double_token, tokens, 13, result.horizontal_negative_alignment_limit, error, "horizontal_negative_alignment_limit")
        && parse_with(parse_bool_token, tokens, 14, result.enable_horizontal_alignment, error, "enable_horizontal_alignment")
        && parse_with(parse_double_token, tokens, 15, result.vertical_positive_alignment_limit, error, "vertical_positive_alignment_limit")
        && parse_with(parse_double_token, tokens, 16, result.vertical_negative_alignment_limit, error, "vertical_negative_alignment_limit")
        && parse_with(parse_bool_token, tokens, 17, result.enable_vertical_alignment, error, "enable_vertical_alignment")
        && parse_with(parse_double_token, tokens, 18, result.positive_layer_target, error, "positive_layer_target")
        && parse_with(parse_double_token, tokens, 19, result.negative_layer_target, error, "negative_layer_target")
        && parse_with(parse_bool_token, tokens, 20, result.enable_layer_count_check, error, "enable_layer_count_check")
        && parse_with(parse_double_token, tokens, 21, result.product_angle_deg, error, "product_angle_deg")
        && parse_with(parse_double_token, tokens, 22, result.xray_angle_deg, error, "xray_angle_deg")
        && parse_with(parse_bool_token, tokens, 23, result.enable_external_angle, error, "enable_external_angle")
        && parse_with(parse_double_token, tokens, 24, result.horizontal_oh_compensation, error, "horizontal_oh_compensation")
        && parse_with(parse_double_token, tokens, 25, result.vertical_oh_compensation, error, "vertical_oh_compensation")
        && parse_with(parse_bool_token, tokens, 26, result.enable_oh_compensation, error, "enable_oh_compensation")
        && parse_with(parse_double_token, tokens, 27, result.horizontal_positive_alignment_compensation, error, "horizontal_positive_alignment_compensation")
        && parse_with(parse_double_token, tokens, 28, result.horizontal_negative_alignment_compensation, error, "horizontal_negative_alignment_compensation")
        && parse_with(parse_bool_token, tokens, 29, result.enable_horizontal_alignment_compensation, error, "enable_horizontal_alignment_compensation")
        && parse_with(parse_double_token, tokens, 30, result.vertical_positive_alignment_compensation, error, "vertical_positive_alignment_compensation")
        && parse_with(parse_double_token, tokens, 31, result.vertical_negative_alignment_compensation, error, "vertical_negative_alignment_compensation")
        && parse_with(parse_bool_token, tokens, 32, result.enable_vertical_alignment_compensation, error, "enable_vertical_alignment_compensation")
        && parse_with(parse_int_token, tokens, 33, result.left_side_oh_layer_index, error, "left_side_oh_layer_index")
        && parse_with(parse_int_token, tokens, 34, result.right_side_oh_layer_index, error, "right_side_oh_layer_index")
        && parse_with(parse_bool_token, tokens, 35, result.enable_both_side_oh_layers, error, "enable_both_side_oh_layers");
}

bool decode_receive_data(const QByteArray& raw, ReceiveDataData& result, QString* error) {
    const QStringList tokens = decode_lines(raw);
    if (!expect_count(tokens, 6, error)) {
        return false;
    }

    result.camera_name = tokens.at(0);
    result.product_qr = tokens.at(1);

    return parse_with(parse_double_token, tokens, 2, result.tube_voltage, error, "tube_voltage")
        && parse_with(parse_double_token, tokens, 3, result.tube_current, error, "tube_current")
        && parse_with(parse_double_token, tokens, 4, result.tube_life, error, "tube_life")
        && parse_with(parse_double_token, tokens, 5, result.fixture_id, error, "fixture_id");
}

QByteArray encode_send_data(const SendDataData& value) {
    // 输出顺序必须与 sendData 协议字段顺序一致，不能重排。
    QStringList lines;
    lines << value.camera_name
          << value.current_time
          << value.product_qr
          << QString::number(value.scan_result)
          << QString::number(value.image_gray_value, 'g', 16)
          << QString::number(value.tube_voltage, 'g', 16)
          << QString::number(value.tube_current, 'g', 16)
          << QString::number(value.tube_life, 'g', 16)
          << QString::number(value.horizontal_oh_max_limit, 'g', 16)
          << QString::number(value.horizontal_oh_min_limit, 'g', 16)
          << QString::number(value.vertical_oh_max_limit, 'g', 16)
          << QString::number(value.vertical_oh_min_limit, 'g', 16)
          << QString::number(value.oh_sub_result)
          << QString::number(value.horizontal_positive_alignment_value, 'g', 16)
          << QString::number(value.horizontal_negative_alignment_value, 'g', 16)
          << QString::number(value.vertical_positive_alignment_value, 'g', 16)
          << QString::number(value.vertical_negative_alignment_value, 'g', 16)
          << QString::number(value.alignment_sub_result)
          << QString::number(value.positive_layer_count)
          << QString::number(value.negative_layer_count)
          << QString::number(value.layer_count_sub_result)
          << QString::number(value.fixture_id, 'g', 16)
          << QString::number(value.final_result_flag);
    return lines.join('\n').toUtf8();
}

} // namespace reference_qt_adapter
