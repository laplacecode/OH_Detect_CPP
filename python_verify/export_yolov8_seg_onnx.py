from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path

from ultralytics import YOLO


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_WEIGHTS = REPO_ROOT / "runs" / "segment" / "test_Pit" / "weights" / "best.pt"


def str2bool(value: str) -> bool:
    lowered = value.lower()
    if lowered in {"1", "true", "yes", "y", "on"}:
        return True
    if lowered in {"0", "false", "no", "n", "off"}:
        return False
    raise argparse.ArgumentTypeError(f"invalid boolean value: {value}")


def resolve_path(path_value: str) -> Path:
    path = Path(path_value)
    if path.is_absolute():
        return path
    return (REPO_ROOT / path).resolve()


def has_module(module_name: str) -> bool:
    return importlib.util.find_spec(module_name) is not None


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Export a YOLOv8 segmentation checkpoint to ONNX."
    )
    parser.add_argument(
        "--weights",
        default=str(DEFAULT_WEIGHTS),
        help="Path to the trained .pt checkpoint to export.",
    )
    parser.add_argument("--imgsz", type=int, default=768, help="Export image size.")
    parser.add_argument("--opset", type=int, default=12, help="ONNX opset version.")
    parser.add_argument(
        "--dynamic",
        type=str2bool,
        default=False,
        help="Enable dynamic input shapes.",
    )
    parser.add_argument(
        "--simplify",
        type=str2bool,
        default=True,
        help="Simplify the exported ONNX graph.",
    )
    parser.add_argument(
        "--half",
        type=str2bool,
        default=False,
        help="Export FP16 model where supported.",
    )
    parser.add_argument(
        "--nms",
        type=str2bool,
        default=False,
        help="Add NMS to the export graph.",
    )
    parser.add_argument(
        "--device",
        default="cpu",
        help="Export device, usually 'cpu' or a CUDA device id.",
    )
    parser.add_argument(
        "--batch",
        type=int,
        default=1,
        help="Batch size used during export.",
    )
    parser.add_argument(
        "--workspace",
        type=float,
        default=None,
        help="Workspace size in GiB for supported exporters.",
    )
    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()

    weights_path = resolve_path(args.weights)
    if not weights_path.exists():
        raise FileNotFoundError(f"Weights file not found: {weights_path}")

    # Ultralytics 导出 ONNX 前必须能 import onnx。
    if not has_module("onnx"):
        raise ModuleNotFoundError(
            "Missing Python package 'onnx'. Install it first, for example: "
            "python -m pip install onnx"
        )

    simplify_value = args.simplify
    # onnxsim 只是图优化，不是导出必需项。
    if simplify_value and not has_module("onnxsim"):
        print("Package 'onnxsim' is not installed. Disabling simplify and exporting plain ONNX.")
        simplify_value = False

    # 这里保持导出参数显式，便于复现实验和交付记录。
    export_kwargs = {
        "format": "onnx",
        "imgsz": args.imgsz,
        "opset": args.opset,
        "dynamic": args.dynamic,
        "simplify": simplify_value,
        "half": args.half,
        "nms": args.nms,
        "device": args.device,
        "batch": args.batch,
    }
    if args.workspace is not None:
        export_kwargs["workspace"] = args.workspace

    print("Resolved export configuration:")
    print(f"  weights: {weights_path}")
    for key, value in export_kwargs.items():
        print(f"  {key}: {value}")

    model = YOLO(str(weights_path))
    output_path = model.export(**export_kwargs)
    print(f"Export completed: {output_path}")


if __name__ == "__main__":
    main()
