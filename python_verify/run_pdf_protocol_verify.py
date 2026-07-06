from __future__ import annotations

import argparse
import json
from pathlib import Path

import cv2
import numpy as np

from pdf_protocol_adapter import PdfProtocolAdapter
from pdf_protocol_codec import (
    InitParas,
    ReceiveDataFields,
    dataclass_to_dict,
    decode_init_paras,
    decode_receive_data,
    decode_send_data,
    encode_init_paras,
    encode_receive_data,
)


REPO_ROOT = Path(__file__).resolve().parents[1]
VERIFY_ROOT = Path(__file__).resolve().parent
DEFAULT_MODEL = REPO_ROOT / "delivery" / "package" / "model" / "best.onnx"
DEFAULT_INPUT_DIR = VERIFY_ROOT / "adapter_input"
DEFAULT_OUTPUT_DIR = VERIFY_ROOT / "pdf_protocol_preview"
SUPPORTED_SUFFIXES = {".png", ".jpg", ".jpeg", ".bmp", ".tif", ".tiff"}


def resolve_path(path_value: str) -> Path:
    path = Path(path_value)
    if path.is_absolute():
        return path
    return (REPO_ROOT / path).resolve()


def find_first_image(input_dir: Path) -> Path:
    candidates = sorted(path for path in input_dir.iterdir() if path.is_file() and path.suffix.lower() in SUPPORTED_SUFFIXES)
    if not candidates:
        raise FileNotFoundError(f"No supported image found in: {input_dir}")
    return candidates[0]


def load_as_cv16u(image_path: Path, promote_8bit: bool) -> np.ndarray:
    image = cv2.imread(str(image_path), cv2.IMREAD_UNCHANGED)
    if image is None:
        raise FileNotFoundError(f"Failed to read image: {image_path}")
    if image.ndim == 3:
        image = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    if image.dtype == np.uint16:
        return image
    if image.dtype == np.uint8 and promote_8bit:
        return (image.astype(np.uint16) * 257).astype(np.uint16)
    raise ValueError("Input image must be CV_16U grayscale, or use --promote-8bit for test images.")


def str2bool(value: str) -> bool:
    lowered = value.strip().lower()
    if lowered in {"true", "1", "yes", "y", "on"}:
        return True
    if lowered in {"false", "0", "no", "n", "off"}:
        return False
    raise argparse.ArgumentTypeError(f"Invalid boolean value: {value}")


def add_bool_argument(parser: argparse.ArgumentParser, name: str, default: bool, help_text: str) -> None:
    parser.add_argument(name, type=str2bool, default=default, help=help_text)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run a PDF protocol verification flow.")
    parser.add_argument("--model", default=str(DEFAULT_MODEL), help="Path to the ONNX model.")
    parser.add_argument("--input-dir", default=str(DEFAULT_INPUT_DIR), help="Directory containing the test image.")
    parser.add_argument("--output-dir", default=str(DEFAULT_OUTPUT_DIR), help="Directory for protocol verification outputs.")
    parser.add_argument("--promote-8bit", action="store_true", help="Promote 8-bit test images to pseudo 16-bit grayscale.")
    parser.add_argument("--reserved", default="", help="Reserved field stored at initParas index 0.")
    parser.add_argument("--camera-name", default="CAM01", help="Camera name used in reciveData bytes.")
    parser.add_argument("--product-qr", default="TEST-QR-0001", help="Product QR used in reciveData bytes.")
    parser.add_argument("--tube-voltage", type=float, default=90.0, help="X-ray tube voltage.")
    parser.add_argument("--tube-current", type=float, default=2.5, help="X-ray tube current.")
    parser.add_argument("--tube-life", type=float, default=100.0, help="X-ray tube lifetime.")
    parser.add_argument("--fixture-id", type=float, default=1.0, help="Fixture id.")
    parser.add_argument("--pixel-scale", type=float, default=1.0, help="Pixel to physical scale coefficient.")
    parser.add_argument("--window-width", type=float, default=40000.0, help="Window width for 16-bit to 8-bit conversion.")
    parser.add_argument("--window-level", type=float, default=32768.0, help="Window level for 16-bit to 8-bit conversion.")
    parser.add_argument("--save-path", default=".", help="Image/CSV save path stored in initParas bytes.")
    parser.add_argument("--horizontal-oh-max-limit", type=float, default=0.0, help="Horizontal OH max limit.")
    parser.add_argument("--horizontal-oh-min-limit", type=float, default=0.0, help="Horizontal OH min limit.")
    add_bool_argument(parser, "--enable-horizontal-oh", False, "Enable horizontal OH check.")
    parser.add_argument("--vertical-oh-max-limit", type=float, default=0.0, help="Vertical OH max limit.")
    parser.add_argument("--vertical-oh-min-limit", type=float, default=0.0, help="Vertical OH min limit.")
    add_bool_argument(parser, "--enable-vertical-oh", False, "Enable vertical OH check.")
    parser.add_argument("--horizontal-positive-alignment-limit", type=float, default=0.0, help="Horizontal positive alignment limit.")
    parser.add_argument("--horizontal-negative-alignment-limit", type=float, default=0.0, help="Horizontal negative alignment limit.")
    add_bool_argument(parser, "--enable-horizontal-alignment", False, "Enable horizontal alignment check.")
    parser.add_argument("--vertical-positive-alignment-limit", type=float, default=0.0, help="Vertical positive alignment limit.")
    parser.add_argument("--vertical-negative-alignment-limit", type=float, default=0.0, help="Vertical negative alignment limit.")
    add_bool_argument(parser, "--enable-vertical-alignment", False, "Enable vertical alignment check.")
    parser.add_argument("--positive-layer-target", type=float, default=0.0, help="Positive layer target count.")
    parser.add_argument("--negative-layer-target", type=float, default=0.0, help="Negative layer target count.")
    add_bool_argument(parser, "--enable-layer-count-check", False, "Enable layer count check.")
    parser.add_argument("--product-angle-deg", type=float, default=0.0, help="Product angle in degrees.")
    parser.add_argument("--xray-angle-deg", type=float, default=0.0, help="X-ray angle in degrees.")
    add_bool_argument(parser, "--enable-external-angle", False, "Enable external angle calculation.")
    parser.add_argument("--horizontal-oh-compensation", type=float, default=0.0, help="Horizontal OH compensation.")
    parser.add_argument("--vertical-oh-compensation", type=float, default=0.0, help="Vertical OH compensation.")
    add_bool_argument(parser, "--enable-oh-compensation", False, "Enable OH compensation.")
    parser.add_argument("--horizontal-positive-alignment-compensation", type=float, default=0.0, help="Horizontal positive alignment compensation.")
    parser.add_argument("--horizontal-negative-alignment-compensation", type=float, default=0.0, help="Horizontal negative alignment compensation.")
    add_bool_argument(parser, "--enable-horizontal-alignment-compensation", False, "Enable horizontal alignment compensation.")
    parser.add_argument("--vertical-positive-alignment-compensation", type=float, default=0.0, help="Vertical positive alignment compensation.")
    parser.add_argument("--vertical-negative-alignment-compensation", type=float, default=0.0, help="Vertical negative alignment compensation.")
    add_bool_argument(parser, "--enable-vertical-alignment-compensation", False, "Enable vertical alignment compensation.")
    parser.add_argument("--left-side-oh-layer-index", type=int, default=0, help="Left-side OH layer index.")
    parser.add_argument("--right-side-oh-layer-index", type=int, default=0, help="Right-side OH layer index.")
    add_bool_argument(parser, "--enable-both-side-oh-layers", False, "Enable both-side OH layer calculation.")
    return parser


def save_json(path: Path, payload: dict) -> None:
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()

    model_path = resolve_path(args.model)
    input_dir = resolve_path(args.input_dir)
    output_dir = resolve_path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    image_path = find_first_image(input_dir)
    image16 = load_as_cv16u(image_path, args.promote_8bit)

    init_value = InitParas(
        reserved=args.reserved,
        onnx_model_path=str(model_path),
        image_and_csv_save_path=args.save_path,
        pixel_to_physical_scale=args.pixel_scale,
        window_width=args.window_width,
        window_level=args.window_level,
        horizontal_oh_max_limit=args.horizontal_oh_max_limit,
        horizontal_oh_min_limit=args.horizontal_oh_min_limit,
        enable_horizontal_oh=args.enable_horizontal_oh,
        vertical_oh_max_limit=args.vertical_oh_max_limit,
        vertical_oh_min_limit=args.vertical_oh_min_limit,
        enable_vertical_oh=args.enable_vertical_oh,
        horizontal_positive_alignment_limit=args.horizontal_positive_alignment_limit,
        horizontal_negative_alignment_limit=args.horizontal_negative_alignment_limit,
        enable_horizontal_alignment=args.enable_horizontal_alignment,
        vertical_positive_alignment_limit=args.vertical_positive_alignment_limit,
        vertical_negative_alignment_limit=args.vertical_negative_alignment_limit,
        enable_vertical_alignment=args.enable_vertical_alignment,
        positive_layer_target=args.positive_layer_target,
        negative_layer_target=args.negative_layer_target,
        enable_layer_count_check=args.enable_layer_count_check,
        product_angle_deg=args.product_angle_deg,
        xray_angle_deg=args.xray_angle_deg,
        enable_external_angle=args.enable_external_angle,
        horizontal_oh_compensation=args.horizontal_oh_compensation,
        vertical_oh_compensation=args.vertical_oh_compensation,
        enable_oh_compensation=args.enable_oh_compensation,
        horizontal_positive_alignment_compensation=args.horizontal_positive_alignment_compensation,
        horizontal_negative_alignment_compensation=args.horizontal_negative_alignment_compensation,
        enable_horizontal_alignment_compensation=args.enable_horizontal_alignment_compensation,
        vertical_positive_alignment_compensation=args.vertical_positive_alignment_compensation,
        vertical_negative_alignment_compensation=args.vertical_negative_alignment_compensation,
        enable_vertical_alignment_compensation=args.enable_vertical_alignment_compensation,
        left_side_oh_layer_index=args.left_side_oh_layer_index,
        right_side_oh_layer_index=args.right_side_oh_layer_index,
        enable_both_side_oh_layers=args.enable_both_side_oh_layers,
    )
    receive_value = ReceiveDataFields(
        camera_name=args.camera_name,
        product_qr=args.product_qr,
        tube_voltage=args.tube_voltage,
        tube_current=args.tube_current,
        tube_life=args.tube_life,
        fixture_id=args.fixture_id,
    )

    init_raw = encode_init_paras(init_value)
    receive_raw = encode_receive_data(receive_value)

    adapter = PdfProtocolAdapter()
    if not adapter.init_paras(init_raw):
        raise RuntimeError("init_paras() failed.")
    if not adapter.recive_data([image16], receive_raw):
        raise RuntimeError("recive_data() failed.")
    if not adapter.run():
        raise RuntimeError("run() failed.")
    images, send_raw = adapter.send_data()

    image_names = [
        "m_drawingImage_8bit.png",
        "m_rawImage_8bit.png",
        "m_enhancedImage_8bit.png",
        "m_segImage.png",
        "m_segImageTo255.png",
    ]
    for name, image in zip(image_names, images):
        cv2.imwrite(str(output_dir / name), image)

    (output_dir / "initParas.bin").write_bytes(init_raw)
    (output_dir / "reciveData.bin").write_bytes(receive_raw)
    (output_dir / "sendData.bin").write_bytes(send_raw)

    save_json(output_dir / "initParas_decoded.json", dataclass_to_dict(decode_init_paras(init_raw)))
    save_json(output_dir / "reciveData_decoded.json", dataclass_to_dict(decode_receive_data(receive_raw)))
    save_json(output_dir / "sendData_decoded.json", dataclass_to_dict(decode_send_data(send_raw)))

    print(f"Image: {image_path}")
    print(f"Model: {model_path}")
    print(f"Output dir: {output_dir}")
    print("Saved protocol files: initParas.bin, reciveData.bin, sendData.bin")
    print("Saved decoded files: initParas_decoded.json, reciveData_decoded.json, sendData_decoded.json")
    print("Saved images:")
    for name in image_names:
        print(f"  {output_dir / name}")


if __name__ == "__main__":
    main()
