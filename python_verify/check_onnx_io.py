from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
from typing import Iterable


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MODEL = REPO_ROOT / "runs" / "segment" / "test_Pit" / "weights" / "best.onnx"


def has_module(module_name: str) -> bool:
    return importlib.util.find_spec(module_name) is not None


def resolve_path(path_value: str) -> Path:
    path = Path(path_value)
    if path.is_absolute():
        return path
    return (REPO_ROOT / path).resolve()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Inspect ONNX model inputs and outputs for delivery validation."
    )
    parser.add_argument(
        "--model",
        default=str(DEFAULT_MODEL),
        help="Path to the ONNX model file.",
    )
    return parser


def format_shape(dims: Iterable[object]) -> str:
    parts: list[str] = []
    for dim in dims:
        if hasattr(dim, "dim_value") and getattr(dim, "dim_value"):
            parts.append(str(dim.dim_value))
        elif hasattr(dim, "dim_param") and getattr(dim, "dim_param"):
            parts.append(str(dim.dim_param))
        else:
            parts.append("?")
    return "(" + ", ".join(parts) + ")"


def inspect_with_onnx(model_path: Path) -> None:
    import onnx

    # 先按 ONNX 图结构读取，再用 checker 做一次基本合法性校验。
    model = onnx.load(str(model_path))
    onnx.checker.check_model(model)

    print("ONNX graph inspection:")
    print(f"  model: {model_path}")
    print(f"  ir_version: {model.ir_version}")
    print(f"  producer_name: {model.producer_name or '-'}")
    print(f"  producer_version: {model.producer_version or '-'}")

    print("  inputs:")
    for index, item in enumerate(model.graph.input):
        tensor_type = item.type.tensor_type
        elem_type = tensor_type.elem_type
        dims = tensor_type.shape.dim
        print(f"    [{index}] name={item.name} elem_type={elem_type} shape={format_shape(dims)}")

    print("  outputs:")
    for index, item in enumerate(model.graph.output):
        tensor_type = item.type.tensor_type
        elem_type = tensor_type.elem_type
        dims = tensor_type.shape.dim
        print(f"    [{index}] name={item.name} elem_type={elem_type} shape={format_shape(dims)}")


def inspect_with_onnxruntime(model_path: Path) -> None:
    import onnxruntime as ort

    # 运行时视角能看到 provider、name、shape、type 等实际消费信息。
    session = ort.InferenceSession(str(model_path), providers=["CPUExecutionProvider"])

    print("ONNX Runtime inspection:")
    print(f"  providers: {session.get_providers()}")

    print("  inputs:")
    for index, item in enumerate(session.get_inputs()):
        print(f"    [{index}] name={item.name} type={item.type} shape={item.shape}")

    print("  outputs:")
    for index, item in enumerate(session.get_outputs()):
        print(f"    [{index}] name={item.name} type={item.type} shape={item.shape}")


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()

    model_path = resolve_path(args.model)
    if not model_path.exists():
        raise FileNotFoundError(f"ONNX model not found: {model_path}")

    if not has_module("onnx"):
        raise ModuleNotFoundError(
            "Missing Python package 'onnx'. Install it first, for example: "
            "python -m pip install onnx"
        )

    inspect_with_onnx(model_path)

    if has_module("onnxruntime"):
        inspect_with_onnxruntime(model_path)
    else:
        print("ONNX Runtime inspection skipped: package 'onnxruntime' is not installed.")


if __name__ == "__main__":
    main()
