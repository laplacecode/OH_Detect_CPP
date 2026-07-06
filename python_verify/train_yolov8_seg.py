from __future__ import annotations

import argparse
from pathlib import Path

import torch
from ultralytics import YOLO


REPO_ROOT = Path(__file__).resolve().parents[1]
VERIFICATION_ROOT = Path(__file__).resolve().parent
DEFAULT_DATA = VERIFICATION_ROOT / "test_Pit" / "data.yaml"
DEFAULT_MODEL = REPO_ROOT / "yolov8n-seg.pt"
DEFAULT_PROJECT = REPO_ROOT / "runs" / "segment"


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


def normalize_label_encoding(data_yaml_path: Path) -> None:
    dataset_root = data_yaml_path.parent
    fixed_count = 0
    cache_warning_count = 0

    for split in ("train", "val", "test"):
        labels_dir = dataset_root / split / "labels"
        if not labels_dir.exists():
            continue

        # 一些标注工具会导出 UTF-8 BOM，这里统一转成无 BOM 的 UTF-8。
        for label_path in labels_dir.glob("*.txt"):
            raw = label_path.read_bytes()
            if raw.startswith(b"\xef\xbb\xbf"):
                label_path.write_bytes(raw[3:])
                fixed_count += 1

        # 标签缓存可能让重新标注后的数据不生效，训练前顺手清理。
        cache_path = labels_dir.with_suffix(".cache")
        if cache_path.exists():
            try:
                cache_path.unlink()
            except PermissionError:
                cache_warning_count += 1
                print(
                    "Warning: failed to delete cache file, please remove it manually "
                    f"if training still uses stale cache: {cache_path}"
                )

    if fixed_count:
        print(f"Normalized {fixed_count} label file(s) from UTF-8 BOM to UTF-8.")
    if cache_warning_count == 0:
        print("Dataset cache cleanup completed.")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Train a YOLOv8 segmentation model with configurable defaults for this workspace."
    )
    parser.add_argument("--data", default=str(DEFAULT_DATA), help="Path to dataset yaml.")
    parser.add_argument("--model", default=str(DEFAULT_MODEL), help="Path to initial .pt model.")
    parser.add_argument("--imgsz", type=int, default=768, help="Training image size.")
    parser.add_argument("--epochs", type=int, default=200, help="Number of training epochs.")
    parser.add_argument("--batch", default="auto", help="Batch size or 'auto'.")
    parser.add_argument("--device", default="0", help="Training device, e.g. '0', 'cpu', '0,1'.")
    parser.add_argument("--project", default=str(DEFAULT_PROJECT), help="Output project directory.")
    parser.add_argument("--name", default="test_Pit", help="Run name.")
    parser.add_argument("--workers", type=int, default=8, help="Number of dataloader workers.")
    parser.add_argument("--patience", type=int, default=50, help="Early stopping patience.")
    parser.add_argument("--optimizer", default="auto", help="Optimizer name or 'auto'.")
    parser.add_argument("--lr0", type=float, default=0.01, help="Initial learning rate.")
    parser.add_argument("--lrf", type=float, default=0.01, help="Final learning rate factor.")
    parser.add_argument("--close-mosaic", type=int, default=10, help="Disable mosaic in final N epochs.")
    parser.add_argument("--mask-ratio", type=int, default=4, help="Segmentation mask downsample ratio.")
    parser.add_argument(
        "--overlap-mask",
        type=str2bool,
        default=True,
        help="Whether overlapping masks are merged during training.",
    )
    parser.add_argument(
        "--amp",
        type=str2bool,
        default=True,
        help="Enable mixed precision training when supported.",
    )
    parser.add_argument(
        "--cache",
        type=str2bool,
        default=False,
        help="Cache images in RAM for faster training.",
    )
    parser.add_argument(
        "--rect",
        type=str2bool,
        default=False,
        help="Use rectangular batches.",
    )
    parser.add_argument(
        "--cos-lr",
        type=str2bool,
        default=False,
        help="Use cosine LR scheduler.",
    )
    parser.add_argument(
        "--deterministic",
        type=str2bool,
        default=True,
        help="Force deterministic training behavior when possible.",
    )
    parser.add_argument("--seed", type=int, default=42, help="Random seed.")
    parser.add_argument(
        "--pretrained",
        type=str2bool,
        default=True,
        help="Use pretrained weights from the model checkpoint.",
    )
    parser.add_argument(
        "--resume",
        type=str2bool,
        default=False,
        help="Resume the latest interrupted training run.",
    )
    parser.add_argument(
        "--exist-ok",
        type=str2bool,
        default=True,
        help="Allow reuse of an existing run directory.",
    )
    parser.add_argument(
        "--val",
        type=str2bool,
        default=True,
        help="Run validation during training.",
    )
    return parser


def resolve_device(device_value: str) -> str:
    normalized = str(device_value).strip().lower()
    if normalized == "cpu":
        return "cpu"

    requested_cuda = normalized.isdigit() or "," in normalized or normalized.startswith("cuda")
    if not requested_cuda:
        return device_value

    if torch.cuda.is_available():
        return device_value

    print("CUDA is not available in the current PyTorch environment. Falling back to CPU.")
    return "cpu"


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()

    data_path = resolve_path(args.data)
    model_path = resolve_path(args.model)
    project_path = resolve_path(args.project)

    if not data_path.exists():
        raise FileNotFoundError(f"Dataset yaml not found: {data_path}")
    if not model_path.exists():
        raise FileNotFoundError(f"Initial model not found: {model_path}")

    normalize_label_encoding(data_path)

    batch_value: int | str
    if isinstance(args.batch, str) and args.batch.lower() != "auto":
        batch_value = int(args.batch)
    else:
        batch_value = args.batch

    resolved_device = resolve_device(args.device)

    train_kwargs = {
        "data": str(data_path),
        "imgsz": args.imgsz,
        "epochs": args.epochs,
        "batch": batch_value,
        "device": resolved_device,
        "project": str(project_path),
        "name": args.name,
        "workers": args.workers,
        "patience": args.patience,
        "optimizer": args.optimizer,
        "lr0": args.lr0,
        "lrf": args.lrf,
        "close_mosaic": args.close_mosaic,
        "mask_ratio": args.mask_ratio,
        "overlap_mask": args.overlap_mask,
        "amp": args.amp,
        "cache": args.cache,
        "rect": args.rect,
        "cos_lr": args.cos_lr,
        "deterministic": args.deterministic,
        "seed": args.seed,
        "pretrained": args.pretrained,
        "resume": args.resume,
        "exist_ok": args.exist_ok,
        "val": args.val,
        "task": "segment",
    }

    print("Resolved training configuration:")
    for key, value in train_kwargs.items():
        print(f"  {key}: {value}")

    model = YOLO(str(model_path))
    results = model.train(**train_kwargs)

    save_dir = getattr(getattr(results, "save_dir", None), "__str__", None)
    if callable(save_dir):
        print(f"Training completed. Outputs saved to: {results.save_dir}")
    else:
        print("Training completed.")


if __name__ == "__main__":
    main()
