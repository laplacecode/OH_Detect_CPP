from __future__ import annotations

from dataclasses import asdict, dataclass, fields
from typing import Iterable, get_type_hints


# PDF 只明确了 UTF-8 和字段顺序，没有定义更底层的序列化格式。
# 这里用“逐行文本字段”作为内部验证约定，便于直接查看和比对字节内容。
LINE_SEPARATOR = "\n"


def _parse_bool(value: str) -> bool:
    lowered = value.strip().lower()
    if lowered in {"true", "1", "yes", "y", "on"}:
        return True
    if lowered in {"false", "0", "no", "n", "off"}:
        return False
    raise ValueError(f"Invalid bool value: {value}")


def _format_bool(value: bool) -> str:
    return "true" if value else "false"


def _encode_lines(values: Iterable[str]) -> bytes:
    return LINE_SEPARATOR.join(values).encode("utf-8")


def _decode_lines(raw: bytes) -> list[str]:
    text = raw.decode("utf-8")
    return text.splitlines()


def _dataclass_from_lines(cls: type, raw: bytes):
    tokens = _decode_lines(raw)
    field_defs = fields(cls)
    type_hints = get_type_hints(cls)
    if len(tokens) != len(field_defs):
        raise ValueError(f"Expected {len(field_defs)} fields, got {len(tokens)}")

    values = {}
    for field_def, token in zip(field_defs, tokens):
        field_type = type_hints.get(field_def.name, str)
        if field_type is bool:
            values[field_def.name] = _parse_bool(token)
        elif field_type is int:
            values[field_def.name] = int(token)
        elif field_type is float:
            values[field_def.name] = float(token)
        else:
            values[field_def.name] = token
    return cls(**values)


def _dataclass_to_lines(instance) -> list[str]:
    result: list[str] = []
    for field_def in fields(instance):
        value = getattr(instance, field_def.name)
        if isinstance(value, bool):
            result.append(_format_bool(value))
        else:
            result.append(str(value))
    return result


@dataclass
class InitParas:
    reserved: str = ""
    onnx_model_path: str = ""
    image_and_csv_save_path: str = ""
    pixel_to_physical_scale: float = 1.0
    window_width: float = 40000.0
    window_level: float = 32768.0
    horizontal_oh_max_limit: float = 0.0
    horizontal_oh_min_limit: float = 0.0
    enable_horizontal_oh: bool = False
    vertical_oh_max_limit: float = 0.0
    vertical_oh_min_limit: float = 0.0
    enable_vertical_oh: bool = False
    horizontal_positive_alignment_limit: float = 0.0
    horizontal_negative_alignment_limit: float = 0.0
    enable_horizontal_alignment: bool = False
    vertical_positive_alignment_limit: float = 0.0
    vertical_negative_alignment_limit: float = 0.0
    enable_vertical_alignment: bool = False
    positive_layer_target: float = 0.0
    negative_layer_target: float = 0.0
    enable_layer_count_check: bool = False
    product_angle_deg: float = 0.0
    xray_angle_deg: float = 0.0
    enable_external_angle: bool = False
    horizontal_oh_compensation: float = 0.0
    vertical_oh_compensation: float = 0.0
    enable_oh_compensation: bool = False
    horizontal_positive_alignment_compensation: float = 0.0
    horizontal_negative_alignment_compensation: float = 0.0
    enable_horizontal_alignment_compensation: bool = False
    vertical_positive_alignment_compensation: float = 0.0
    vertical_negative_alignment_compensation: float = 0.0
    enable_vertical_alignment_compensation: bool = False
    left_side_oh_layer_index: int = 0
    right_side_oh_layer_index: int = 0
    enable_both_side_oh_layers: bool = False


@dataclass
class ReceiveDataFields:
    camera_name: str = ""
    product_qr: str = ""
    tube_voltage: float = 0.0
    tube_current: float = 0.0
    tube_life: float = 0.0
    fixture_id: float = 0.0


@dataclass
class SendDataFields:
    camera_name: str = ""
    current_time: str = ""
    product_qr: str = ""
    scan_result: int = 2
    image_gray_value: float = 0.0
    tube_voltage: float = 0.0
    tube_current: float = 0.0
    tube_life: float = 0.0
    horizontal_oh_max_limit: float = 0.0
    horizontal_oh_min_limit: float = 0.0
    vertical_oh_max_limit: float = 0.0
    vertical_oh_min_limit: float = 0.0
    oh_sub_result: int = 2
    horizontal_positive_alignment_value: float = 0.0
    horizontal_negative_alignment_value: float = 0.0
    vertical_positive_alignment_value: float = 0.0
    vertical_negative_alignment_value: float = 0.0
    alignment_sub_result: int = 2
    positive_layer_count: int = 0
    negative_layer_count: int = 0
    layer_count_sub_result: int = 2
    fixture_id: float = 0.0
    final_result_flag: int = 2


def encode_init_paras(value: InitParas) -> bytes:
    return _encode_lines(_dataclass_to_lines(value))


def decode_init_paras(raw: bytes) -> InitParas:
    return _dataclass_from_lines(InitParas, raw)


def encode_receive_data(value: ReceiveDataFields) -> bytes:
    return _encode_lines(_dataclass_to_lines(value))


def decode_receive_data(raw: bytes) -> ReceiveDataFields:
    return _dataclass_from_lines(ReceiveDataFields, raw)


def encode_send_data(value: SendDataFields) -> bytes:
    return _encode_lines(_dataclass_to_lines(value))


def decode_send_data(raw: bytes) -> SendDataFields:
    return _dataclass_from_lines(SendDataFields, raw)


def dataclass_to_dict(value) -> dict:
    return asdict(value)
